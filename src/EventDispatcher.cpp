#include "EventDispatcher.h"
#include "PluginManager.h"
#include "KCSE/KCSEAPI.h"
#include "KCSE/Trampoline.h"
#include "Offsets/Offsets.h"
#include "REL.h"
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
    REL::Relocation<>{ *reinterpret_cast<std::uintptr_t*>(pThis) }.write_vfunc(9,g_origCompleteInit);
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

    // CCryAction vtable: CompleteInit slot 9 (+0x48), OnActionEvent slot 123 (+0x3D8).
    REL::Relocation<> fwVtbl{ *reinterpret_cast<std::uintptr_t*>(pFramework) };
    g_origCompleteInit  = reinterpret_cast<CompleteInitFn>(fwVtbl.write_vfunc(9, Hooked_CompleteInit));
    g_origOnActionEvent = reinterpret_cast<OnActionEventFn>(fwVtbl.write_vfunc(123, Hooked_OnActionEvent));

    // Mid-function call-site hooks: REL::ID(containing fn) + byte offset into it (a raw
    // mid-fn RVA isn't in the address library; the offset is build-invariant).
    g_origProcessMessage = reinterpret_cast<ProcessMessageFn>(  // C_ModuleMessageSaveGameRequest::Dispatch +0xCE
        REL::Relocation<>{ REL::ID(137723), 0xCE }.write_call<5>(Hook_SaveGame));
    REL::Relocation<>{ REL::ID(65914), 0x74 }.write_call<5>(Hook_NewGame);  // C_NewGamePrepareMessage::Dispatch +0x74

    spdlog::info("Hooks installed");
}

void Remove()
{
    auto* pFramework = Offsets::GetCCryAction();
    if (pFramework) {
        REL::Relocation<> fwVtbl{ *reinterpret_cast<std::uintptr_t*>(pFramework) };
        if (g_origCompleteInit)
            fwVtbl.write_vfunc(9,g_origCompleteInit);
        if (g_origOnActionEvent)
            fwVtbl.write_vfunc(123,g_origOnActionEvent);
    }
}

}  // namespace EventDispatcher
