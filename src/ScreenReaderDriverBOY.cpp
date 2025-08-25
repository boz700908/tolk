/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverBOY.cpp
 *  Description:    Driver for the BOY screen reader.
 *  License:        LGPLv3
 */

#include "ScreenReaderDriverBOY.h"
#include <windows.h>
#include <string>
#include <algorithm>

typedef void(__stdcall *BoyCtrlSetAnyKeyStopSpeakingFunc)(bool);

static int g_speakCompleteReason = -1;

static bool g_speakParam1 = true; // 原 Param1 -> 顺延后第2个参数
static bool g_speakParam2 = true; // 原 Param2 -> 顺延后第3个参数
static bool g_speakParam3 = true; // 原 Param3 -> 顺延后第4个参数
static bool g_stopSpeakValue = true;
static bool g_enableAnyKeyStopFunc = true; // Param4

static bool ReadBoolFromIni(const wchar_t* section, const wchar_t* key, bool defaultValue, const std::wstring& iniPath)
{
    wchar_t buf[16] = {0};
    GetPrivateProfileStringW(section, key, defaultValue ? L"true" : L"false", buf, 16, iniPath.c_str());
    std::wstring val(buf);
    std::transform(val.begin(), val.end(), val.begin(), ::towlower);
    return (val == L"true");
}

static void LoadBoyCtrlConfig()
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring dir(path);
    size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        dir = dir.substr(0, pos + 1);
    }
    std::wstring iniPath = dir + L"boyctrl.ini";

    g_speakParam1 = ReadBoolFromIni(L"Config", L"Param1", true, iniPath);
    g_speakParam2 = ReadBoolFromIni(L"Config", L"Param2", true, iniPath);
    g_speakParam3 = ReadBoolFromIni(L"Config", L"Param3", true, iniPath);

    g_stopSpeakValue = g_speakParam1;
    g_enableAnyKeyStopFunc = ReadBoolFromIni(L"Config", L"Param4", true, iniPath);
}

void __stdcall SpeakCompleteCallback(int reason)
{
    g_speakCompleteReason = reason;
}

ScreenReaderDriverBOY::ScreenReaderDriverBOY()
    : ScreenReaderDriver(L"BoyPCReader", true, false)
#ifdef _WIN64
    , controller(LoadLibrary(L"BoyCtrl-x64.dll"))
#else
    , controller(LoadLibrary(L"BoyCtrl.dll"))
#endif
    , BoyInit(nullptr)
    , BoyUninit(nullptr)
    , BoyIsRunning(nullptr)
    , BoySpeak(nullptr)
    , BoyStopSpeak(nullptr)
{
    if (!controller) {
        return;
    }

    LoadBoyCtrlConfig();

    BoyInit      = reinterpret_cast<BoyCtrlInitialize>(   GetProcAddress(controller, "BoyCtrlInitialize"));
    BoyUninit    = reinterpret_cast<BoyCtrlUninitialize>( GetProcAddress(controller, "BoyCtrlUninitialize"));
    BoyIsRunning = reinterpret_cast<BoyCtrlIsReaderRunning>(GetProcAddress(controller, "BoyCtrlIsReaderRunning"));
    BoySpeak     = reinterpret_cast<BoyCtrlSpeak>(        GetProcAddress(controller, "BoyCtrlSpeak"));
    BoyStopSpeak = reinterpret_cast<BoyCtrlStopSpeaking>(GetProcAddress(controller, "BoyCtrlStopSpeaking"));

    if (BoyInit) {
        BoyInit(nullptr);
    }

    auto pAnyKeyStop = reinterpret_cast<BoyCtrlSetAnyKeyStopSpeakingFunc>(
        GetProcAddress(controller, "BoyCtrlSetAnyKeyStopSpeaking"));

    if (g_enableAnyKeyStopFunc && pAnyKeyStop) {
        pAnyKeyStop(g_stopSpeakValue);
    }
}

ScreenReaderDriverBOY::~ScreenReaderDriverBOY()
{
    if (!controller) {
        return;
    }

    if (BoyUninit) {
        BoyUninit();
    }

    FreeLibrary(controller);
}

bool ScreenReaderDriverBOY::Speak(const wchar_t* str, bool /*interrupt*/)
{
    g_speakCompleteReason = -1;
    if (BoySpeak) {
        // 去掉原第一个参数，直接从 str 作为第一个参数开始
        return (BoySpeak(str, g_speakParam1, g_speakParam2, g_speakParam3, SpeakCompleteCallback) == 0);
    }
    return false;
}

bool ScreenReaderDriverBOY::Braille(const wchar_t* /*str*/)
{
    return false;
}

bool ScreenReaderDriverBOY::Silence()
{
    if (BoyStopSpeak) {
        BoyStopSpeak(g_stopSpeakValue);
        g_speakCompleteReason = 3;
        return true;
    }
    return false;
}

bool ScreenReaderDriverBOY::IsSpeaking()
{
    return (g_speakCompleteReason == -1);
}

bool ScreenReaderDriverBOY::IsActive()
{
    return (BoyIsRunning ? BoyIsRunning() : false);
}

bool ScreenReaderDriverBOY::Output(const wchar_t* str, bool interrupt)
{
    bool spoke    = Speak(str, interrupt);
    bool brailled = Braille(str);
    return (spoke || brailled);
}
