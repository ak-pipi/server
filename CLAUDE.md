# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

NiuMa 游戏服务器 -- 使用 C++ 开发的棋牌游戏逻辑服务器。属于更大系统的一部分，系统还包括 Java WEB 服务器、Unity3D 客户端、Cocos Creator 客户端和 Vue 后台管理前端。本服务器负责游戏逻辑（房间管理、发牌、结算），WEB 服务器负责登录认证、游戏大厅和负载均衡。

## 编译命令

**前置依赖：** CMake 3.10+、Boost（静态链接，版本 < 1.86）、rabbitmq-c、mysql-concpp（MySQL Connector/C++ 9.x）、OpenSSL。运行时依赖：MySQL 8.0+、Redis、RabbitMQ。

```bash
# 编译（项目根目录）
mkdir -p build && cd build
cmake ..
make

# 部署
mkdir -p ../bin/log
cp Server/Server ../bin/
# 将 server.ini 配置文件复制到 bin 目录下

# 运行
cd bin
./Server
```

编译目标：`3rdpart`（hiredis、jsoncpp、zlib）、`Framework`（静态库）、`Mahjong`（静态库）、`Poker`（静态库）、`Server`（可执行文件）。项目中无测试框架。

## 架构

所有代码位于 `NiuMa` 命名空间下，使用 C++11 标准。

### 核心线程模型：Venue + Handler

核心设计是 **Venue（场地）** 模式。一个 Venue 代表一张游戏桌（如麻将桌）。每个 Venue 被分配给唯一的 `VenueInnerHandler`，该 Handler 在单一线程中运行并拥有独立的消息队列。因此，一个 Venue 的所有游戏逻辑都在同一线程内执行，无需加锁或多线程同步。

- **外层线程池**（`VenueOuterHandler`）：多个 Handler 共享同一个消息队列，处理场地外部逻辑（玩家加入/离开、连接管理）。
- **内层线程池**（`VenueInnerHandler`）：每个 Handler 拥有独立的消息队列，处理各场地类型的游戏逻辑（每种游戏房间类型在每线程中各有一个 Handler）。

消息分发：网络线程遍历 Handler 列表，拥有目标场地 ID 的 Handler 通过哈希表匹配接收消息。

### 关键类继承关系

```
Venue（抽象） -> GameRoom -> MahjongRoom -> [StandardMahjongRoom, TaoJiangMahjongRoom, HongZhongMahjongRoom, ChangShaMahjongRoom]
Venue（抽象） -> GameRoom -> [BiJiRoom, LackeyRoom, NiuNiu100Room, GuanDanRoom, PaoDeKuaiRoom, YuanJiangQianFenRoom]
Venue（抽象） -> GameRoom -> [YiYangWaiHuZiRoom]（自定义字牌，不使用 Poker/Mahjong 基类）
GameAvatar -> MahjongAvatar -> [StandardMahjongAvatar, TaoJiangMahjongAvatar, HongZhongMahjongAvatar, ChangShaMahjongAvatar]
GameAvatar -> PokerAvatar -> [PaoDeKuaiAvatar]
GameAvatar -> [YiYangWaiHuZiAvatar]（自定义字牌头像）
Session -> MsgSession（TCP）, EchoSession（调试）
MessageHandler -> PlayerSignatureHandler -> VenueInnerHandler, VenueOuterHandler
VenueLoader（抽象） -- 每种游戏类型的工厂类
```

### 支持的游戏类型

