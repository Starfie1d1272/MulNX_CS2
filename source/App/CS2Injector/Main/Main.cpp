#include <MulNXExtensions/Win32Starter/Win32Starter.hpp>
#include <MulNXExtensions/MulNXController/MulNXController.hpp>
#include <MulNXExtensions/CS2Remoting/CS2Remotings.hpp>
#include <App/CS2Injector/UIDocker/UIDocker.hpp>

// Main code
int wmain(int argc, wchar_t** argv) {
    // One-shot integration mode. The caller supplies a fresh absolute result
    // file; CREATE_NEW prevents stale success or overwriting an existing file.
    HANDLE resultFile = INVALID_HANDLE_VALUE;
    if (argc != 1) {
        if (argc != 3 || std::wstring_view(argv[1]) != L"--launch-result"
            || !fs::path(argv[2]).is_absolute())return 2;
        resultFile = CreateFileW(argv[2], GENERIC_WRITE, FILE_SHARE_READ, nullptr,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (resultFile == INVALID_HANDLE_VALUE)return 2;
    }
    auto report = [&](const nlohmann::json& result) {
        auto text = result.dump();
        DWORD written = 0;
        bool ok = WriteFile(resultFile, text.data(), static_cast<DWORD>(text.size()), &written, nullptr)
            && written == text.size() && FlushFileBuffers(resultFile);
        CloseHandle(resultFile);
        resultFile = INVALID_HANDLE_VALUE;
        return ok;
    };
    try {
        // 创建核心
        auto core = MulNX::Core::Core::Create("CS2Injector");
        // 将模块句柄传递给核心，以便后续使用
        core->hMyOriginModule = GetModuleHandleW(NULL);
        // 注册所有模块
        (*core->ModuleManager())
            .CreateSystemModules()// 创建所有系统模块，这是框架运行的基础
            .CreateModule<Win32Starter>("Win32Starter")
            .CreateModule<DLLInjectHelper>("DLLInjectHelper")
            .CreateModule<CS2HelperController>("CS2HelperController")
            .CreateModule<CS2BootLoader>("CS2BootLoader")
            .CreateModule<MulNXController>("MulNXController")
            .CreateModule<UIDocker>("UIDocker")
            ;

        auto pStarter = core->ModuleManager()->FindModule<Win32Starter>("Win32Starter");
        auto pBoot = core->ModuleManager()->FindModule<CS2BootLoader>("CS2BootLoader");
        pStarter->quiet = pBoot->externalLaunch = resultFile != INVALID_HANDLE_VALUE;

        // 启动核心
        core->EntryInit(core.get());
        if (pBoot->externalLaunch) {
            auto result = pBoot->LaunchOnce();
            auto success = result.value("ok", false);
            if (!report(result))success = false;
            // Run() consumes this before returning and releases the normal
            // driver shutdown gate; the created game remains independent.
            PostQuitMessage(0);
            pStarter->Run();
            core->Driver()->WaitEnd();
            return success ? 0 : 1;
        }
        // 启动主循环
        pStarter->Run();
        core->Driver()->WaitEnd();
        return 0;
    }
    catch (std::exception& e) {
        if (argc != 1) {
            if (resultFile != INVALID_HANDLE_VALUE)
                report({ {"protocol", 1}, {"ok", false}, {"stage", "initialize"}, {"errorCode", 0}, {"message", e.what()} });
            return 1;
        }
        MulNX::ErrorTerminate("在启动时发生异常！异常描述：" + std::string(e.what()));
    }
    catch (...) {
        if (argc != 1) {
            if (resultFile != INVALID_HANDLE_VALUE)
                report({ {"protocol", 1}, {"ok", false}, {"stage", "initialize"}, {"errorCode", 0}, {"message", "未知启动异常"} });
            return 1;
        }
        MulNX::ErrorTerminate("在启动时发生未知异常！");
    }
    return 0;
}
