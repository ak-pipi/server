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

### 积分钱包 MQ 对接

`Framework/Game/WalletEventTask` 提供通过 RabbitMQ 向 web_server 发送积分变动事件的工具。事件类型：`GAME_WIN`（赢牌）、`GAME_LOSE`（输牌）、`ROOM_FEE`（房费）。事件包含 user_id、wallet_type、change_amount、biz_type、biz_id、remark。使用 `RabbitmqClient::getSingleton().publishJson()` 发布。

### 回放系统增强

- 每局记录包含：房间号、局号、庄家、玩家得分/金币变动、结算数据
- 回放数据格式：MessagePack 序列化 → zlib 压缩 → Base64 编码，存入 MySQL
- `Framework/Game/ReplayUtils` 提供随机种子 hash 生成（`generateSeedHash`，使用 CRC32）和回放数据压缩（`compressReplay`）
- 所有新游戏的回放数据包含 `randomSeedHash` 字段用于合规审计
- 数据库记录表包含 `random_seed_hash` 列

### 风控数据采集

- `Framework/Game/RiskControlCollector` 每局采集：同桌玩家组合、IP 记录、设备 ID、操作耗时
- 异常检测辅助数据：频繁同桌、固定输赢关系、同 IP 多号、异常逃跑
- 通过 MQ 将风控数据以 JSON 格式推送给 web_server（`RiskControlData` 事件类型）
- **已集成到所有新增游戏**：PaoDeKuai、ChangShaMahjong、YiYangWaiHuZi、YuanJiangQianFen
- 集成模式：每个 Room 持有 `_riskCollector` 成员，在 `startRound()` 中调用 `startRound()` + `recordPlayer()`，在 `saveRoundRecord()` 中调用 `setRandomSeedHash()` + `recordScore()` + `finishRound()`

### 合规随机算法审计

- 发牌使用服务端安全随机数
- `Framework/Game/RandomAuditLogger` 每局生成并记录随机种子审计日志
- 审计日志通过 MQ 推送给 web_server（`RandomAuditLog` 事件类型）
- 包含：场地ID、游戏类型、局号、庄家、种子hash、发牌顺序hash、玩家ID列表
- `ReplayUtils::generateSeedHash()` 使用 CRC32 生成唯一种子hash
- **已集成到所有新增游戏**：在 `saveRoundRecord()` 中调用 `RandomAuditLogger::logAudit()`

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
- **MQ 接收版本更新**：web_server 通过 `MsgGameVersionUpdate` 消息下发版本配置
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

## 第三方依赖

内置于 `3rdpart/`：hiredis、jsoncpp、zlib-1.2.11、msgpack（header-only）。外部依赖（通过 CMake `find_package`）：Boost（log、log_setup、filesystem、system）、OpenSSL、mysql-concpp、rabbitmq-c。
