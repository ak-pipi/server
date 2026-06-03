# C++ 游戏服务器 MQ 对接文档

> 版本: 1.0 | 日期: 2026-06-03
>
> 本文档供 `server/` (C++) 开发人员参考，明确 C++ 端需要实现和修改的功能点、消息格式和实现步骤。
> 完整的双端对接文档见 `docs/CPP_JAVA_功能对接对齐文档.md`。

---

## 目录

- [1. 通信协议约定](#1-通信协议约定)
- [2. 游戏结算入账 WalletChangeEvent（待办：更多游戏集成）](#2-游戏结算入账-walletchangeevent待办更多游戏集成)
- [3. 风控数据采集 RiskControlData（待办：各游戏集成）](#3-风控数据采集-riskcontroldata待办各游戏集成)
- [4. 管理后台房间操作 MsgCreateRoom / MsgForceDissolveRoom（待实现）](#4-管理后台房间操作-msgcreateroom--msgforcedissolveroom待实现)
- [5. 游戏规则热更新 MsgGameVersionUpdate（已实现，无需修改）](#5-游戏规则热更新-msggameversionupdate已实现无需修改)
- [6. IP 黑名单跨服务器同步（已实现，无需修改）](#6-ip-黑名单跨服务器同步已实现无需修改)
- [7. 随机审计日志 RandomAuditLog（待办：各游戏集成）](#7-随机审计日志-randomauditlog待办各游戏集成)
- [8. 新增游戏类型支持 1031-1036（已实现，无需修改）](#8-新增游戏类型支持-1031-1036已实现无需修改)
- [9. Handler 注册模式参考](#9-handler-注册模式参考)
- [附录: 完整消息类型汇总表](#附录-完整消息类型汇总表)

---

## 1. 通信协议约定

### 1.1 MQ 消息发送方（生产者）规范

无论 C++ 还是 Java，发送 MQ 消息时必须遵循以下格式：

```
1. 构建内层 JSON 对象（具体业务字段）
2. 将 JSON 字符串进行 Base64 编码，得到 msgPack
3. 构建外层信封: { "msgType": "消息类型", "msgPack": "Base64字符串" }
4. 将外层信封序列化为 JSON 字符串，发送到 RabbitMQ
```

**C++ 端**: 使用 `RabbitmqClient::publishJson(exchange, routingKey, msgType, innerJson)`
该函数内部自动完成 Base64 编码和信封封装。

### 1.2 Routing Key 规范

| 方向 | Exchange | Routing Key | 说明 |
|------|----------|-------------|------|
| web_server -> C++ 定向 | `game.direct` | `game_server_001` (目标C++服务器的server_id) | 发给特定C++服务器 |
| web_server -> C++ 广播 | `game.direct` | `web_server_001` | 广播给所有C++服务器 |
| C++ -> web_server | `game.direct` | `web_server_001` | C++回传给web_server |
| C++ -> C++ 广播 | `game.fanout` | `""` (空,fanout忽略) | 跨C++服务器广播 |

---

## 2. 游戏结算入账 WalletChangeEvent（待办：更多游戏集成）

### 2.1 功能说明

C++ 游戏服务器在每局结算时，通过 MQ 发送积分变动事件。Java web_server 消费该事件后完成玩家金币/钻石的入账操作。

### 2.2 消息格式

**MQ 信封**:
- `msgType`: `"WalletChangeEvent"`
- Exchange: `game.direct`
- Routing Key: `web_server_001`

**内层 JSON 字段** (msgPack 解码后):

| 字段名 | 类型 | 必填 | 说明 | 示例值 |
|--------|------|------|------|--------|
| `user_id` | string | 是 | 玩家ID | `"100086"` |
| `wallet_type` | string | 是 | 钱包类型 | `"gold"` |
| `change_amount` | int64 | 是 | 变动金额（正数） | `100` |
| `event_type` | string | 是 | 事件类型 | `"GAME_WIN"` / `"GAME_LOSE"` / `"ROOM_FEE"` |
| `biz_type` | string | 是 | 业务类型 | `"game_mahjong"` / `"room_fee"` |
| `biz_id` | string | 是 | 业务ID(场地ID) | `"abc1234567"` |
| `remark` | string | 否 | 备注 | `"赢牌得分"` / `"房费消耗"` |

**示例**:
```json
{
    "user_id": "100086",
    "wallet_type": "gold",
    "change_amount": 100,
    "event_type": "GAME_WIN",
    "biz_type": "game_pao_de_kuai",
    "biz_id": "venue_abc123",
    "remark": "跑得快赢牌得分"
}
```

### 2.3 C++ 端（已部分实现）

已在以下游戏中集成 `WalletEventTask::publish()` 调用：

| 游戏 | 文件 | 行号 |
|------|------|------|
| 跑得快 | `Server/PaoDeKuai/PaoDeKuaiRoom.cpp` | ~446, 451 |
| 长沙麻将 | `Server/ChangShaMahjong/ChangShaMahjongRoom.cpp` | ~739, 744 |
| 益阳歪胡子 | `Server/YiYangWaiHuZi/YiYangWaiHuZiRoom.cpp` | ~442, 443 |
| 沅江千分 | `Server/YuanJiangQianFen/YuanJiangQianFenRoom.cpp` | ~444, 445 |
| 通用房费 | `Framework/Game/WalletEventTask::publishRoomFee()` | ~32-38 |

### 2.4 待办：在其余游戏中集成

需在以下游戏的结算逻辑中增加 `WalletEventTask::publish()` 调用：
- 麻将 (1021)
- 百人牛牛 (1023)
- 六安比鸡 (1027)
- 逮狗腿 (1028)
- 掼蛋 (1030)
- 桃江麻将 (1031)
- 红中麻将 (1032)

### 2.5 C++ 调用示例

```cpp
// 玩家赢牌
WalletEventTask::publish(playerId, "GAME_WIN", winAmount, "game_guan_dan", venueId, "掼蛋赢牌得分");

// 玩家输牌
WalletEventTask::publish(playerId, "GAME_LOSE", loseAmount, "game_guan_dan", venueId, "掼蛋输牌扣分");

// 房费
WalletEventTask::publishRoomFee(playerId, feeAmount, venueId, exchange, routingKey);
```

---

## 3. 风控数据采集 RiskControlData（待办：各游戏集成）

### 3.1 功能说明

C++ 游戏服务器每局结束后采集风控数据（同桌关系、IP、设备ID、操作耗时、随机种子），通过 MQ 推送给 Java web_server，由风控引擎进行分析。

### 3.2 消息格式

**MQ 信封**:
- `msgType`: `"RiskControlData"`
- Exchange: `game.direct`
- Routing Key: `web_server_001`

**内层 JSON 字段**:

| 字段名 | 类型 | 说明 |
|--------|------|------|
| `eventType` | string | 固定值 `"RiskControlData"` |
| `venueId` | string | 场地ID |
| `gameType` | int | 游戏类型ID |
| `roundNo` | int | 局号 |
| `startTime` | int64 | 局开始时间(毫秒时间戳) |
| `endTime` | int64 | 局结束时间(毫秒时间戳) |
| `playerCount` | int | 玩家数量 |
| `randomSeedHash` | string | 随机种子hash |
| `players` | array | 玩家风控数据数组 |

**players 数组元素**:

| 字段名 | 类型 | 说明 |
|--------|------|------|
| `playerId` | string | 玩家ID |
| `ipAddress` | string | IP地址 |
| `deviceId` | string | 设备ID |
| `seat` | int | 座位号 |
| `score` | int | 当局得分 |
| `winGold` | int64 | 当局赢得金币 |
| `escaped` | bool | 是否逃跑 |
| `operationTimes` | array[int] | 操作耗时列表(毫秒) |

**示例**:
```json
{
    "eventType": "RiskControlData",
    "venueId": "venue_abc123",
    "gameType": 1028,
    "roundNo": 5,
    "startTime": 1717382400000,
    "endTime": 1717382520000,
    "playerCount": 4,
    "randomSeedHash": "a1b2c3d4",
    "players": [
        {
            "playerId": "100001",
            "ipAddress": "192.168.1.100",
            "deviceId": "device_abc",
            "seat": 0,
            "score": 10,
            "winGold": 500,
            "escaped": false,
            "operationTimes": [1200, 800, 3500, 2100]
        }
    ]
}
```

### 3.3 C++ 端（框架已实现，各游戏待集成）

`RiskControlCollector` 已在 `Framework/Game/RiskControlCollector.cpp` 中实现，通过 `finishRound()` 发布数据。

### 3.4 待办：在各游戏 Room 子类中集成

在各游戏 Room 子类中集成 `RiskControlCollector` 调用：
1. 在 `startRound()` 时调用 `collector.startRound(venueId, gameType, roundNo)`
2. 在玩家入座时调用 `collector.recordPlayer(playerId, seat, ip, deviceId)`
3. 在玩家操作时调用 `collector.recordOperation(playerId, elapsedMs)`
4. 在结算时调用 `collector.recordScore(playerId, score, winGold)`
5. 在逃跑时调用 `collector.recordEscape(playerId)`
6. 在发牌时调用 `collector.setRandomSeedHash(hash)`
7. 在结束时调用 `collector.finishRound()` 发布到 MQ

### 3.5 C++ 集成示例

```cpp
// 在 GameRoom 子类中添加成员
RiskControlCollector _riskCollector;

// 局开始时
_riskCollector.startRound(getId(), GameType::Lackey, _roundNo);

// 玩家入座时
Avatar::Ptr avatar = getAvatar(seat);
if (avatar) {
    _riskCollector.recordPlayer(avatar->getPlayerId(), seat,
        avatar->getIpAddress(), avatar->getDeviceId());
}

// 玩家操作时
_riskCollector.recordOperation(playerId, elapsedMs);

// 结算时
_riskCollector.recordScore(playerId, score, winGold);

// 局结束时
_riskCollector.finishRound();
```

---

## 4. 管理后台房间操作 MsgCreateRoom / MsgForceDissolveRoom（待实现）

### 4.1 功能说明

Java 管理后台通过 MQ 指令让 C++ 服务器创建或强制解散游戏房间。这是管理员操作功能。Java 端发送逻辑已实现，C++ 端需要新增 MQ 消息处理器。

### 4.2 MsgCreateRoom 消息格式

**方向**: Java web_server -> C++ Server
**Exchange**: `game.direct`
**Routing Key**: `web_server_001` (广播到所有 C++ 服务器，由目标服务器处理)

**内层 JSON 字段**:

| 字段名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `roomId` | string | 是 | 房间ID |
| `roomNo` | string | 是 | 房间号(6位数字) |
| `gameId` | string | 是 | 游戏类型ID (如 `"1021"`) |
| `districtId` | string | 否 | 赛区ID，如果是赛区房间 |
| `configSnapshot` | string | 是 | 房间配置JSON快照 |

### 4.3 MsgForceDissolveRoom 消息格式

**方向**: Java web_server -> C++ Server
**Exchange**: `game.direct`
**Routing Key**: `web_server_001`

**内层 JSON 字段**: 与 `MsgCreateRoom` 相同。

### 4.4 实现步骤

**Step 1**: 在 `VenueManager.h` 中声明处理方法：

```cpp
// VenueManager.h 新增
private:
    void handleCreateRoom(const std::string& json);
    void handleForceDissolveRoom(const std::string& json);
```

**Step 2**: 在 `VenueManager.cpp` 中实现处理逻辑：

```cpp
// VenueManager.cpp 新增

void VenueManager::handleCreateRoom(const std::string& json) {
    std::stringstream ss(json);
    Json::Value obj;
    ss >> obj;

    std::string roomId  = obj["roomId"].asString();
    std::string roomNo  = obj["roomNo"].asString();
    std::string gameId  = obj["gameId"].asString();
    std::string districtId = obj.get("districtId", "").asString();
    std::string configSnapshot = obj["configSnapshot"].asString();

    int gameType = std::atoi(gameId.c_str());

    // 查找对应的 VenueLoader
    VenueLoader::Ptr loader = getLoader(gameType);
    if (!loader) {
        LOG_ERROR("MsgCreateRoom: 未找到游戏类型 " + gameId + " 的加载器");
        return;
    }

    // 通过 loader 创建场地
    Venue::Ptr venue = loader->load(roomId, configSnapshot);
    if (!venue) {
        LOG_ERROR("MsgCreateRoom: 创建场地失败, roomId=" + roomId);
        return;
    }

    // 将场地分配到内部处理器
    assignVenue(venue);

    LOG_INFO("MsgCreateRoom: 场地创建成功, roomId=" + roomId + ", gameType=" + gameId);
}

void VenueManager::handleForceDissolveRoom(const std::string& json) {
    std::stringstream ss(json);
    Json::Value obj;
    ss >> obj;

    std::string roomId = obj["roomId"].asString();

    Venue::Ptr venue = getVenue(roomId);
    if (!venue) {
        LOG_WARN("MsgForceDissolveRoom: 场地不存在, roomId=" + roomId);
        return;
    }

    // 调用场地的强制解散方法
    std::string errMsg;
    int ret = venue->forceDissolve(errMsg);
    if (ret != 0) {
        LOG_ERROR("MsgForceDissolveRoom: 解散失败, roomId=" + roomId + ", err=" + errMsg);
    } else {
        LOG_INFO("MsgForceDissolveRoom: 解散成功, roomId=" + roomId);
    }
}
```

**Step 3**: 在 `VenueManager::init()` 中注册新的 Handler（与 `LeaveVenueHandler` 同级）：

```cpp
// VenueManager::init() 中新增

// --- MsgCreateRoom 处理器 ---
class CreateRoomHandler : public RabbitmqMessageJsonHandler {
public:
    CreateRoomHandler(const std::string& tag)
        : RabbitmqMessageJsonHandler(tag) {}
    virtual ~CreateRoomHandler() {}
protected:
    virtual bool receive(const std::string& message) override {
        return (message.find("MsgCreateRoom") != std::string::npos);
    }
    virtual void handleImpl(const std::string& msgType, const std::string& json) override {
        VenueManager::getSingleton().handleCreateRoom(json);
    }
};
{
    RabbitmqMessageHandler::Ptr handler(new CreateRoomHandler(consumerTag));
    RabbitmqConsumer::getSingleton().addHandler(handler);
}

// --- MsgForceDissolveRoom 处理器 ---
class ForceDissolveRoomHandler : public RabbitmqMessageJsonHandler {
public:
    ForceDissolveRoomHandler(const std::string& tag)
        : RabbitmqMessageJsonHandler(tag) {}
    virtual ~ForceDissolveRoomHandler() {}
protected:
    virtual bool receive(const std::string& message) override {
        return (message.find("MsgForceDissolveRoom") != std::string::npos);
    }
    virtual void handleImpl(const std::string& msgType, const std::string& json) override {
        VenueManager::getSingleton().handleForceDissolveRoom(json);
    }
};
{
    RabbitmqMessageHandler::Ptr handler(new ForceDissolveRoomHandler(consumerTag));
    RabbitmqConsumer::getSingleton().addHandler(handler);
}
```

**Step 4**: 确保 `Venue` 基类有 `forceDissolve()` 方法，如果没有则需要新增。

### 4.5 注意事项

- `MsgCreateRoom` 是管理后台功能，C++ 端需要能从 `configSnapshot` 解析出游戏配置并恢复场地状态。
- 如果目标房间不在当前 C++ 服务器上（venueId 没有映射到本机的 server_id），则忽略该消息。
- `MsgForceDissolveRoom` 需要先检查 `venue_server_map:<roomId>` 确认房间在本机。

---

## 5. 游戏规则热更新 MsgGameVersionUpdate（已实现，无需修改）

### 5.1 功能说明

Java 管理后台发布/回滚游戏规则版本时，通过 MQ 通知所有 C++ 游戏服务器加载新配置。

### 5.2 C++ 端期望的 `MsgGameVersionUpdate` 消息格式

**内层 JSON 字段**:

| 字段名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `current_version` | string | 是 | 当前游戏引擎版本号 |
| `min_compatible_version` | string | 是 | 最低兼容版本号 |
| `gray_percent` | int | 否 | 灰度发布百分比(0-100) |
| `gray_version` | string | 否 | 灰度版本号 |
| `gray_player_ids` | string | 否 | 灰度玩家ID列表(逗号分隔) |

**示例**:
```json
{
    "current_version": "1.2.0",
    "min_compatible_version": "1.0.0",
    "gray_percent": 10,
    "gray_version": "1.3.0-beta",
    "gray_player_ids": "100001,100002,100003"
}
```

### 5.3 C++ 端（已实现）

`VersionManager` 已在 `Framework/Game/VersionManager.cpp:54-73` 中注册了 `MsgGameVersionUpdate` 的处理器。收到消息后会：
1. 解析 JSON 字段
2. 更新内存中的版本配置
3. 持久化到 Redis `game_engine_version_config` hash
4. 客户端下次连接时获取新版本号

> **注意**: Java 端之前发送的 msgType 是 `MsgLoadGameRule`，与 C++ 端不匹配。Java 端将修改为发送 `MsgGameVersionUpdate`。C++ 端无需任何修改。

---

## 6. IP 黑名单跨服务器同步（已实现，无需修改）

### 6.1 功能说明

当某个 C++ 服务器检测到异常行为（3秒内超过20次异常请求），自动将该 IP 加入本地黑名单，并通过 MQ 广播给其他 C++ 服务器。

### 6.2 消息格式

**Exchange**: `game.fanout` (广播)
**Routing Key**: `""` (空，fanout 忽略)

#### MsgIpBlacklistAdd

| 字段名 | 类型 | 说明 |
|--------|------|------|
| `remoteIp` | string | 被加入黑名单的IP地址 |
| `timestamp` | int64 | 加入时间(Unix秒) |

#### MsgIpBlacklistRemove

| 字段名 | 类型 | 说明 |
|--------|------|------|
| `remoteIp` | string | 被移出黑名单的IP地址 |

### 6.3 C++ 端（已实现）

`SecurityManager` 已实现：
- `abnormalBehavior()` 检测异常并广播 `MsgIpBlacklistAdd`
- `checkBlacklist()` 过期清理并广播 `MsgIpBlacklistRemove`
- `handleMessage()` 处理来自其他 C++ 服务器的广播

黑名单有效期: 5 分钟（300秒）
触发阈值: 3 秒内超过 19 次异常行为

---

## 7. 随机审计日志 RandomAuditLog（待办：各游戏集成）

### 7.1 功能说明

C++ 游戏服务器每局发牌时记录随机种子的 hash 和发牌顺序的 hash，通过 MQ 推送给 Java web_server 存储用于合规审计。

### 7.2 消息格式

**MQ 信封**:
- `msgType`: `"RandomAuditLog"`
- Exchange: `game.direct`
- Routing Key: `web_server_001`

**内层 JSON 字段**:

| 字段名 | 类型 | 说明 |
|--------|------|------|
| `eventType` | string | 固定值 `"RandomAuditLog"` |
| `venueId` | string | 场地ID |
| `gameType` | int | 游戏类型ID |
| `roundNo` | int | 局号 |
| `banker` | int | 庄家座位号 |
| `seedHash` | string | 随机种子hash (8位hex CRC32) |
| `timestamp` | int64 | 发牌时间(毫秒时间戳) |
| `orderHash` | string | 发牌顺序hash (前10张牌ID拼接) |
| `playerIds` | array[string] | 参与玩家ID列表 |

**示例**:
```json
{
    "eventType": "RandomAuditLog",
    "venueId": "venue_abc123",
    "gameType": 1021,
    "roundNo": 3,
    "banker": 0,
    "seedHash": "a1b2c3d4",
    "timestamp": 1717382400000,
    "orderHash": "13:4,7:2,11:5,3:8,1:9",
    "playerIds": ["100001", "100002", "100003", "100004"]
}
```

### 7.3 C++ 端（框架已实现，各游戏待集成）

`RandomAuditLogger::logAudit()` 已在 `Framework/Game/RandomAuditLogger.cpp` 中实现。

### 7.4 待办：在各游戏 Room 子类的发牌逻辑中调用

```cpp
#include "Game/RandomAuditLogger.h"

// 在发牌后调用
std::vector<int> cardOrder = getDealOrder();       // 发牌顺序
std::vector<std::string> playerIds = getPlayerIds(); // 玩家列表
std::string seedHash = getRandomSeedHash();        // 种子hash

RandomAuditLogger::logAudit(
    getId(),          // venueId
    gameType,        // gameType
    _roundNo,        // roundNo
    _bankerSeat,     // banker
    seedHash,        // seedHash
    cardOrder,       // cardOrder
    playerIds        // playerIds
);
```

---

## 8. 新增游戏类型支持 1031-1036（已实现，无需修改）

C++ 端已完整实现以下 6 个新游戏，包括 Room、Avatar、Messages、Loader，且已在 `main.cpp` 中注册。

| ID | 游戏名称 |
|----|---------|
| 1031 | 桃江麻将 |
| 1032 | 红中麻将 |
| 1033 | 跑得快 |
| 1034 | 长沙麻将 |
| 1035 | 益阳歪胡子 |
| 1036 | 沅江千分 |

Java 端需要补齐对应的常量和业务逻辑（见 Java 端文档）。

---

## 9. Handler 注册模式参考

所有 MQ 消息处理器遵循统一的继承模式，新增 Handler 时请参考此模式。

### 9.1 类继承关系

```
RabbitmqMessageHandler (基类)
    └── RabbitmqMessageJsonHandler (JSON解析中间层)
          └── 具体Handler类 (如 LeaveVenueHandler)
```

### 9.2 新增 Handler 标准模板

```cpp
// 在 XxxManager::init() 中定义和注册

class MyNewHandler : public RabbitmqMessageJsonHandler {
public:
    MyNewHandler(const std::string& tag)
        : RabbitmqMessageJsonHandler(tag) {}
    virtual ~MyNewHandler() {}

protected:
    // 1. receive(): 过滤消息，只有包含特定字符串的消息才处理
    virtual bool receive(const std::string& message) override {
        return (message.find("MyNewMessageType") != std::string::npos);
    }

    // 2. handleImpl(): 处理解析后的 JSON
    virtual void handleImpl(const std::string& msgType, const std::string& json) override {
        if (msgType != "MyNewMessageType")
            return;

        // 解析 JSON
        std::stringstream ss(json);
        Json::Value obj;
        ss >> obj;

        // 提取字段
        std::string field1 = obj["field1"].asString();

        // 业务处理...

        // 可选: 发送响应
        Json::Value response(Json::objectValue);
        response["result"] = 0;
        RabbitmqClient::getSingleton().publishJson(
            _directExchange, "web_server_001", "ResponseMsgType", response.toStyledString());
    }
};

// 3. 注册到 Consumer
RabbitmqMessageHandler::Ptr handler(new MyNewHandler(consumerTag));
RabbitmqConsumer::getSingleton().addHandler(handler);
```

### 9.3 已有 Handler 注册位置

| Handler | 注册位置 | ConsumerTag |
|---------|---------|-------------|
| `LeaveVenueHandler` | `VenueManager::init()` | `directConsumerTag` |
| `VersionUpdateHandler` | `VersionManager::init()` | `directConsumerTag` |
| `BlacklistHandler` | `SecurityManager::init()` | `fanoutConsumerTag` |

---

## 附录: 完整消息类型汇总表

### C++ -> Java（C++ 发送，Java 接收）

| msgType | Exchange | RoutingKey | C++ 发送方 | 状态 |
|---------|----------|------------|-----------|------|
| `CommandResult` | game.direct | 动态(from msg) | VenueManager | **正常** |
| `WalletChangeEvent` | game.direct | web_server_001 | WalletEventTask | **断开**(Java待实现) |
| `RiskControlData` | game.direct | web_server_001 | RiskControlCollector | **断开**(Java待实现) |
| `RandomAuditLog` | game.direct | web_server_001 | RandomAuditLogger | **断开**(Java待实现) |
| `MsgIpBlacklistAdd` | game.fanout | "" | SecurityManager | **断开**(Java待实现,可选) |
| `MsgIpBlacklistRemove` | game.fanout | "" | SecurityManager | **断开**(Java待实现,可选) |

### Java -> C++（Java 发送，C++ 接收）

| msgType | Exchange | RoutingKey | Java 发送方 | C++ 接收方 | 状态 |
|---------|----------|------------|-----------|-----------|------|
| `MsgLeaveVenue` | game.direct | 动态(from Redis) | GameServiceImpl | VenueManager | **正常** |
| `MsgCreateRoom` | game.direct | web_server_001 | RoomManageServiceImpl | **待实现** | **断开** |
| `MsgForceDissolveRoom` | game.direct | web_server_001 | RoomManageServiceImpl | **待实现** | **断开** |
| `MsgGameVersionUpdate` | game.direct | web_server_001 | GameRuleVersionServiceImpl | VersionManager | **msgType不匹配**(Java待改) |

### C++ -> C++（C++ 服务器间广播）

| msgType | Exchange | RoutingKey | 发送方 | 接收方 | 状态 |
|---------|----------|------------|--------|--------|------|
| `MsgIpBlacklistAdd` | game.fanout | "" | SecurityManager | SecurityManager | **正常** |
| `MsgIpBlacklistRemove` | game.fanout | "" | SecurityManager | SecurityManager | **正常** |

---

## 附录: C++ 端实现优先级

| 优先级 | 功能 | 工作量 | 说明 |
|--------|------|--------|------|
| P0 | 在更多游戏中集成 WalletChangeEvent | 小 | 金币不入账=核心功能缺失 |
| P1 | 实现 MsgCreateRoom / MsgForceDissolveRoom 处理器 | 中 | 管理后台功能缺失 |
| P2 | 在各游戏中集成 RiskControlCollector | 小 | 风控系统数据缺失 |
| P2 | 在各游戏中集成 RandomAuditLogger | 小 | 合规审计缺失 |
