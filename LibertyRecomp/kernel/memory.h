#pragma once

#ifndef _WIN32
#define MEM_COMMIT  0x00001000  
#define MEM_RESERVE 0x00002000  
#endif

#include <cpu/ppc_context.h>
#include <memory>
namespace rex::memory { class Memory; }

struct Memory
{
    uint8_t* base{};

    Memory();
    ~Memory();  // Defined in .cpp where rex::memory::Memory is complete

    // Optional standalone memory allocation and function-table registration.
    // Dispatch is authoritative in rex::Runtime: FindFunction/InsertFunction
    // require a live runtime whose virtual_membase() matches base.
    // Normal startup uses Runtime::Setup() and sets base directly instead.
    void InitializeFromRexGlue();

    // Access the underlying RexGlue memory system (available after InitializeFromRexGlue)
    rex::memory::Memory* GetRexMemory() const noexcept { return rex_memory_.get(); }

    // Transfer ownership of the RexGlue memory to caller (e.g. rex::Runtime).
    // After this call, GetRexMemory() returns nullptr but base remains valid
    // as long as the caller keeps the returned unique_ptr alive.
    std::unique_ptr<rex::memory::Memory> TakeRexMemory() noexcept { return std::move(rex_memory_); }

    // SDK v0.2.1: now public - called from main.cpp after Runtime::Setup() sets base.
    void PopulateFunctionTableAndVtables();

private:
    std::unique_ptr<rex::memory::Memory> rex_memory_;

public:

    bool IsInMemoryRange(const void* host) const noexcept
    {
        return host >= base && host < (base + PPC_MEMORY_SIZE);
    }

    void* Translate(size_t offset) const noexcept
    {
        if (offset)
            assert(offset < PPC_MEMORY_SIZE);

        return base + offset;
    }

    uint32_t MapVirtual(const void* host) const noexcept
    {
        if (host)
            assert(IsInMemoryRange(host));

        return static_cast<uint32_t>(static_cast<const uint8_t*>(host) - base);
    }

    PPCFunc* FindFunction(uint32_t guest) const noexcept;
    void InsertFunction(uint32_t guest, PPCFunc* host);
};

extern "C" void* MmGetHostAddress(uint32_t ptr);
extern Memory g_memory;
