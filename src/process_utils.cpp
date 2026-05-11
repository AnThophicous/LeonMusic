#include "internal.hpp"

#include <array>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

#ifdef _WIN32
std::wstring utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, wide.data(), size);
    if (!wide.empty() && wide.back() == L'\0') {
        wide.pop_back();
    }
    return wide;
}

std::wstring quoteWindowsArgument(const std::string& arg) {
    if (arg.find_first_of(" \t\"") == std::string::npos) {
        return utf8ToWide(arg);
    }

    std::wstring wide = L"\"";
    int backslashes = 0;
    for (const wchar_t ch : utf8ToWide(arg)) {
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }

        if (ch == L'"') {
            wide.append(backslashes * 2 + 1, L'\\');
            wide.push_back(L'"');
            backslashes = 0;
            continue;
        }

        if (backslashes > 0) {
            wide.append(backslashes, L'\\');
            backslashes = 0;
        }
        wide.push_back(ch);
    }

    if (backslashes > 0) {
        wide.append(backslashes * 2, L'\\');
    }

    wide.push_back(L'"');
    return wide;
}
#endif

}  // namespace

leonmusic::ProcessResult leonmusic::runProcess(const std::vector<std::string>& args) {
    ProcessResult result {};
    if (args.empty()) {
        result.error = "no process arguments were provided";
        return result;
    }

#ifdef _WIN32
    SECURITY_ATTRIBUTES securityAttributes {};
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;

    HANDLE readHandle = nullptr;
    HANDLE writeHandle = nullptr;
    if (!CreatePipe(&readHandle, &writeHandle, &securityAttributes, 0)) {
        result.error = "CreatePipe failed";
        return result;
    }

    SetHandleInformation(readHandle, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startupInfo {};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdOutput = writeHandle;
    startupInfo.hStdError = writeHandle;
    startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    std::wstringstream commandBuilder {};
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            commandBuilder << L' ';
        }
        commandBuilder << quoteWindowsArgument(args[i]);
    }

    std::wstring commandLine = commandBuilder.str();
    PROCESS_INFORMATION processInfo {};
    const BOOL created = CreateProcessW(
        nullptr,
        commandLine.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo);

    CloseHandle(writeHandle);

    if (!created) {
        CloseHandle(readHandle);
        result.error = "CreateProcessW failed";
        return result;
    }

    std::array<char, 4096> buffer {};
    DWORD bytesRead = 0;
    while (ReadFile(readHandle, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr) && bytesRead > 0) {
        result.output.append(buffer.data(), bytesRead);
    }

    WaitForSingleObject(processInfo.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);
    result.exitCode = static_cast<int>(exitCode);

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    CloseHandle(readHandle);
#else
    int pipefd[2] {};
    if (pipe(pipefd) != 0) {
        result.error = "pipe failed";
        return result;
    }

    const pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        result.error = "fork failed";
        return result;
    }

    if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        std::vector<char*> argv {};
        argv.reserve(args.size() + 1);
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
    }

    close(pipefd[1]);
    std::array<char, 4096> buffer {};
    ssize_t count = 0;
    while ((count = read(pipefd[0], buffer.data(), buffer.size())) > 0) {
        result.output.append(buffer.data(), static_cast<std::size_t>(count));
    }
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        result.exitCode = WEXITSTATUS(status);
    }
#endif

    return result;
}

