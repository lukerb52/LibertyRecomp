// Standalone regression: real SDK GDFX mounting, no game content or identity stubs.
#include "install/gta4_source_inspector.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace fs = std::filesystem;
using namespace gta4::install;

void Require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

struct TemporaryDirectory {
    fs::path path;
    TemporaryDirectory() {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0; attempt < 100; ++attempt) {
            path = fs::temp_directory_path() /
                ("liberty-source-selection-" + std::to_string(nonce) + "-" + std::to_string(attempt));
            if (fs::create_directory(path)) return;
        }
        throw std::runtime_error("Cannot create fixture directory");
    }
    ~TemporaryDirectory() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};

void StoreLe32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes.at(offset + i) = uint8_t(value >> (8 * i));
}

void WriteAt(std::ofstream& stream, uint64_t offset, const std::vector<uint8_t>& bytes) {
    stream.seekp(static_cast<std::streamoff>(offset));
    stream.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    Require(bool(stream), "Cannot write fixture");
}

// Match DiscImageDevice::Verify/ReadEntry in the SDK: volume descriptor at
// partition sector 32, LE root sector/size at +20/+24; a directory node has
// LE child ordinals (+0/+2), sector (+4), length (+8), attributes (+12),
// name length (+13), and name (+14). Zero child ordinals terminate the tree.
constexpr uint32_t kSectorSize = 2048;
constexpr uint32_t kRootSector = 33;
constexpr uint32_t kFileSector = 34;
constexpr std::array<uint32_t, 5> kPartitionOffsets = {
    0x00000000, 0x0000FB20, 0x00020600, 0x02080000, 0x0FD90000,
};
// Long enough to reach the XEX magic check, deliberately not an executable.
const std::vector<uint8_t> kInvalidXex(64, 0xA5);

enum class Damage { kNone, kRootOutsideImage, kTruncatedName, kFileOutsideImage };

void WriteDisc(const fs::path& path, uint32_t partition, std::string_view name,
               Damage damage = Damage::kNone) {
    std::vector<uint8_t> descriptor(28, 0);
    constexpr std::string_view magic = "MICROSOFT*XBOX*MEDIA";
    for (size_t i = 0; i < magic.size(); ++i) descriptor[i] = magic[i];
    StoreLe32(descriptor, 20, damage == Damage::kRootOutsideImage ? 0xFFFFFFFF : kRootSector);
    StoreLe32(descriptor, 24, uint32_t(14 + name.size()));

    std::vector<uint8_t> root(14 + name.size(), 0);
    StoreLe32(root, 4, damage == Damage::kFileOutsideImage ? 0xFFFFFFFF : kFileSector);
    StoreLe32(root, 8, uint32_t(kInvalidXex.size()));
    root[13] = damage == Damage::kTruncatedName ? 255 : uint8_t(name.size());
    for (size_t i = 0; i < name.size(); ++i) root[14 + i] = name[i];

    // Seek instead of allocating the large partition prefix. The largest
    // fixture is a sparse ~254 MiB file with only a few disk blocks written.
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    Require(bool(stream), "Cannot create disc fixture");
    WriteAt(stream, uint64_t(partition) + 32 * kSectorSize, descriptor);
    WriteAt(stream, uint64_t(partition) + kRootSector * kSectorSize, root);
    WriteAt(stream, uint64_t(partition) + kFileSector * kSectorSize, kInvalidXex);
    stream.close();
    Require(bool(stream), "Cannot close disc fixture");
}

void RequireRejected(const GameSourceInspection& result) {
    Require(!result.supported(), "Synthetic input must never be reported as supported");
    Require(!result.rejection_reason.empty(), "Rejected source needs an actionable reason");
    const auto formatted = FormatGameSourceInspection(result);
    Require(formatted.find(result.rejection_reason) != std::string::npos,
            "User-facing inspection must preserve the rejection reason");
}

void TestDiscRecognition(const fs::path& directory) {
    const auto expected = InspectGameXex(kInvalidXex);
    RequireRejected(expected);
    Require(expected.status == GameSourceStatus::kCorruptImage, "Invalid XEX should be corrupt");
    for (const auto offset : kPartitionOffsets) {
        for (const auto name : {"default.xex", "DEFAULT.XEX", "DeFaUlT.XeX"}) {
            const auto path = directory / "synthetic.iso";
            WriteDisc(path, offset, name);
            const auto result = InspectGameSource(path);
            RequireRejected(result);
            Require(result.source_kind == GameSourceKind::kDiscImage, "GDFX must be recognized as disc");
            Require(result.status != GameSourceStatus::kMissingDefaultXex,
                    "default.xex lookup must ignore filename case");
            Require(result.status == expected.status && result.rejection_reason == expected.rejection_reason,
                    "Disc must read default.xex bytes and reach the real XEX parser");
            fs::remove(path);
        }
    }
}

void TestMalformedDiscs(const fs::path& directory) {
    for (const auto offset : kPartitionOffsets) {
        for (const auto damage : {Damage::kRootOutsideImage, Damage::kTruncatedName,
                                  Damage::kFileOutsideImage}) {
            const auto path = directory / "malformed.iso";
            WriteDisc(path, offset, "DEFAULT.XEX", damage);
            const auto result = InspectGameSource(path);
            RequireRejected(result);
            Require(result.source_kind == GameSourceKind::kDiscImage, "Malformed ISO should retain source kind");
            Require(result.status == GameSourceStatus::kCorruptImage, "Malformed GDFX must be rejected");
            fs::remove(path);
        }
    }
    const auto missing = directory / "missing-default.iso";
    WriteDisc(missing, 0, "other.bin");
    const auto result = InspectGameSource(missing);
    RequireRejected(result);
    Require(result.source_kind == GameSourceKind::kDiscImage, "Valid GDFX with other file is a disc");
    Require(result.status == GameSourceStatus::kMissingDefaultXex, "Missing XEX must have distinct diagnostic");
}

void TestNonDiscs(const fs::path& directory) {
    const auto path = directory / "random.iso";
    std::vector<uint8_t> random(40 * kSectorSize);
    uint32_t seed = 123456789;
    for (auto& byte : random) {
        seed = seed * 1664525u + 1013904223u;
        byte = uint8_t(seed >> 24);
    }
    for (const auto& bytes : {std::vector<uint8_t>{}, std::vector<uint8_t>{'M', 'I', 'C'}, random}) {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        WriteAt(stream, 0, bytes);
        stream.close();
        Require(bool(stream), "Cannot close random fixture");
        const auto result = InspectGameSource(path);
        RequireRejected(result);
        Require(result.status == GameSourceStatus::kCorruptImage, "Random/truncated ISO must be corrupt");
    }
    const auto missing = InspectGameSource(directory / "nonexistent.iso");
    RequireRejected(missing);
}
}  // namespace

int main() {
    try {
        TemporaryDirectory directory;
        TestDiscRecognition(directory.path);
        TestMalformedDiscs(directory.path);
        TestNonDiscs(directory.path);
        std::cout << "Source selection regression passed: 15 offset/case combinations, "
                     "15 malformed GDFX images, missing XEX, random/truncated/missing files.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Source selection regression failed: " << error.what() << '\n';
        return 1;
    }
}
