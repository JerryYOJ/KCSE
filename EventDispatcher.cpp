#include "EventDispatcher.h"
#include "PluginManager.h"
#include "KCSE/KCSEAPI.h"
#include "KCSE/Trampoline.h"
#include "Offsets/Offsets.h"
#include "vtable_hook.h"
#include <Windows.h>
#include <spdlog/spdlog.h>

// ---- CompleteInit vtable hook (DataLoaded, one-shot) ----

using CompleteInitFn = void(__fastcall*)(void* pThis);
static CompleteInitFn g_origCompleteInit = nullptr;

void __fastcall Hooked_CompleteInit(void* pThis)
{
    g_origCompleteInit(pThis);
    spdlog::info("DataLoaded");
    PluginManager::Dispatch(KCSE::kPluginHandle_KCSE, KCSE::IMessagingInterface::kMessage_DataLoaded, nullptr, 0, nullptr);
    VtableHook::Restore<CompleteInitFn>(pThis, 0x48, g_origCompleteInit);
    g_origCompleteInit = nullptr;
}

// ---- OnActionEvent vtable hook (LoadGame) ----
// Save game loading goes through CCryAction::LoadGame → CryEngine serialization → eAE_PostLoad.
// The WH module message state machine (sub_180680FF4) is NOT used for save loads (state stays 0).

using OnActionEventFn = void(__fastcall*)(void* pThis, SActionEvent* pEvent);
static OnActionEventFn g_origOnActionEvent = nullptr;

void __fastcall Hooked_OnActionEvent(void* pThis, SActionEvent* pEvent)
{
    g_origOnActionEvent(pThis, pEvent);

    if (pEvent->m_event == SActionEvent::eAE_PostLoad) {
        spdlog::info("LoadGame");
        PluginManager::Dispatch(KCSE::kPluginHandle_KCSE, KCSE::IMessagingInterface::kMessage_LoadGame, nullptr, 0, nullptr);
    }
}

// ---- Module message call site hooks (SaveGame + NewGame) ----

using ProcessMessageFn = void(__fastcall*)(void* pManager, void* pMessage);
static ProcessMessageFn g_origProcessMessage = nullptr;

constexpr uintptr_t kCallSite_SaveGame = 0xF08642;
constexpr uintptr_t kCallSite_NewGame  = 0xA8B944;

void __fastcall Hook_SaveGame(void* pManager, void* pMessage)
{
    spdlog::info("SaveGame");
    PluginManager::Dispatch(KCSE::kPluginHandle_KCSE, KCSE::IMessagingInterface::kMessage_SaveGame, nullptr, 0, nullptr);
    g_origProcessMessage(pManager, pMessage);
}

void __fastcall Hook_NewGame(void* pManager, void* pMessage)
{
    spdlog::info("NewGame");
    PluginManager::Dispatch(KCSE::kPluginHandle_KCSE, KCSE::IMessagingInterface::kMessage_NewGame, nullptr, 0, nullptr);
    g_origProcessMessage(pManager, pMessage);
}

namespace EventDispatcher {

void Install()
{
    auto* pFramework = Offsets::GetCCryAction();
    auto whGame = reinterpret_cast<uintptr_t>(GetModuleHandleA("WHGame.dll"));
    auto& trampoline = KCSE::GetTrampoline();

    g_origCompleteInit = VtableHook::Swap<CompleteInitFn>(pFramework, 0x48, Hooked_CompleteInit);
    g_origOnActionEvent = VtableHook::Swap<OnActionEventFn>(pFramework, 0x3D8, Hooked_OnActionEvent);

    g_origProcessMessage = reinterpret_cast<ProcessMessageFn>(
        trampoline.write_call<5>(whGame + kCallSite_SaveGame, Hook_SaveGame));

    trampoline.write_call<5>(whGame + kCallSite_NewGame, Hook_NewGame);

    spdlog::info("Hooks installed");
}

void Remove()
{
    auto* pFramework = Offsets::GetCCryAction();
    if (pFramework) {
        if (g_origCompleteInit)
            VtableHook::Restore<CompleteInitFn>(pFramework, 0x48, g_origCompleteInit);
        if (g_origOnActionEvent)
            VtableHook::Restore<OnActionEventFn>(pFramework, 0x3D8, g_origOnActionEvent);
    }
}

}  // namespace EventDispatcher
