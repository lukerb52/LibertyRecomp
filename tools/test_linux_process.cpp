// clang++ -std=c++23 -I LibertyRecomp tools/test_linux_process.cpp -o /tmp/test-linux-process
#include <filesystem>
#include <vector>
#include <string>
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <stdexcept>
#include <iostream>

static int readlinkMode = 0;
static ssize_t TestReadlink(const char* path, char* buffer, size_t size)
{
    if (readlinkMode == 0)
        return ::readlink(path, buffer, size);
    if (readlinkMode == 1)
        return -1;
    // readlink never appends a NUL. Simulate both a full and nearly full buffer.
    std::memset(buffer, 'x', size);
    return static_cast<ssize_t>(size - (readlinkMode == 2 ? 0 : 1));
}
#define readlink TestReadlink
#include "../LibertyRecomp/os/linux/process_linux.cpp"
#undef readlink

static void Require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

int main(int argc, char** argv)
{
    Require(argc > 0, "Missing executable path");
    const auto executable = std::filesystem::canonical(argv[0]);
    Require(os::process::GetExecutablePath() == executable, "Executable path mismatch");
    Require(os::process::GetExecutableRoot() == executable.parent_path() / "", "Executable root mismatch");
    Require(os::process::GetWorkingDirectory() == std::filesystem::current_path(), "Working directory mismatch");
    readlinkMode = 1;
    Require(os::process::GetExecutablePath().empty(), "Failed readlink not handled");
    readlinkMode = 2;
    Require(os::process::GetExecutablePath().empty(), "Truncated readlink not rejected");
    readlinkMode = 3;
    Require(os::process::GetExecutablePath().string() == std::string(PATH_MAX - 1, 'x'),
            "Unterminated readlink result not bounded by its length");
    std::cout << "Linux process regression tests passed\n";
}
