#pragma once
#include <MulNX/MulNX.hpp>
#include <nlohmann/json.hpp>

class CS2BootLoader final : public MulNX::Module<CS2BootLoader> {
    class CS2HelperController* pHelperController = nullptr;

    fs::path gamePath;
    std::string launchOptions;

    std::vector<std::string> patternsCheckDangerous;
    bool environmentChecked = false;
    nlohmann::json launchResult;
    bool Fail(const std::string& stage, DWORD code, const std::string& message);
    bool CheckEnvironment();
    bool LaunchAndInject();

    void Window(MulNX::UICoordinator* uico);
    void ProcessMsg(MulNX::Message& msg);
    bool Init()override;
public:
    // Set before core initialization; prevents auto.yaml from launching twice.
    bool externalLaunch = false;
    nlohmann::json LaunchOnce();
};
