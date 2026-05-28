#pragma once
#include <cstdint>
#include <Windows.h>

namespace VtableHook {

template<typename Fn>
Fn Swap(void* pObject, uint32_t vtableByteOffset, Fn newFunc) {
    auto vtable = *reinterpret_cast<uintptr_t**>(pObject);
    auto& slot = vtable[vtableByteOffset / 8];

    DWORD oldProt;
    VirtualProtect(&slot, sizeof(slot), PAGE_EXECUTE_READWRITE, &oldProt);
    auto original = reinterpret_cast<Fn>(slot);
    slot = reinterpret_cast<uintptr_t>(newFunc);
    VirtualProtect(&slot, sizeof(slot), oldProt, &oldProt);

    return original;
}

template<typename Fn>
void Restore(void* pObject, uint32_t vtableByteOffset, Fn originalFunc) {
    auto vtable = *reinterpret_cast<uintptr_t**>(pObject);
    auto& slot = vtable[vtableByteOffset / 8];

    DWORD oldProt;
    VirtualProtect(&slot, sizeof(slot), PAGE_EXECUTE_READWRITE, &oldProt);
    slot = reinterpret_cast<uintptr_t>(originalFunc);
    VirtualProtect(&slot, sizeof(slot), oldProt, &oldProt);
}

}  // namespace VtableHook
