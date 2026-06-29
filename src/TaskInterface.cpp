#include "TaskInterface.h"
#include "Offsets/Offsets.h"
#include "REL.h"

static std::mutex          s_taskLock;
static std::queue<KCSE::TaskFn>  s_tasks;

using PostUpdateFn = void(__fastcall*)(void* pThis, bool haveFocus, unsigned int updateFlags);
static PostUpdateFn g_origPostUpdate = nullptr;

void __fastcall Hooked_PostUpdate(void* pThis, bool haveFocus, unsigned int updateFlags)
{
    g_origPostUpdate(pThis, haveFocus, updateFlags);
    TaskInterface::ProcessTasks();
}

namespace TaskInterface {

void AddTask(KCSE::TaskFn task)
{
    std::lock_guard lock(s_taskLock);
    s_tasks.push(task);
}

void ProcessTasks()
{
    std::queue<KCSE::TaskFn> localQueue;
    {
        std::lock_guard lock(s_taskLock);
        localQueue.swap(s_tasks);
    }

    while (!localQueue.empty()) {
        auto task = localQueue.front();
        localQueue.pop();
        task();
    }
}

void InstallHook()
{
    auto* pFramework = Offsets::GetCCryAction();
    // CCryAction vtable: PostUpdate slot 12 (+0x60).
    g_origPostUpdate = reinterpret_cast<PostUpdateFn>(
        REL::Relocation<>{ *reinterpret_cast<std::uintptr_t*>(pFramework) }.write_vfunc(12,Hooked_PostUpdate));
}

void RemoveHook()
{
    auto* pFramework = Offsets::GetCCryAction();
    if (pFramework && g_origPostUpdate)
        REL::Relocation<>{ *reinterpret_cast<std::uintptr_t*>(pFramework) }.write_vfunc(12,g_origPostUpdate);
}

}  // namespace TaskInterface
