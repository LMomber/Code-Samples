#include "helpers_dx12.hpp"
#include "tools/log.hpp"

#include <direct.h>

std::string ConvertWideToString(LPWSTR wideStr) 
{
    if (!wideStr) return std::string();

    int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, nullptr, 0, nullptr, nullptr);
    std::string narrowStr(bufferSize, 0);
    WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, narrowStr.data(), bufferSize, nullptr, nullptr);

    return narrowStr;
}

void PrintLastError()
{
    DWORD error = GetLastError();
    LPWSTR errorText = nullptr;

    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL,
                   error,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPWSTR)&errorText,
                   0,
                   NULL);

    if (error != 0 && errorText)
    {
        bee::Log::Warn("Error : {}\n", ConvertWideToString(errorText));
        LocalFree(errorText);
    }
}

void PrintWorkingDirectory()
{
    char cwd[256];
    _getcwd(cwd, sizeof(cwd));
    bee::Log::Info("Current Working Directory: {}\n", cwd);
}
