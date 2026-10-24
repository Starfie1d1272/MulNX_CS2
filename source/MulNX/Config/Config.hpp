#pragma once
#include <string>
#include <source_location>
#include <functional>

class MulNXInfo {
public:
    inline static constexpr const char Version[] = MulNXVersion;
    inline static constexpr const char TimeStamp[] = "Built: " __DATE__ " " __TIME__;
    inline static constexpr const char FullName[] = "Multiple Next eXtension";
    inline static constexpr const char DeveloperName[] = "Co1Swet";
#ifdef _DEBUG
    inline static constexpr const bool IsDebugVersion = true;
#else
    inline static constexpr const bool IsDebugVersion = false;
#endif
};

namespace MulNX {
    // Optional process-local fatal sink for non-interactive hosts. If absent
    // (or if it returns), preserve the normal error dialog and termination.
    extern std::function<void(const std::string&)> errorTerminateHandler;
    [[noreturn]] void ErrorTerminate(const std::string& Msg,
        const std::source_location& loc = std::source_location::current());
}
