# Qt-IM

一个基于 Qt 的桌面即时通讯示例项目，目标体验类似 QQ，包含独立的客户端与服务器端，支持文本聊天、系统消息、语音录制/播放、图片截取编辑、主题样式与扩展插件（如天气）。项目采用 Qt Widgets 架构与 TCP 套接字通信，并使用本地数据库进行数据持久化。

## 功能特性
- 登录与用户会话：`loginwidget` 负责账号登录与初始配置，支持系统消息提示（`systemmessagedialog`）。
- 联系人与聊天窗口：联系人/会话列表与聊天窗口由 `uipage` 目录下的组件承载（`chatwindow` 等），消息气泡与自定义控件在 `basewidget` 中实现（如 `chatbubble`、`customwidget`）。
- 文本与表情：聊天窗口支持文本消息，样式通过 `resource/qss` 管理，自定义气泡渲染在 `basewidget/chatbubble.cpp`。
- 语音消息：`media/AudioRecorder` 和 `voice` 负责录音与播放，Windows 下依赖 FMOD（需提供相应 DLL）。
- 图片截取与编辑：`pictureedit` 模块提供截图、裁剪对话框（`photoshotdialog`、`cutdialog`、`picturecutdialog`），支持聊天图片的基本编辑。
- 主题与资源管理：统一资源文件 `images.qrc`，样式位于 `resource/qss`，图标与背景在 `resource` 子目录。
- 天气插件：`weatherwidget` 提供天气信息展示（可选）。
- 数据持久化：`databasemagr` 负责客户端/服务器的数据访问与管理，默认使用 SQLite（随 Qt 提供）。
- 网络通信：客户端 `clientsocket` 通过 TCP 连接到服务器；服务器端 `tcpserver`/`clientsocket` 管理连接与消息分发。
- 心跳与自动重连：客户端定时 `Ping`，服务端返回 `Pong`。客户端在心跳丢失后自动重连（指数退避，最高 30s）。
- 离线消息与ACK：
  - 私聊消息在接收方离线时入队（服务端持久化），用户登录后批量推送；
  - 服务器在转发或入队后返回 `Ack`（含 `queued` 与可选 `msgId`），当前客户端以日志占位处理。
- Excel 支持（服务器端，可选）：`ChatServer/libexcel` 提供导入/导出能力（视场景使用）。

## 系统架构
- 客户端（`ChatClient`）：
  - UI 层：位于 `uipage` 与 `basewidget`，负责界面与交互（聊天窗、系统设置、天气、消息气泡等）。
  - 媒体与图片：`media` 与 `pictureedit` 处理语音录制/播放与图片编辑。
  - 通信层：`clientsocket` 通过 `QTcpSocket` 与服务器通信，协议为自定义文本/JSON/二进制消息（详见源码）。
  - 数据层：`databasemagr` 使用 SQLite 做本地数据存储（会话、联系人、消息等）。
- 服务器（`ChatServer`）：
  - 网络服务：`tcpserver` 基于 `QTcpServer` 监听端口，管理客户端连接与消息路由。
  - 数据层：`databasemagr` 进行服务器侧的数据读写与持久化。
  - 管理界面：`mainwindow` 提供基础的服务器管理 UI（端口配置、状态查看等）。
  - Excel 支持：`libexcel` 提供数据导入/导出（按需启用）。

## 目录结构（简要）
```
Qt-IM/
├── ChatClient/        # 客户端工程（Qt Widgets）
│   ├── basewidget/    # 自定义控件与动画、消息气泡等
│   ├── comapi/        # 客户端数据模型与公共接口
│   ├── media/         # 录音/播放（AudioRecorder, voice）
│   ├── pictureedit/   # 截图/裁剪/图片编辑对话框
│   ├── resource/      # 资源文件（qss、图片、图标、声音等）
│   ├── uipage/        # UI 页面（聊天、系统消息、设置、天气、头像等）
│   ├── clientsocket.* # 客户端网络通信
│   ├── databasemagr.* # 本地数据库管理
│   └── *.pro          # qmake 项目文件
├── ChatServer/        # 服务器工程
│   ├── tcpserver.*    # TCP 监听与连接管理
│   ├── clientsocket.* # 服务器端客户端连接封装
│   ├── databasemagr.* # 服务器侧数据库管理
│   ├── libexcel/      # Excel 导入/导出（可选）
│   └── *.pro          # qmake 项目文件
└── README.md
```

