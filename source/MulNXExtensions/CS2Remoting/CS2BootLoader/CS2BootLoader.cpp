#include "CS2BootLoader.hpp"
#include <MulNX/Base/UI/UI.hpp>
#include <MulNX/Base/CharUtility/CharUtility.hpp>
#include <MulNXExtensions/CS2Remoting/CS2HelperController/CS2HelperController.hpp>
#include <commdlg.h>
#include <MulNXThirdParty/yaml.hpp>
#include <fstream>
#include <TlHelp32.h>

VOID CS2BootLoader::Window(MulNX::UICoordinator* uico) {
    auto w = MulNX::UI::RAIIWindow("CS2 Boot Loader");
    if (!w)return;
    uico->CallbackCall("CS2BootLoad"_hash, nullptr);
    if (!w.ShouldDraw())return;

    std::unique_lock lock(this->smutex);

    // 显示当前游戏路径
    ImGui::Text(I18n("boot.game_path.show", gamePath.string()).c_str());

    if (this->environmentChecked) {
        if (ImGui::Button(I18n("boot.launch").c_str())) {
            this->PublishAsync("CS2BootLoader/Launch"_hash);
        }
    }
    else {
        if (ImGui::Button(I18n("boot.check_env").c_str())) {
            this->environmentChecked = this->CheckEnvironment();
        }
    }


    if (ImGui::Button(I18n("boot.game_path.select").c_str())) {
        static wchar_t filePath[MAX_PATH] = {};

        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = GetActiveWindow();
        ofn.lpstrFilter = L"可执行文件\0*.exe\0所有文件\0*.*\0";
        ofn.lpstrFile = filePath;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        if (GetOpenFileNameW(&ofn)) {
            auto [msg, rp] = MulNX::Message::Create<MulNX::NetExt>("CS2BootLoader/PathUpdate"_hash);
            rp->str1 = MulNX::CharUtility::WToU8(filePath);
            this->PublishAsync(std::move(msg));
        }
    }

    ImGui::InputText(I18n("boot.launch_options").c_str(), &this->launchOptions);

    if (ImGui::Button(I18n("boot.save").c_str())) {
        this->PublishAsync("CS2BootLoader/Save"_hash);
    }
}

bool CS2BootLoader::Init() {
    this->pHelperController = this->Core->ModuleManager()->FindModule<CS2HelperController>("CS2HelperController");

    auto configPath = this->PathGet("Config");
    auto config = YAML::LoadFilePath(configPath / "config.yaml");
    this->gamePath = config["path"].as<std::string>();
    this->launchOptions = config["launchOptions"].as<std::string>();
    this->patternsCheckDangerous = config["patternsCheckDangerous"].as<std::vector<std::string>>();

    (*this)
        .SubscribeAsync("CS2BootLoader/Launch")
        .SubscribeAsync("CS2BootLoader/PathUpdate")
        .SubscribeAsync("CS2BootLoader/Save")
        ;

    auto autoCfg = YAML::LoadFilePath(configPath / "auto.yaml");
    auto autoLaunch = autoCfg["autoLaunch"].as<bool>();
#ifdef _DEBUG
    autoLaunch = true;
#endif
    if (autoLaunch && !this->externalLaunch) {
        this->PublishAsync("CS2BootLoader/Launch"_hash);
    }
    
    this->SendTask("Update", "MulNXMain", [this]()->bool {
        this->Update();
        return true;
        });

    this->SendUIRoot(this->GetName(), [this](auto uico, auto&&...) {return this->Window(uico);});

    return true;
}

void CS2BootLoader::ProcessMsg(MulNX::Message& msg) {
    std::unique_lock lock(this->smutex);
    switch (msg.type) {
    case "CS2BootLoader/Launch"_hash: {
        this->LaunchAndInject();
        break;
    }
    case "CS2BootLoader/PathUpdate"_hash: {
        auto* pNetExt = msg.asp.get<MulNX::NetExt>();
        this->gamePath = pNetExt->str1;
        break;
    }
    case "CS2BootLoader/Save"_hash: {
        auto configPath = this->PathGet("Config");
        YAML::Node config;
        config["path"] = this->gamePath.string();
        config["launchOptions"] = this->launchOptions;
        config["patternsCheckDangerous"] = this->patternsCheckDangerous;
        std::ofstream fout((configPath / "config.yaml").wstring());
        fout << config;
        fout.close();
        break;
    }
    default:break;
    }
}

