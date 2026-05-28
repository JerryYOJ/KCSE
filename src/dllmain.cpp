#include "dll_proxy/dinput8_proxy.h"
#include "Interfaces.h"
#include "PluginManager.h"
#include "EventDispatcher.h"
#include "TaskInterface.h"
#include "KCSE/Trampoline.h"
#include "Offsets/Offsets.h"
#include <Windows.h>
#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

uint32_t g_kcseVersion = 1;
uint32_t g_gameVersion = 0;

static uint32_t DetectGameVersion()
{
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);

    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeA(exePath, &handle);
    if (!size) return 0;

    std::vector<uint8_t> buffer(size);
    if (!GetFileVersionInfoA(exePath, handle, size, buffer.data())) return 0;

    VS_FIXEDFILEINFO* info = nullptr;
    UINT len = 0;
    if (!VerQueryValueA(buffer.data(), "\\", reinterpret_cast<void**>(&info), &len)) return 0;

    return (HIWORD(info->dwFileVersionMS) << 24) |
           (LOWORD(info->dwFileVersionMS) << 16) |
           (HIWORD(info->dwFileVersionLS) << 8)  |
            LOWORD(info->dwFileVersionLS);
}

static void MainThread(HMODULE hModule)
{
    auto logger = spdlog::basic_logger_mt("KCSE", "KCSE/KCSE.log", true);
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    spdlog::flush_on(spdlog::level::info);

    g_gameVersion = DetectGameVersion();
    spdlog::info("Initializing v{} (game {}.{}.{}.{})", g_kcseVersion,
        (g_gameVersion >> 24) & 0xFF, (g_gameVersion >> 16) & 0xFF,
        (g_gameVersion >> 8) & 0xFF, g_gameVersion & 0xFF);

    while (!Offsets::GetCCryAction())
        Sleep(100);

    KCSE::AllocTrampoline(1 << 12);
    TaskInterface::InstallHook();
    EventDispatcher::Install();

    PluginManager::Init();
    PluginManager::LoadAll(&g_kcseInterface);
    PluginManager::DispatchPostLoad();

    spdlog::info("Ready.");
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        if (!proxy::Init())
            return FALSE;
        CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(MainThread), hModule, 0, nullptr);
        break;

    case DLL_PROCESS_DETACH:
        TaskInterface::RemoveHook();
        EventDispatcher::Remove();
        KCSE::GetTrampoline().destroy();
        proxy::Shutdown();
        break;
    }
    return TRUE;
}