| ID | 游戏 | 目录 | 状态 |
|----|------|------|------|
| 1021 | 标准麻将 | `Server/StandardMahjong/` | 已实现 |
| 1022 | 斗地主 | - | 未实现 |
| 1023 | 百人牛牛 | `Server/NiuNiu100/` | 已实现 |
| 1027 | 六安比鸡 | `Server/BiJi/` | 已实现 |
| 1028 | 逮狗腿 | `Server/Lackey/` | 已实现 |
| 1030 | 掼蛋 | `Server/GuanDan/` | 已实现 |
| 1031 | 桃江麻将 | `Server/TaoJiangMahjong/` | 已实现（2人） |
| 1032 | 红中麻将 | `Server/HongZhongMahjong/` | 已实现（4人，红中赖子） |
| 1033 | 跑得快 | `Server/PaoDeKuai/` | 已实现（2人扑克） |
| 1034 | 长沙麻将 | `Server/ChangShaMahjong/` | 已实现（4人，起手胡/中鸟） |
| 1035 | 益阳歪胡子 | `Server/YiYangWaiHuZi/` | 已实现（3人字牌，胡息计分） |
| 1036 | 沅江千分 | `Server/YuanJiangQianFen/` | 已实现（4人积分扑克，叫分抢庄） |

### 模块说明

| 目录 | 用途 |
|------|------|
| `Framework/` | 核心框架静态库 |
| `Framework/Base/` | 配置（INI）、日志、单例模式 |
| `Framework/Network/` | TCP 服务器、WebSocket 服务器、会话（boost::asio） |
| `Framework/Message/` | 消息分发、消息处理线程池 |
| `Framework/Venue/` | 场地生命周期管理、内外部 Handler |
| `Framework/Player/` | 玩家加载、认证 |
| `Framework/Game/` | 游戏基类（GameRoom、GameAvatar、GameMessages）、积分钱包（WalletEventTask）、回放工具（ReplayUtils）、风控采集（RiskControlCollector）、随机审计（RandomAuditLogger）、版本管理（VersionManager） |
| `Framework/Thread/` | 通用线程池、线程工作器 |
| `Framework/Timer/` | 异步定时器管理 |
| `Framework/Database/` | 数据库连接池抽象 |
| `Framework/MySql/` | MySQL 连接池（mysql-concpp） |
| `Framework/Redis/` | Redis 连接池（hiredis） |
| `Framework/Rabbitmq/` | RabbitMQ 客户端/消费者 |
| `Mahjong/` | 麻将专用逻辑（牌面、动作、规则、计分、回放） |
| `Poker/` | 共用的扑克/纸牌逻辑（牌面、发牌、组合、牛牛规则、斗地主规则） |
| `Server/` | 主可执行文件 -- 入口（`main.cpp`）、各游戏房间实现、配置（`server.ini`） |

### 网络协议

支持 TCP 和 WebSocket 连接，消息使用 MessagePack 序列化，消息体采用 Base64 编码。消息携带场地 ID 字段用于路由分发。

### 添加新游戏的步骤

1. 在 `Server/GameDefines.h` 中定义游戏类型枚举
2. 在 `Server/<游戏名>/` 下实现游戏逻辑（Room、RoomHandler、Messages、Loader、Avatar、Playback、RecordTask）
3. 如需可复用 `Poker/` 或 `Mahjong/` 中的共享逻辑
4. 在 `Server/main.cpp` 中完成三项注册：
   - 注册 `VenueLoader`（`registLoader`）
   - 创建 `VenueInnerHandler` 线程（`registHandler`）
   - 注册网络消息创建器（`registMessages`）
5. CMake 通过 `GLOB` 自动发现新文件，无需修改 `CMakeLists.txt`

### 玩法配置动态加载

新增游戏支持通过 `ruleConfig` JSON 动态加载玩法规则。规则配置在 Room 构造函数中从 JSON 解析并缓存，随房间生命周期保持快照。配置项包括：底注、封顶分数、房费类型、局数、操作开关（吃/碰/杠/自摸/点炮）、庄家规则等。

### MQ 通信协议约定

所有 MQ 消息遵循统一的信封格式：

```
1. 构建内层 JSON 对象（具体业务字段）
2. 将 JSON 字符串进行 Base64 编码，得到 msgPack
3. 构建外层信封: { "msgType": "消息类型", "msgPack": "Base64字符串" }
4. 将外层信封序列化为 JSON 字符串，发送到 RabbitMQ
```

C++ 端使用 `RabbitmqClient::publishJson(exchange, routingKey, msgType, innerJson)` 发送，该函数内部自动完成 Base64 编码和信封封装。