## 环境与依赖
- Qt 版本：建议 Qt 5.12+ 或 Qt 6.x（Widgets、Network、Multimedia 模块）。
- 编译器：MSVC 2019+ 或 MinGW（Windows）。
- 第三方库：FMOD（用于语音录制/播放，Windows 需 `fmod.dll`）。
- 数据库：SQLite（Qt 自带）。

## 构建与运行
### 使用 Qt Creator
1. 启动服务器：
   - 打开 `ChatServer/ChatServer.pro`。
   - 配置 Kit（Qt 版本与编译器），构建并运行。
   - 在服务器 UI 中设置监听端口（若提供），确保防火墙允许该端口。
2. 启动客户端：
   - 打开 `ChatClient/ChatClient.pro`。
   - 构建并运行。
   - 在登录界面或系统设置中配置服务器 IP 与端口，连接成功后即可开始聊天。

### 命令行构建（示例）
- 请在已配置好 Qt 的终端环境中执行：
  - 服务器：
    - `cd ChatServer`
    - `qmake`
    - Windows（MSVC）：`nmake`；Windows（MinGW）：`mingw32-make`
  - 客户端：
    - `cd ChatClient`
    - `qmake`
    - Windows（MSVC）：`nmake`；Windows（MinGW）：`mingw32-make`

## 使用说明
- 首次运行：客户端会初始化本地数据文件（由 `databasemagr` 管理）。
- 登录与连接：在登录或系统设置中填写服务器地址与端口，连接成功后进入主界面。
- 添加联系人：通过联系人/会话列表管理条目，打开聊天窗口开始会话。
- 发送消息：在聊天窗口输入文本后发送；支持系统消息提示与基本富文本/样式。
- 语音消息：使用聊天窗口中的录音功能录制并发送语音（需 FMOD 支持）。
- 图片处理：通过截图/裁剪对话框对图片进行编辑后发送。
- 主题设置：在系统设置中切换样式主题与声音提醒等选项。

## 文档
- 升级路线图：`docs/ROADMAP.md`
- 消息协议说明：`docs/PROTOCOL.md`
  - 包含心跳、自动重连、私聊消息 ACK 与离线消息队列说明。

## 下一步计划
- 启用心跳保活与自动重连，提高弱网稳定性（客户端已支持心跳与自动重连）。
- 明确协议与错误码，逐步加入消息送达确认与离线消息。
- 增强文件与图片传输体验（断点续传、缩略图、本地缓存）。

## 配置与数据
- 客户端配置：位于应用数据目录或同级目录（具体以 `databasemagr` 实现为准）。
- 资源文件：统一打包在 `images.qrc`，样式在 `resource/qss`。
- 服务器端口：在服务器 UI 或配置中设置；请确保防火墙放行。
- Excel 导入/导出（服务器端）：位于 `libexcel`，按需启用并放置依赖库。

## 开发说明
- 关键模块：
  - `basewidget`：动画堆叠窗口、消息气泡与自定义控件。
  - `uipage`：聊天窗、系统消息对话框、系统设置、天气组件、头像等。
  - `media`：录音与播放封装（`AudioRecorder`、`voice`）。
  - `pictureedit`：截图、裁剪与图片编辑相关对话框。
  - `clientsocket`/`tcpserver`：客户端与服务器的 TCP 通信核心。
  - `databasemagr`：数据库封装与管理（SQLite）。
  - `comapi`：公共模型与数据结构（如联系人、消息项）。
- 通信协议：基于 TCP 的自定义协议，具体格式与处理流程可参考 `ChatClient/clientsocket.cpp` 与 `ChatServer/tcpserver.cpp`。
- 日志与调试：默认使用 `qDebug()` 打印；可在关键路径添加日志辅助调试。

## 常见问题
- 无法连接服务器：
  - 检查服务器是否已启动并监听正确端口。
  - 确认客户端 IP/端口配置无误，防火墙是否放行。
- 语音功能不可用：
  - 确保在可执行目录部署 FMOD 相关 DLL。
  - 检查麦克风权限与设备可用性。
- 构建失败：
  - 确认 Qt 安装包含 `Network` 与 `Multimedia` 模块。
  - 为所选编译器正确配置 Qt Kit。

## 许可与贡献
- 许可：未明确声明，按仓库约定使用；如需商用请先确认许可证。
- 贡献：欢迎提 Issue 或提交 PR，一起完善与扩展功能。

Instant messaging software imitating QQ
