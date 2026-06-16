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
    recoverCount(0),
    defaultRate(0),
    defaultVolume(100)
{
    TOLK_LOG_INFO("SAPI: Initializing optimized fallback driver (Windows 10/11)");
    Initialize();
}

ScreenReaderDriverSAPI::~ScreenReaderDriverSAPI() {
    TOLK_LOG_INFO("SAPI: Finalizing driver");
    Finalize();
}

bool ScreenReaderDriverSAPI::Recover() {
    // 防止无限恢复：最多尝试3次
    if (InterlockedIncrement(&recoverCount) > 3) {
        TOLK_LOG_ERROR("SAPI: Recovery failed after 3 attempts, giving up");
        return false;
    }

    TOLK_LOG_WARN("SAPI: Attempting automatic recovery (%d/3)", recoverCount);

    // 清理旧实例
    if (controller) {
        controller->Release();
        controller = nullptr;
    }

    // 重新初始化
    HRESULT hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_INPROC_SERVER, IID_ISpVoice, (void **)&controller);
    if (FAILED(hr)) {
        TOLK_LOG_ERROR("SAPI: Recovery failed, hr=0x%08X", hr);
        return false;
    }

    // 恢复默认参数
    controller->SetRate(defaultRate);
    controller->SetVolume(defaultVolume);

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

    // 自动恢复机制
    if (!controller && !Recover()) {
        ReleaseSRWLockExclusive(&srwLock);
        return false;
    }

    // Windows 10/11 优化：使用 SAPI 5.4 最佳实践
    // SPF_ASYNC: 异步语音，不阻塞调用线程
    // SPF_IS_NOT_XML: 明确不是XML，避免解析开销
    // SPF_PURGEBEFORESPEAK: 打断当前语音
    DWORD flags = SPF_ASYNC | SPF_IS_NOT_XML;
    if (interrupt) {
        flags |= SPF_PURGEBEFORESPEAK;
    }

    HRESULT hr = controller->Speak(str, flags, nullptr);

    // 错误处理和自动恢复
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
    // 快速路径：先检查原子变量
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

    // Windows 10/11 优化：使用 Skip 方法代替空语音
    // 这是 SAPI 5.4 推荐的停止语音方式
    HRESULT hr = controller->Skip(L"Sentence", 1000, nullptr);
    if (FAILED(hr)) {
        // 回退到传统方式
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

    // Windows 10/11 优化：使用 CLSCTX_ALL 获得最佳兼容性
    HRESULT hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, (void **)&controller);
    if (FAILED(hr)) {
        TOLK_LOG_WARN("SAPI: CoCreateInstance failed, hr=0x%08X", hr);
        ReleaseSRWLockExclusive(&srwLock);
        return;
    }

    TOLK_LOG_INFO("SAPI: COM instance created successfully (SAPI 5.4 mode)");

    // Windows 10/11 优化：设置默认语音参数
    // 缓存默认值，用于恢复
    controller->GetRate(&defaultRate);
    controller->GetVolume(&defaultVolume);

    // Windows 10/11 优化：设置语音优先级为最高
    controller->SetPriority(SPRI_ALERT);

    // Windows 10/11 优化：启用音频优化
    controller->SetOutput(nullptr, TRUE);

    TOLK_LOG_INFO("SAPI: Default rate=%d, volume=%d", defaultRate, defaultVolume);

    ReleaseSRWLockExclusive(&srwLock);
}

void ScreenReaderDriverSAPI::Finalize() {
    AcquireSRWLockExclusive(&srwLock);

    if (controller) {
        TOLK_LOG_INFO("SAPI: Releasing COM instance");

        // 优雅停止：先停止语音
        controller->Skip(L"Sentence", 1000, nullptr);

        // 然后释放
        controller->Release();
        controller = nullptr;
    }

    InterlockedExchange(&isSpeaking, 0);
    ReleaseSRWLockExclusive(&srwLock);
}