**Exchange 与 Routing Key 规范**：

| 方向 | Exchange | Routing Key | 说明 |
|------|----------|-------------|------|
| web_server -> C++ 定向 | `game.direct` | `game_server_001` | 发给特定C++服务器 |
| web_server -> C++ 广播 | `game.direct` | `web_server_001` | 广播给所有C++服务器 |
| C++ -> web_server | `game.direct` | `web_server_001` | C++回传给web_server |
| C++ -> C++ 广播 | `game.fanout` | `""`（空） | 跨C++服务器广播 |

### MQ Handler 注册模式

所有 MQ 消息处理器遵循统一的继承模式：

```
RabbitmqMessageHandler (基类)
    └── RabbitmqMessageJsonHandler (JSON解析中间层)
          └── 具体Handler类 (如 LeaveVenueHandler)
```

新增 Handler 标准模板（在 `XxxManager::init()` 中定义和注册）：

```cpp
class MyNewHandler : public RabbitmqMessageJsonHandler {
public:
    MyNewHandler(const std::string& tag) : RabbitmqMessageJsonHandler(tag) {}
    virtual ~MyNewHandler() {}
protected:
    virtual bool receive(const std::string& message) override {
        return (message.find("MyNewMessageType") != std::string::npos);
    }
    virtual void handleImpl(const std::string& msgType, const std::string& json) override {
        // 解析 JSON 并处理业务逻辑
    }
};
RabbitmqMessageHandler::Ptr handler(new MyNewHandler(consumerTag));
RabbitmqConsumer::getSingleton().addHandler(handler);
```

已有 Handler 注册位置：

| Handler | 注册位置 | ConsumerTag |
|---------|---------|-------------|
| `LeaveVenueHandler` | `VenueManager::init()` | `directConsumerTag` |
| `VersionUpdateHandler` | `VersionManager::init()` | `directConsumerTag` |
| `BlacklistHandler` | `SecurityManager::init()` | `fanoutConsumerTag` |

### 积分钱包 MQ 对接

`Framework/Game/WalletEventTask` 提供通过 RabbitMQ 向 web_server 发送积分变动事件的工具。事件类型：`GAME_WIN`（赢牌）、`GAME_LOSE`（输牌）、`ROOM_FEE`（房费）。事件包含 user_id、wallet_type、change_amount、biz_type、biz_id、remark。使用 `RabbitmqClient::getSingleton().publishJson()` 发布。

**MQ 信封**：`msgType: "WalletChangeEvent"`，Exchange: `game.direct`，Routing Key: `web_server_001`。

**已集成游戏**：跑得快、长沙麻将、益阳歪胡子、沅江千分。

**注意**：`WalletEventTask::publishRoomFee()` 方法已定义但尚无任何调用点（需在各游戏 Room 中集成房费扣费逻辑时调用）。

**待集成游戏**：标准麻将(1021)、百人牛牛(1023)、六安比鸡(1027)、逮狗腿(1028)、掼蛋(1030)、桃江麻将(1031)、红中麻将(1032)。

调用示例：
```cpp
WalletEventTask::publish(playerId, "GAME_WIN", winAmount, "game_guan_dan", venueId, "掼蛋赢牌得分");
WalletEventTask::publish(playerId, "GAME_LOSE", loseAmount, "game_guan_dan", venueId, "掼蛋输牌扣分");
WalletEventTask::publishRoomFee(playerId, feeAmount, venueId, exchange, routingKey);
```

### 回放系统增强

- 每局记录包含：房间号、局号、庄家、玩家得分/金币变动、结算数据
- 回放数据格式：MessagePack 序列化 → zlib 压缩 → Base64 编码，存入 MySQL
- `Framework/Game/ReplayUtils` 提供随机种子 hash 生成（`generateSeedHash`，使用 CRC32）和回放数据压缩（`compressReplay`）
- 所有新游戏的回放数据包含 `randomSeedHash` 字段用于合规审计
- 数据库记录表包含 `random_seed_hash` 列

### 风控数据采集

