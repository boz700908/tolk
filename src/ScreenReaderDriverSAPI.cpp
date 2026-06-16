/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverSAPI.cpp
 *  Description:    Driver for the Microsoft Speech API (SAPI) - Optimized for Windows 10/11
 *  Copyright:      (c) 2014, Davy Kager <mail@davykager.nl>
 *  License:        LGPLv3
 */
#include "ScreenReaderDriverSAPI.h"
#include "TolkDebug.h"

ScreenReaderDriverSAPI::ScreenReaderDriverSAPI() :
    ScreenReaderDriver(L"SAPI", true, false),
    controller(nullptr),
    srwLock(SRWLOCK_INIT),
    isSpeaking(0),
    recoverCount(0)
{
    TOLK_LOG_INFO("SAPI: Initializing optimized fallback driver (Windows 10/11)");
    Initialize();
}

ScreenReaderDriverSAPI::~ScreenReaderDriverSAPI() {
    TOLK_LOG_INFO("SAPI: Finalizing driver");
    Finalize();
}

bool ScreenReaderDriverSAPI::Recover() {
    // Prevent infinite recovery: max 3 attempts
    if (InterlockedIncrement(&recoverCount) > 3) {
        TOLK_LOG_ERROR("SAPI: Recovery failed after 3 attempts, giving up");
        return false;
    }

    TOLK_LOG_WARN("SAPI: Attempting automatic recovery (%d/3)", recoverCount);

    // Clean up old instance
    if (controller) {
        controller->Release();
        controller = nullptr;
    }

    // Reinitialize - uses system default voice settings automatically
    HRESULT hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_INPROC_SERVER, IID_ISpVoice, (void **)&controller);
    if (FAILED(hr)) {
        TOLK_LOG_ERROR("SAPI: Recovery failed, hr=0x%08X", hr);
        return false;
    }

    TOLK_LOG_INFO("SAPI: Recovery successful");
    InterlockedExchange(&recoverCount, 0);
    return true;
}

bool ScreenReaderDriverSAPI::Speak(const wchar_t *str, bool interrupt) {
    AcquireSRWLockExclusive(&srwLock);

    if (!str || str[0] == L'\0') {
        ReleaseSRWLockExclusive(&srwLock);
        return false;
    }

    // Auto-recovery mechanism
    if (!controller && !Recover()) {
        ReleaseSRWLockExclusive(&srwLock);
        return false;
    }

    // Windows 10/11 optimization: SAPI 5.4 best practices
    // SPF_ASYNC: Async speech, does not block calling thread
    // SPF_IS_NOT_XML: Explicitly not XML, avoids parsing overhead
    // SPF_PURGEBEFORESPEAK: Interrupt current speech
    DWORD flags = SPF_ASYNC | SPF_IS_NOT_XML;
    if (interrupt) {
        flags |= SPF_PURGEBEFORESPEAK;
    }

    HRESULT hr = controller->Speak(str, flags, nullptr);

    // Error handling and auto-recovery
    if (FAILED(hr)) {
        TOLK_LOG_WARN("SAPI: Speak failed (hr=0x%08X), attempting recovery", hr);
        if (Recover()) {
            hr = controller->Speak(str, flags, nullptr);
        }
    }

    if (SUCCEEDED(hr)) {
        InterlockedExchange(&isSpeaking, 1);
        InterlockedExchange(&recoverCount, 0);
    }

    ReleaseSRWLockExclusive(&srwLock);
    return SUCCEEDED(hr);
}

bool ScreenReaderDriverSAPI::IsSpeaking() {
    // Fast path: check atomic flag first
    if (InterlockedCompareExchange(&isSpeaking, 0, 0) == 0) {
        return false;
    }

    AcquireSRWLockExclusive(&srwLock);

    if (!controller) {
        InterlockedExchange(&isSpeaking, 0);
        ReleaseSRWLockExclusive(&srwLock);
        return false;
    }

    SPVOICESTATUS status;
    HRESULT hr = controller->GetStatus(&status, nullptr);
    if (FAILED(hr)) {
        TOLK_LOG_WARN("SAPI: GetStatus failed, hr=0x%08X", hr);
        InterlockedExchange(&isSpeaking, 0);
        ReleaseSRWLockExclusive(&srwLock);
        return false;
    }

    bool speaking = (status.dwRunningState == SPRS_IS_SPEAKING);
    if (!speaking) {
        InterlockedExchange(&isSpeaking, 0);
    }

    ReleaseSRWLockExclusive(&srwLock);
    return speaking;
}

bool ScreenReaderDriverSAPI::Silence() {
    AcquireSRWLockExclusive(&srwLock);

    if (!controller && !Recover()) {
        ReleaseSRWLockExclusive(&srwLock);
        return false;
    }

    // Windows 10/11 optimization: Use Skip method instead of empty speech
    // This is the recommended way to stop speech in SAPI 5.4
    HRESULT hr = controller->Skip(L"Sentence", 1000, nullptr);
    if (FAILED(hr)) {
        // Fallback to traditional method
        const DWORD flags = SPF_ASYNC | SPF_IS_NOT_XML | SPF_PURGEBEFORESPEAK;
        hr = controller->Speak(nullptr, flags, nullptr);
    }

    if (SUCCEEDED(hr)) {
        InterlockedExchange(&isSpeaking, 0);
    }

    ReleaseSRWLockExclusive(&srwLock);
    return SUCCEEDED(hr);
}

void ScreenReaderDriverSAPI::Initialize() {
    AcquireSRWLockExclusive(&srwLock);

    if (controller) {
        ReleaseSRWLockExclusive(&srwLock);
        return;
    }

    // Windows 10/11 optimization: Use CLSCTX_ALL for best compatibility
    // ISpVoice automatically inherits system default voice, rate, and volume settings
    HRESULT hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, (void **)&controller);
    if (FAILED(hr)) {
        TOLK_LOG_WARN("SAPI: CoCreateInstance failed, hr=0x%08X", hr);
        ReleaseSRWLockExclusive(&srwLock);
        return;
    }

    TOLK_LOG_INFO("SAPI: COM instance created successfully (SAPI 5.4 mode)");

    ReleaseSRWLockExclusive(&srwLock);
}

void ScreenReaderDriverSAPI::Finalize() {
    AcquireSRWLockExclusive(&srwLock);

    if (controller) {
        TOLK_LOG_INFO("SAPI: Releasing COM instance");

        // Graceful stop: Stop speech first
        controller->Skip(L"Sentence", 1000, nullptr);

        // Then release
        controller->Release();
        controller = nullptr;
    }

    InterlockedExchange(&isSpeaking, 0);
    ReleaseSRWLockExclusive(&srwLock);
}