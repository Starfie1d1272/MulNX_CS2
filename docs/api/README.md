# 外部启动器接入

`CS2Injector.exe --launch-result "D:\制作资料\launch-result.json"` 在不显示启动器页面的情况下执行一次官方启动流程。调用前在 `CS2BootLoader/Config/config.yaml` 配置 `path` 和 `launchOptions`；仍强制附加 `-insecure`，并在实际启动前检查冲突进程、游戏路径和已有游戏。普通无参数启动保持原界面。

结果路径必须是绝对路径，父目录已存在，文件尚不存在。调用方每次使用新路径；已有文件或参数错误返回退出码 2，不启动游戏。不要并发启动多个请求，不要在已存在的 CS2 中使用此入口。

启动器写入 UTF-8 JSON，完成正常收尾后退出。成功退出码为 0，启动失败为 1。成功示例：

```json
{"protocol":1,"ok":true,"stage":"resumed","pid":1234,"created":"134057000000000000","cleanupPending":false}
```

`created` 是 Windows 进程创建 FILETIME 的十进制字符串。调用方须重新核对 PID、创建时间及 CS2 程序路径；`resumed` 仅表示官方工具加载调用返回、游戏主线程已恢复，不表示游戏窗口、GSI、地图或游戏内 WebSocket 已就绪。退出启动器不会关闭成功启动的游戏。

失败结果包含 `ok:false`、`stage`、`errorCode`、`message` 和 `cleanupPending`；阶段包括 `initialize`、`environment`、`launch`、`identity`、`load`、`shutdown`。直接捕获的系统错误保留代码；既有流程只返回 false 或致命错误文本时，代码为 0，不将残留 GetLastError 冒充本次错误。原有详细日志保持不变。

新入口在环境检查后复用既有启动流程，不改变 `auto.yaml` 跳过环境检查的原设计或普通界面的错误处理。新入口的框架致命错误写回结果后退出，不弹出确认框；90 秒超时保护只作用于此模式。失败时仅通过本次保留的进程句柄终止并等待确认，不按名称结束其他游戏。清理未获确认时返回 `cleanupPending:true` 和 `cleanupErrorCode`；调用方不能视为已经收尾。成功结果无法写入时也执行本次游戏清理并非零退出。

调用方应等待进程退出后读取结果，拒绝空文件、不支持的协议、结果与退出码不一致和超时，不自动重试启动。需要操作系统授权或安全软件处理时，正常完成这些步骤，不绕过系统机制。

游戏内控制接口见具体模块；其入队回执不等于镜头已经执行。