- `Framework/Game/RiskControlCollector` 每局采集：同桌玩家组合、IP 记录、设备 ID、操作耗时
- 异常检测辅助数据：频繁同桌、固定输赢关系、同 IP 多号、异常逃跑
- **MQ 信封**：`msgType: "RiskControlData"`，Exchange: `game.direct`，Routing Key: `web_server_001`
- **已集成到所有新增游戏**：PaoDeKuai、ChangShaMahjong、YiYangWaiHuZi、YuanJiangQianFen
- **待集成游戏**：标准麻将(1021)、百人牛牛(1023)、六安比鸡(1027)、逮狗腿(1028)、掼蛋(1030)、桃江麻将(1031)、红中麻将(1032)
- 集成模式：每个 Room 持有 `_riskCollector` 成员，在 `startRound()` 中调用 `startRound()` + `recordPlayer()`，在 `saveRoundRecord()` 中调用 `setRandomSeedHash()` + `recordScore()` + `finishRound()`

### IP 黑名单跨服务器同步

`SecurityManager` 已实现 IP 黑名单检测与跨 C++ 服务器广播同步：
- `abnormalBehavior()` 检测异常（3秒内超过19次异常请求）并广播 `MsgIpBlacklistAdd`
- `checkBlacklist()` 过期清理（有效期5分钟/300秒）并广播 `MsgIpBlacklistRemove`
- `handleMessage()` 处理来自其他 C++ 服务器的广播
- Exchange: `game.fanout`（广播），Routing Key: `""`（空）
- Handler: `BlacklistHandler` 注册在 `SecurityManager::init()`，使用 `fanoutConsumerTag`
- Java web_server 可选监听用于管理后台展示和手动管理（待 Java 端实现）

### 管理后台房间操作（待实现）

Java 管理后台通过 MQ 指令让 C++ 服务器创建或强制解散游戏房间。Java 端发送逻辑已实现，C++ 端需要新增两个 MQ Handler。

**MsgCreateRoom**（方向: Java -> C++，Exchange: `game.direct`，Routing Key: `web_server_001`）：

内层 JSON 字段：`roomId`(房间ID)、`roomNo`(房间号)、`gameId`(游戏类型ID如"1021")、`districtId`(赛区ID,可选)、`configSnapshot`(房间配置JSON快照)。

**MsgForceDissolveRoom**：字段与 MsgCreateRoom 相同，Exchange/Routing Key 相同。

实现步骤：
1. 在 `VenueManager.h` 中声明 `handleCreateRoom()` / `handleForceDissolveRoom()` 方法
2. 在 `VenueManager.cpp` 中实现处理逻辑（查找 VenueLoader 创建场地 / 查找场地并调用 forceDissolve）
3. 在 `VenueManager::init()` 中注册 `CreateRoomHandler` / `ForceDissolveRoomHandler`（使用 `directConsumerTag`，与 `LeaveVenueHandler` 同级）
4. 确保 `Venue` 基类有 `forceDissolve()` 方法

注意事项：
- 需检查 `venue_server_map:<roomId>` 确认房间在本机，不在本机则忽略
- 需能从 `configSnapshot` 解析出游戏配置并恢复场地状态

### 合规随机算法审计

- 发牌使用服务端安全随机数
- `Framework/Game/RandomAuditLogger` 每局生成并记录随机种子审计日志
- **MQ 信封**：`msgType: "RandomAuditLog"`，Exchange: `game.direct`，Routing Key: `web_server_001`
- 包含：场地ID、游戏类型、局号、庄家、种子hash、发牌顺序hash、玩家ID列表
- `ReplayUtils::generateSeedHash()` 使用 CRC32 生成唯一种子hash
- **已集成到所有新增游戏**：在 `saveRoundRecord()` 中调用 `RandomAuditLogger::logAudit()`
- **待集成游戏**：标准麻将(1021)、百人牛牛(1023)、六安比鸡(1027)、逮狗腿(1028)、掼蛋(1030)、桃江麻将(1031)、红中麻将(1032)

### 跑得快（PaoDeKuai）架构

