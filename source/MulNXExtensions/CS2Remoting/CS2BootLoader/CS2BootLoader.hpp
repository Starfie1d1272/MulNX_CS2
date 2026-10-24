#pragma once
#include <MulNX/MulNX.hpp>

class CS2BootLoader final : public MulNX::Module<CS2BootLoader> {
    class CS2HelperController* pHelperController = nullptr;

    fs::path gamePath;
    std::string launchOptions;

    std::vector<std::string> patternsCheckDangerous;
    bool environmentChecked = false;
    std::function<void(const PROCESS_INFORMATION&)> onProcessCreated;
    bool CheckEnvironment();
    bool LaunchAndInject();

    void Window(MulNX::UICoordinator* uico);
    void ProcessMsg(MulNX::Message& msg);
    bool Init()override;
public:
    // Only the explicit non-interactive entry uses these; auto.yaml and UI
    // launches keep their original behavior.
    bool externalLaunch = false;
    bool LaunchChecked(std::atomic<const char*>& stage, std::function<void(const PROCESS_INFORMATION&)> created);
};
