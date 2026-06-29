#include "dll_proxy/dinput8_proxy.h"
#include "Interfaces.h"
#include "PluginManager.h"
#include "EventDispatcher.h"
#include "TaskInterface.h"
#include "KCSE/Trampoline.h"
#include "Offsets/Offsets.h"
#include "REL.h"
#include <Windows.h>
#include <string>
#include <string_view>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

uint32_t g_kcseVersion = 1;
uint32_t g_gameVersion = 0;

namespace {

std::string NarrowACP(const std::wstring& w)
{
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_ACP, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_ACP, 0, w.c_str(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

// "1.9.8" -> 0x01090800 (major<<24 | minor<<16 | patch<<8). 0 if unparsable.
uint32_t PackVersion(std::string_view v)
{
    uint32_t parts[3] = { 0, 0, 0 };
    int idx = 0;
    for (char c : v) {
        if (c == '.') { if (++idx >= 3) break; }
        else if (c >= '0' && c <= '9') parts[idx] = parts[idx] * 10 + static_cast<uint32_t>(c - '0');
        else break;
    }
    return (parts[0] << 24) | ((parts[1] & 0xFF) << 16) | ((parts[2] & 0xFF) << 8);
}

}  // namespace

static void MainThread(HMODULE)
{
    // REL::Module requires WHGame.dll to be mapped; wait for it before any REL:: use.
    while (!GetModuleHandleW(L"WHGame.dll"))
        Sleep(50);

    auto& mod = REL::Module::get();

    // Logs + address library live under <game_root>\KCSE\ (not Bin\Win64).
    const std::wstring kcseDir = mod.game_root() + L"\\KCSE";
    const std::wstring logDir = kcseDir + L"\\logs";
    CreateDirectoryW(kcseDir.c_str(), nullptr);
    CreateDirectoryW(logDir.c_str(), nullptr);

    auto logger = spdlog::basic_logger_mt("KCSE", NarrowACP(logDir + L"\\KCSE.log"), true);
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    spdlog::flush_on(spdlog::level::info);

    // The PE version is stale (1.9.7 on 1.9.8) -- report the real version from the
    // game's own config (wh_sys_version / whdlversions.txt), keyed by distribution.
    std::string rel{ mod.release() };       if (rel.empty()) rel = "?";
    std::string build{ mod.build_code() };  if (build.empty()) build = "?";
    g_gameVersion = PackVersion(mod.release());
    spdlog::info("KCSE v{} | {} | version {} | build {} | ts 0x{:08X}",
        g_kcseVersion, REL::to_string(mod.distribution()), rel, build, mod.timestamp());

    // Force the address library to load now -> clean, early failure on GOG/Epic if
    // the per-distribution table is missing (identity no-op on Steam).
    (void)REL::IDDatabase::get();

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