跑得快是首个使用 `Poker/` 模块的扑克类游戏，架构与麻将类游戏不同：
- 继承 `GameRoom`（不是 `MahjongRoom`），使用 `GameRoomHandler`（不是 `MahjongRoomHandler`）
- 自定义 `PaoDeKuaiRule`（继承 `PokerRule`）：牌型识别（单张/对子/三条/顺子/连对/飞机/炸弹/王炸）和比较
- 自定义 `PaoDeKuaiAvatar`（继承 `PokerAvatar`）：实现 `combineAllGenres()` 生成所有合法出牌组合
- 使用 `PokerDealer` 发牌，1副牌，2人各15张
- 自定义 `GameState` 枚举：None → Ready → Playing → Settling
- 服务端强校验：牌型合法性、手牌归属、压牌规则、首出黑桃3检查

### 长沙麻将（ChangShaMahjong）架构

长沙麻将继承 `MahjongRoom`，4人麻将，具有独特的地方规则：
- **起手胡检测**：发牌后立即检测缺一色、板板胡、大四喜、六六顺、节节高、三同、一枝花，每种类型可后台开关
- **中鸟结算**：胡牌后翻鸟牌，根据鸟牌数量和命中玩家计算翻倍，支持配置鸟数、是否翻倍、是否封顶
- 使用 `MahjongRoomHandler` 和标准麻将消息分发机制

### 益阳歪胡子（YiYangWaiHuZi）架构

益阳歪胡子是首款**自定义字牌**游戏，不继承 `Poker/` 或 `Mahjong/` 模块：
- 继承 `GameRoom`，使用 `GameRoomHandler`，自定义 `YiYangWaiHuZiAvatar`（继承 `GameAvatar`）
- 自定义牌张系统 `WaiHuZiCard`：大字/小字各10种（壹到拾），共80张，3人游戏
- **胡息计算**：碰牌（小字1息/大字3息）、偎/跑/提有更高胡息值，底息 + 特殊牌型加息
- **操作类型**：Chi（吃）、Peng（碰）、Wei（偎）、Pao（跑）、Ti（提）、Hu（胡）、Pass（过）
- 最低胡息门槛（`min_huxi`）可配置
- 使用 `GameRoomHandler` 而非 `MahjongRoomHandler`

### 沅江千分（YuanJiangQianFen）架构

沅江千分是积分扑克游戏，使用 `Poker/` 模块：
- 继承 `GameRoom`，使用 `PokerAvatar`、`PokerDealer`、`PokerRule`
- 4人游戏，使用2副牌
- **叫分/抢庄阶段**：`call_score_enabled` 可开关，`banker_rule` 支持叫分庄/随机庄/轮庄
- **计分牌系统**：后台可配置 `score_cards` JSON（牌面→分值映射），如 5→5分、10→10分、K→10分
- **流程**：准备 → 发牌 → 叫分 → 出牌 → 计分 → 判断目标分 → 单局结算 → 总结算
- 支持局数限制（`round_limit`）和目标分数（`target_score`）

### 版本管理与灰度发布

- `Framework/Game/VersionManager` 单例管理游戏引擎版本
- **MQ 接收版本更新**：web_server 通过 `MsgGameVersionUpdate` 消息下发版本配置（Exchange: `game.direct`，Routing Key: `web_server_001`）
  - 内层 JSON 字段：`current_version`、`min_compatible_version`、`gray_percent`、`gray_version`、`gray_player_ids`
  - Handler: `VersionUpdateHandler` 注册在 `VersionManager::init()`，使用 `directConsumerTag`
  - Java 端之前发送的 msgType 是 `MsgLoadGameRule`，与 C++ 端不匹配，Java 端将修改为 `MsgGameVersionUpdate`，C++ 端无需修改
