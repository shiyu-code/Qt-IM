# Qt-IM 消息协议说明

## 报文封装
- 所有消息以 JSON 封装，并采用紧凑格式（`QJsonDocument::Compact`）。
- 顶层字段：
  - `type`：`E_MSG_TYPE` 枚举值（`unit.h`）。
  - `from`：发送方用户 ID（服务器返回时为源用户）。
  - `data`：消息体对象或数组，根据 `type` 定义。

示例：
```json
{"type":64,"from":1001,"data":{"id":1001,"to":1002,"msg":"你好","type":0}}
```

## 常用类型
- 登录注册：`Register`、`Login`、`Logout`、`LoginRepeat`。
- 用户状态：`UserOnLine`、`UserOffLine`、`UpdateHeadPic`。
- 好友与群：`AddFriend`、`AddGroup`、`CreateGroup`、`GetMyFriends`、`GetMyGroups`、`RefreshFriends`、`RefreshGroups`。
- 聊天：`SendMsg`（私聊）、`SendGroupMsg`（群聊）、`SendFace`（表情）。
- 文件与图片：`SendFile`、`SendPicture`、`SendFileOk`、`GetFile`、`GetPicture`。
- 心跳保活：`Ping`、`Pong`（新增）。
- 送达确认：`Ack`（新增，0x72）。

## 字段约定
- `SendMsg/SendGroupMsg`：
  - `data.id`：发送方用户 ID（与顶层 `from` 等价）。
  - `data.to`：接收方用户 ID，或群组 ID（群聊）。
  - `data.msg`：文本消息内容。
  - `data.type`：`MessageType`（`Text/Audio/Picture/Files`）。
  - `data.msgId`：客户端生成的消息ID（int，建议毫秒时间戳裁剪），用于 ACK 匹配。
  - `data.ts`：客户端生成的发送时间戳（毫秒）。
- `UserOnLine/UserOffLine`：
  - `data.id`：变更状态的用户 ID。
  - `data.text`：`online/offline`。
- `SendFileOk`：
  - `data.to`：接收方用户 ID。
  - `data.msg`：文件名。
- `GetFile`：
  - `data.id`：发起下载的用户 ID。
  - `data.to`：文件归属用户 ID。
  - `data.msg`：文件名。
- `Ping/Pong`：
  - `data.id`：客户端用户 ID。
  - `data.ts`：时间戳（毫秒）。

## Ack（送达确认）
- 类型：`Ack = 0x72`
- 触发时机：
  - 私聊消息到达在线接收方时，服务器返回 `queued=0` 的 `Ack` 给发送方；
  - 接收方离线时，消息入队（服务器持久化至离线队列），服务器返回 `queued=1` 的 `Ack` 给发送方。
- 结构：
  - `data.to`：接收方用户 ID。
  - `data.type`：原始消息类型（如 `SendMsg`、`SendPicture`）。
  - `data.queued`：`0` 表示已转发到在线用户；`1` 表示已入离线队列。
  - `data.msg`：原始消息内容（文本/文件名等）。
  - `data.msgId`：客户端生成的消息ID；用于匹配客户端发送的具体消息（无论 `queued=0/1` 都会回传）。

## 离线消息
- 服务端会将离线私聊消息持久化到 `MSGQUEUE` 表（`fromId|toId|type|msg|ts`），并在用户登录成功后批量推送。
  - 表字段包含 `msgId`，由客户端生成并在入队时存储；便于去重与后续扩展。
- 推送格式遵循在线转发的私聊消息结构：
  ```json
  {"type":64,"from":1002,"data":{"id":1002,"to":1001,"msg":"离线期间的消息","type":0}}
  ```
- 推送完成后，服务器删除对应队列记录（确保不重复发送）。

## 行为与流转
- 私聊：客户端发送 `SendMsg`，服务器根据 `data.to` 路由给在线目标用户（`signalMsgToClient`）。
- 群聊：客户端发送 `SendGroupMsg`，服务器遍历群成员，对在线且非发送方的成员转发。
- 文件：通过文件中转服务器传输（`TCP_FILE_PORT`），完成后由 `SendFileOk` 通知对端。
- 心跳：客户端每 15s 发送 `Ping`；服务器收到后返回 `Pong`。
  - 若客户端连续 3 次未收到 `Pong`，视为连接异常并触发自动重连（指数退避，最大 30s）。
  - 在私聊消息发送场景中，客户端建议根据 `Ack` 结果做轻量提示或占位处理（当前实现为日志记录）。

## 错误与状态
- 顶层 `type` 表示服务端响应类型，`data` 内可附加：
  - `code`：`0` 成功，非零错误码（建议预留）。
  - `msg`：错误描述。
- 客户端可据此展示或重试；后续将引入 ACK 确认与重试策略。
  - 重连策略：断线或心跳超时后自动重连，退避间隔 1s、2s、4s... 直至 30s；连接成功后复位。

## 兼容性与扩展
- 保持枚举值向后兼容，新增类型应追加在尾部（如 `0x70+`）。
- 统一大小写与字段命名；避免语义不清的短字段。
- 后续将增加 `messageId`、`sessionToken`、`error` 结构。