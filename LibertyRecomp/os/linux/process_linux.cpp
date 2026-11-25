#include <os/process.h>

#include <signal.h>
#include <limits.h>
#include <unistd.h>

std::filesystem::path os::process::GetExecutablePath()
{
    char exePath[PATH_MAX];
    const ssize_t length = readlink("/proc/self/exe", exePath, sizeof(exePath));
    if (length > 0 && static_cast<size_t>(length) < sizeof(exePath))
    {
        return std::filesystem::path(std::u8string_view(
            reinterpret_cast<const char8_t*>(exePath), static_cast<size_t>(length)));
    }
    else
    {
        return std::filesystem::path();
    }
}

std::filesystem::path os::process::GetExecutableRoot()
{
    return GetExecutablePath().remove_filename();
}

std::filesystem::path os::process::GetWorkingDirectory()
{
    char cwd[PATH_MAX] = {};
    char *res = getcwd(cwd, sizeof(cwd));
    if (res != nullptr)
    {
        return std::filesystem::path(std::u8string_view((const char8_t*)(cwd)));
    }
    else
    {
        return std::filesystem::path();
    }
}

bool os::process::SetWorkingDirectory(const std::filesystem::path& path)
{
    return chdir(path.c_str()) == 0;
}

bool os::process::StartProcess(const std::filesystem::path& path, const std::vector<std::string>& args, std::filesystem::path work)
{
    pid_t pid = fork();
    if (pid < 0)
        return false;

    if (pid == 0)
    {
        setsid();
        
        std::u8string workU8 = work.u8string();
        chdir((const char*)(workU8.c_str())); 
        
        std::u8string pathU8 = path.u8string();
        std::vector<char*> argStrs;
        argStrs.push_back((char*)(pathU8.c_str()));
        for (const std::string& arg : args)
            argStrs.push_back((char *)(arg.c_str()));
        
        argStrs.push_back(nullptr);
        execvp((const char*)(pathU8.c_str()), argStrs.data());
        raise(SIGKILL);
    }

    return true;
}

void os::process::CheckConsole()
{
    // Always visible on Linux.
    g_consoleVisible = true;
}

void os::process::ShowConsole()
{
    // Unnecessary on Linux.
}