- **客户端连接响应**：`MsgPlayerConnectResp` 返回当前版本（`engineVersion`）、最低兼容版本（`minVersion`）、玩家版本（`playerVersion`）、是否强制更新（`forceUpdate`）
- **灰度发布**：支持按玩家ID列表和百分比分配新版本（`_grayPercent`、`_grayPlayerIds`、`_grayVersion`）
- **Redis 持久化**：版本配置存储在 Redis `game_engine_version_config` 哈希表中，重启后自动恢复
- **版本比较**：支持语义化版本比较（major.minor.patch）

### 房间状态机

`GameRoom` 基类定义了标准化的 `RoomState` 枚举，提供统一的房间生命周期状态：
- `Waiting` → `Ready` → `Playing` → `Settling` → `Finished`（正常流程）
- `Dissolved`（投票解散）
- `Exception`（异常中断）
- 通过 `getRoomState()` / `setRoomState()` 访问
- 各游戏内部可继续使用自定义 `GameState` 枚举管理详细游戏流程，`RoomState` 作为标准化外壳

## 配置

运行时配置文件为 `Server/server.ini`，包含以下配置段：`[Server]`（服务器 ID、端口、线程数）、`[Websocket]`、`[Mysql]`、`[Redis]`、`[RabbitMQ]`。服务器启动后在 Redis 中注册自身信息，每 3 秒更新一次保活时间。

## MQ 消息类型汇总

### C++ -> Java（C++ 发送，Java 接收）

| msgType | Exchange | RoutingKey | C++ 发送方 | 状态 |
|---------|----------|------------|-----------|------|
| `CommandResult` | game.direct | 动态(from msg) | VenueManager | 正常 |
| `WalletChangeEvent` | game.direct | web_server_001 | WalletEventTask | 断开（Java待实现） |
| `RiskControlData` | game.direct | web_server_001 | RiskControlCollector | 断开（Java待实现） |
| `RandomAuditLog` | game.direct | web_server_001 | RandomAuditLogger | 断开（Java待实现） |
| `MsgIpBlacklistAdd` | game.fanout | "" | SecurityManager | 断开（Java待实现，可选） |
| `MsgIpBlacklistRemove` | game.fanout | "" | SecurityManager | 断开（Java待实现，可选） |

### Java -> C++（Java 发送，C++ 接收）

| msgType | Exchange | RoutingKey | Java 发送方 | C++ 接收方 | 状态 |
|---------|----------|------------|-----------|-----------|------|
| `MsgLeaveVenue` | game.direct | 动态(from Redis) | GameServiceImpl | VenueManager | 正常 |
| `MsgCreateRoom` | game.direct | web_server_001 | RoomManageServiceImpl | 待实现 | 断开 |
| `MsgForceDissolveRoom` | game.direct | web_server_001 | RoomManageServiceImpl | 待实现 | 断开 |
| `MsgGameVersionUpdate` | game.direct | web_server_001 | GameRuleVersionServiceImpl | VersionManager | msgType不匹配（Java待改） |

### C++ -> C++（C++ 服务器间广播）

| msgType | Exchange | RoutingKey | 发送方 | 接收方 | 状态 |
|---------|----------|------------|--------|--------|------|
| `MsgIpBlacklistAdd` | game.fanout | "" | SecurityManager | SecurityManager | 正常 |
| `MsgIpBlacklistRemove` | game.fanout | "" | SecurityManager | SecurityManager | 正常 |

## C++ 端待办优先级

| 优先级 | 功能 | 工作量 | 说明 |
|--------|------|--------|------|
| P0 | 在更多游戏中集成 WalletChangeEvent | 小 | 金币不入账=核心功能缺失 |
| P1 | 实现 MsgCreateRoom / MsgForceDissolveRoom 处理器 | 中 | 管理后台功能缺失 |
| P2 | 在各游戏中集成 RiskControlCollector | 小 | 风控系统数据缺失 |
| P2 | 在各游戏中集成 RandomAuditLogger | 小 | 合规审计缺失 |

## 第三方依赖

内置于 `3rdpart/`：hiredis、jsoncpp、zlib-1.2.11、msgpack（header-only）。外部依赖（通过 CMake `find_package`）：Boost（log、log_setup、filesystem、system）、OpenSSL、mysql-concpp、rabbitmq-c。