bool CS2BootLoader::CheckEnvironment() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return this->Fail("environment", GetLastError(), "无法创建进程快照");
    }
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(pe32);

    if (!Process32FirstW(hSnapshot, &pe32)) {
        auto code = GetLastError();
        CloseHandle(hSnapshot);
        return this->Fail("environment", code, "Process32First失败");
    }

    do {
        std::wstring ws(pe32.szExeFile);
        std::string exeNameUtf8 = MulNX::CharUtility::WToU8(ws);

        for (const auto& pattern : patternsCheckDangerous) {
            if (exeNameUtf8.find(pattern) != std::string::npos) {
                CloseHandle(hSnapshot);
                this->launchResult["conflictPid"] = pe32.th32ProcessID;
                this->launchResult["conflictProcess"] = exeNameUtf8;
                return this->Fail("environment", 0, "请先关闭冲突程序：" + exeNameUtf8);
            }
        }
    } while (Process32NextW(hSnapshot, &pe32));

    auto code = GetLastError();
    CloseHandle(hSnapshot);
    if (code != ERROR_NO_MORE_FILES)
        return this->Fail("environment", code, "进程枚举未完成");
    return true;
}

bool CS2BootLoader::Fail(const std::string& stage, DWORD code, const std::string& message) {
    this->launchResult.update({ {"protocol", 1}, {"ok", false}, {"stage", stage},
        {"errorCode", code}, {"message", message} });
    this->LogError(std::format("{} (stage={}, error={})", message, stage, code));
    return false;
}

nlohmann::json CS2BootLoader::LaunchOnce() {
    std::unique_lock lock(this->smutex);
    this->LaunchAndInject();
    return this->launchResult;
}

bool CS2BootLoader::LaunchAndInject() {
    this->launchResult = nlohmann::json::object();
    // Every launch path, including auto.yaml and external callers, rechecks.
    if (!this->CheckEnvironment())return false;
    // 验证游戏路径
    if (gamePath.empty() || !fs::exists(gamePath)) {
        return this->Fail("path", 0, std::format("无效CS2路径: {}", gamePath.string()));
    }

    if (gamePath.filename() != "cs2.exe") {
        return this->Fail("path", 0, "指定的游戏路径不指向cs2.exe");
    }

    // 检查游戏是否已在运行
    HWND hwnd = FindWindowW(nullptr, L"Counter-Strike 2");
    if (hwnd != nullptr) {
        return this->Fail("process", 0, "CS2 已在运行，请先关闭游戏");
    }

    // 构建命令行参数
    std::wstring cmdLine = L"\"" + gamePath.wstring() + L"\"";
    cmdLine += L" -insecure ";// 强制insecure
    cmdLine += std::wstring(this->launchOptions.begin(), this->launchOptions.end());

    // 以 CREATE_SUSPENDED 方式创建游戏进程
    STARTUPINFOW si{ sizeof(si) };
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(
        nullptr,                // 命令行中已包含可执行文件路径
        cmdLine.data(),
        nullptr, nullptr, FALSE,
        CREATE_SUSPENDED,       // 挂起主线程
        nullptr, nullptr,
        &si, &pi
    );
    if (!ok) {
        return this->Fail("create", GetLastError(), "创建CS2进程失败");
    }

    if (!this->pHelperController->Remoting(pi)) {
        auto code = GetLastError();
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return this->Fail("load", code, "CS2工具初始化失败");
    }

    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(pi.hProcess, &created, &exited, &kernel, &user)) {
        auto code = GetLastError();
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return this->Fail("identity", code, "读取CS2创建时间失败");
    }

    // 恢复游戏主线程
    if (ResumeThread(pi.hThread) == static_cast<DWORD>(-1)) {
        auto code = GetLastError();
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return this->Fail("resume", code, "恢复CS2主线程失败");
    }
    auto creation = (static_cast<uint64_t>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
    this->launchResult = { {"protocol", 1}, {"ok", true}, {"stage", "resumed"},
        {"pid", pi.dwProcessId}, {"created", std::to_string(creation)} };

    // 清理句柄
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    this->LogInfo(std::format("CS2启动并注入成功: {}", gamePath.string()));
    return true;
}
