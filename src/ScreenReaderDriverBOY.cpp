/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverBOY.cpp
 *  Description:    Driver for the BOY screen reader.
 *  Copyright:      (c) 2024, qt06<qt06.com@gmail.com>
 *  License:        LGPLv3
 */

#include "ScreenReaderDriverBOY.h"
#include <windows.h>
#include <string>

typedef void(__stdcall *BoyCtrlSetAnyKeyStopSpeakingFunc)(bool);

// Reason: 1=speaking completed, 2=Interrupted by new speaking, 3=Interrupted by stopped call
static int g_speakCompleteReason = -1;

static bool g_speakParam1 = true;
static bool g_speakParam2 = true;
static bool g_speakParam3 = true;
static bool g_stopSpeakValue = true;
static bool g_enableAnyKeyStopFunc = true;

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

    g_speakParam1 = GetPrivateProfileIntW(L"Config", L"Param1", 1, iniPath.c_str()) != 0;
    g_speakParam2 = GetPrivateProfileIntW(L"Config", L"Param2", 1, iniPath.c_str()) != 0;
    g_speakParam3 = GetPrivateProfileIntW(L"Config", L"Param3", 1, iniPath.c_str()) != 0;

    g_stopSpeakValue = g_speakParam1;
    g_enableAnyKeyStopFunc = GetPrivateProfileIntW(L"Config", L"Param4", 1, iniPath.c_str()) != 0;
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

    if (pAnyKeyStop && g_enableAnyKeyStopFunc) {
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
