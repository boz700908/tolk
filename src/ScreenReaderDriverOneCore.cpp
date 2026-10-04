/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverOneCore.cpp
 *  Description:    Driver for the Windows OneCore speech engine
 *                  (Windows.Media.SpeechSynthesis).
 *  Copyright:      (c) 2024, Tolk contributors
 *  License:        LGPLv3
 */
#include "ScreenReaderDriverOneCore.h"
#include "TolkDebug.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Media.Playback.h>
#include <winrt/Windows.Media.SpeechSynthesis.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/base.h>

using namespace winrt::Windows::Media::SpeechSynthesis;
using namespace winrt::Windows::Media::Playback;
using namespace winrt::Windows::Media::Core;

// All WinRT objects live on a dedicated MTA thread. Tolk may be called from an
// STA (a UI thread) where the synthesizer could not be used directly, and the
// media player has to stay on one consistent apartment for its whole life.
struct ScreenReaderDriverOneCore::Impl {
  std::thread worker;
  std::mutex lock;
  std::condition_variable jobsReady;
  std::condition_variable readyChanged;
  std::deque<std::function<void()> > jobs;
  bool started;
  bool ready;
  bool supported;
  std::atomic<bool> quitting;
  std::atomic<bool> speaking;
  SpeechSynthesizer synth{nullptr};
  MediaPlayer player{nullptr};
  MediaPlaybackSession::PlaybackStateChanged_revoker stateChanged;

  Impl() : started(false), ready(false), supported(false), quitting(false), speaking(false) {}
  ~Impl() { Stop(); }

  bool Start() {
    std::unique_lock<std::mutex> guard(lock);
    if (!started) {
      started = true;
      worker = std::thread([this]() { Run(); });
      readyChanged.wait_for(guard, std::chrono::seconds(10), [this]() { return ready; });
    }
    return ready && supported;
  }

  void Stop() {
    {
      std::lock_guard<std::mutex> guard(lock);
      if (!started) return;
      quitting.store(true);
    }
    jobsReady.notify_all();
    if (worker.joinable()) worker.join();
    std::lock_guard<std::mutex> guard(lock);
    started = false;
    ready = false;
    supported = false;
  }

  bool Invoke(const std::function<void()> &job) {
    {
      std::lock_guard<std::mutex> guard(lock);
      if (!ready || !supported) return false;
      jobs.push_back(job);
    }
    jobsReady.notify_one();
    return true;
  }

  void Run() {
    bool apartment = false;
    bool ok = false;
    try {
      winrt::init_apartment(winrt::apartment_type::multi_threaded);
      apartment = true;
      if (winrt::Windows::Foundation::Metadata::ApiInformation::IsTypePresent(
            L"Windows.Media.SpeechSynthesis.SpeechSynthesizer") &&
          winrt::Windows::Foundation::Metadata::ApiInformation::IsTypePresent(
            L"Windows.Media.Playback.MediaPlayer")) {
        synth = SpeechSynthesizer();
        synth.Options().AppendedSilence(SpeechAppendedSilence::Min);
        synth.Options().PunctuationSilence(SpeechPunctuationSilence::Min);
        player = MediaPlayer();
        stateChanged = player.PlaybackSession().PlaybackStateChanged(winrt::auto_revoke,
          [this](const MediaPlaybackSession &session, const auto &) {
            speaking.store(session.PlaybackState() == MediaPlaybackState::Playing);
          });
        ok = true;
      }
    }
    catch (...) {
      ok = false;
    }
    {
      std::lock_guard<std::mutex> guard(lock);
      supported = ok;
      ready = true;
    }
    readyChanged.notify_all();
    if (ok) {
      for (;;) {
        std::function<void()> job;
        {
          std::unique_lock<std::mutex> guard(lock);
          jobsReady.wait(guard, [this]() { return quitting.load() || !jobs.empty(); });
          if (quitting.load()) break;
          job = std::move(jobs.front());
          jobs.pop_front();
        }
        job();
      }
      stateChanged.revoke();
      if (player) {
        player.Pause();
        player.Source(nullptr);
      }
      player = nullptr;
      synth = nullptr;
    }
    if (apartment) winrt::uninit_apartment();
  }

  void StopPlayback() {
    if (player) {
      player.Pause();
      player.Source(nullptr);
    }
    speaking.store(false);
  }

  void Speak(const std::wstring &text, bool interrupt) {
    try {
      if (interrupt) StopPlayback();
      const auto stream = synth.SynthesizeTextToStreamAsync(winrt::hstring(text)).get();
      const auto source = MediaSource::CreateFromStream(stream, stream.ContentType());
      player.Source(source);
      player.Play();
      speaking.store(true);
    }
    catch (const winrt::hresult_error &e) {
      TOLK_LOG_WARN("OneCore: speak failed (hr=0x%08X)", static_cast<unsigned int>(e.code().value));
    }
    catch (...) {
      TOLK_LOG_WARN("OneCore: speak failed");
    }
  }
};

ScreenReaderDriverOneCore::ScreenReaderDriverOneCore() :
  ScreenReaderDriver(L"OneCore", true, false),
  impl(new Impl()),
  disabled(false)
{
  TOLK_LOG_INFO("OneCore: Initializing driver");
}

ScreenReaderDriverOneCore::~ScreenReaderDriverOneCore() {
  TOLK_LOG_INFO("OneCore: Finalizing driver");
  delete impl;
  impl = nullptr;
}

bool ScreenReaderDriverOneCore::Speak(const wchar_t *str, bool interrupt) {
  if (!impl) return false;
  Impl *worker = impl;
  const std::wstring text(str ? str : L"");
  return worker->Invoke([worker, text, interrupt]() { worker->Speak(text, interrupt); });
}

bool ScreenReaderDriverOneCore::Silence() {
  if (!impl) return false;
  Impl *worker = impl;
  return worker->Invoke([worker]() { worker->StopPlayback(); });
}

bool ScreenReaderDriverOneCore::IsSpeaking() {
  return impl && impl->speaking.load();
}

bool ScreenReaderDriverOneCore::IsActive() {
  // Performance: Check cache first (100ms timeout)
  DWORD currentTime = GetTickCount();
  if ((currentTime - lastIsActiveTime) < CACHE_TIMEOUT_MS) {
    return cachedIsActive;
  }
  cachedIsActive = false;
  if (!disabled) {
    // OneCore is the Windows speech engine behind Narrator, not a screen
    // reader that can be detected on its own. It is only offered while a
    // screen reader has claimed the Windows screen-reader flag, so that it
    // stays below the named screen readers and above the SAPI fallback.
    BOOL screenReader = FALSE;
    if (SystemParametersInfoW(SPI_GETSCREENREADER, 0, &screenReader, 0) && screenReader != FALSE) {
      bool supported = false;
      try {
        supported = impl && impl->Start();
      }
      catch (...) {
        supported = false;
      }
      if (supported) {
        cachedIsActive = true;
      }
      else {
        TOLK_LOG_WARN("OneCore: Windows speech synthesis is unavailable, driver disabled");
        disabled = true;
      }
    }
  }
  lastIsActiveTime = currentTime;
  return cachedIsActive;
}
