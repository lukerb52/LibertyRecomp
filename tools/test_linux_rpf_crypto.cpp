// Standalone Linux regression test; no game data or SDK submodules required.
// clang++ -std=c++23 -ffunction-sections -fdata-sections \
//   tools/test_linux_rpf_crypto.cpp -Wl,--gc-sections -pthread -lcrypto -lz -o /tmp/test-linux-rpf
// /tmp/test-linux-rpf
#include "../LibertyRecomp/install/rpf_extractor.cpp"
#include <iostream>
#include <stdexcept>

static std::vector<uint8_t> Hex(std::string_view text)
{
    std::vector<uint8_t> result;
    for (size_t i = 0; i < text.size(); i += 2)
        result.push_back(static_cast<uint8_t>(std::stoul(std::string(text.substr(i, 2)), nullptr, 16)));
    return result;
}

static void Require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

int main()
{
    const auto key = Hex("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4");
    const auto plain = Hex("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51");
    // NIST plaintext/key encrypted 16 times with openssl enc -aes-256-ecb -nopad.
    const auto cipher = Hex("5cca7160663c7fb5a5859187700a7e792aebd995e77b99ce3d00dc0d04850542");
    for (size_t trailing = 0; trailing < 16; ++trailing)
    {
        auto data = cipher;
        data.insert(data.end(), trailing, 0xa5);
        auto expected = plain;
        expected.insert(expected.end(), trailing, 0xa5);
        Require(RpfExtractor::DecryptAES256_16Rounds(data, key), "AES decryption failed");
        Require(data == expected, "16-pass result or partial-block preservation failed");
    }

    auto data = cipher;
    for (size_t length : {size_t{0}, size_t{16}, size_t{31}, size_t{33}})
        Require(!RpfExtractor::DecryptAES256_16Rounds(data, std::vector<uint8_t>(length)),
                "Invalid AES key accepted");
    Require(data == cipher, "Invalid key modified ciphertext");
    std::vector<uint8_t> empty;
    Require(!RpfExtractor::DecryptAES256_16Rounds(empty, key), "Empty input accepted");
    for (size_t length = 1; length < 16; ++length)
    {
        std::vector<uint8_t> shortData(length, 0x42);
        Require(RpfExtractor::DecryptAES256_16Rounds(shortData, key), "Partial-only input failed");
        Require(shortData == std::vector<uint8_t>(length, 0x42), "Partial-only input changed");
    }
    std::cout << "Linux RPF crypto regression tests passed\n";
}
