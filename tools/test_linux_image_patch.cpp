// Build: cmake --build --preset linux-release --target liberty_test_image_patch --parallel 2
#include <kernel/image_patch.h>

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

static void Require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

int main()
{
    try
    {
        using namespace rex::memory;
        constexpr uint32_t imageBase = 0x82000000;
        constexpr uint32_t patchAddress = 0x82000768;
        constexpr uint32_t rawValue = 0x1234ABCD;
        constexpr uint32_t readWrite = kMemoryProtectRead | kMemoryProtectWrite;
        Memory memory;
        Require(!LibertyMemory::PatchImageU32(nullptr, patchAddress, rawValue),
                "Null memory accepted");
        Require(!LibertyMemory::PatchImageU32(&memory, patchAddress, rawValue),
                "Uninitialized memory accepted");
        Require(memory.Initialize(), "Memory initialization failed");
        auto* heap = memory.LookupHeap(imageBase);
        Require(heap != nullptr, "Image heap missing");
        const uint32_t pageSize = heap->page_size();
        Require(pageSize > patchAddress - imageBase + sizeof(rawValue),
                "Patch must fit within first image page");
        auto* base = memory.virtual_membase();

        // Distinct reservations reproduce the XEX loader's separate image regions.
        for (uint32_t page = 0; page < 2; ++page)
        {
            const uint32_t address = imageBase + page * pageSize;
            Require(heap->AllocFixed(address, pageSize, pageSize,
                                    kMemoryAllocationReserve | kMemoryAllocationCommit,
                                    readWrite), "Image page allocation failed");
            std::memset(base + address, page == 0 ? 0x5A : 0xA5, pageSize);
            Require(heap->Protect(address, pageSize, kMemoryProtectRead),
                    "Image page read-only protection failed");
        }
        const uint32_t gap = imageBase + 2 * pageSize;
        Require(heap->AllocFixed(gap, pageSize, pageSize, kMemoryAllocationReserve,
                                kMemoryProtectNoAccess), "Gap reservation failed");

        std::vector<unsigned char> expected(base + imageBase, base + imageBase + 2 * pageSize);
        Require(!heap->Protect(imageBase, 2 * pageSize, readWrite),
                "Whole-image protection unexpectedly spanned separate regions");
        for (uint32_t page = 0; page < 2; ++page)
        {
            uint32_t protection = 0;
            Require(heap->QueryProtect(imageBase + page * pageSize, &protection) &&
                        protection == kMemoryProtectRead,
                    "Failed broad protection changed image permissions");
        }
        Require(!LibertyMemory::PatchImageU32(&memory, patchAddress + 1, rawValue),
                "Unaligned patch accepted");
        Require(LibertyMemory::PatchImageU32(&memory, patchAddress, rawValue),
                "Read-only image word patch failed");
        std::memcpy(expected.data() + patchAddress - imageBase, &rawValue, sizeof(rawValue));
        Require(std::memcmp(base + imageBase, expected.data(), expected.size()) == 0,
                "Raw word differs or neighboring image bytes changed");

        // A write after a failed initial Protect would fault on this reserved-only page.
        Require(!LibertyMemory::PatchImageU32(&memory, gap, rawValue),
                "Uncommitted gap patch unexpectedly succeeded");
        for (uint32_t page = 0; page < 2; ++page)
        {
            uint32_t protection = 0;
            Require(heap->QueryProtect(imageBase + page * pageSize, &protection) &&
                        protection == kMemoryProtectRead,
                    "Exact read-only image protection was not restored");
        }
        uint32_t gapProtection = 0;
        Require(heap->QueryProtect(gap, &gapProtection) && gapProtection == kMemoryProtectNoAccess,
                "Failed gap patch changed protection");
        Require(std::memcmp(base + imageBase, expected.data(), expected.size()) == 0,
                "Failed patch changed image bytes");
        for (uint32_t page = 0; page < 3; ++page)
            Require(heap->Release(imageBase + page * pageSize), "Image reservation release failed");
        std::puts("Linux image patch regression tests passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Image patch regression failed: %s\n", error.what());
        return 1;
    }
}
