#pragma once

#include <cstdint>
#include <cstring>
#include <rex/system/xmemory.h>

namespace LibertyMemory
{
// rawValue is already encoded for the guest; do not byte-swap it here.
inline bool PatchImageU32(rex::memory::Memory* memory, uint32_t address, uint32_t rawValue)
{
    if (!memory || !memory->virtual_membase() || (address & 3u) != 0)
        return false;

    auto* heap = memory->LookupHeap(address);
    if (!heap)
        return false;

    uint32_t oldProtect = 0;
    if (!heap->Protect(address, sizeof(rawValue),
                       rex::memory::kMemoryProtectRead | rex::memory::kMemoryProtectWrite,
                       &oldProtect))
        return false;

    std::memcpy(memory->virtual_membase() + address, &rawValue, sizeof(rawValue));
    return heap->Protect(address, sizeof(rawValue), oldProtect);
}
}
