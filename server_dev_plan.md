# 棋牌游戏平台 - Game Server 开发计划与技术方案

> 版本：v1.0 | 基于 dev_plan.md + 游戏引擎详细设计.md + 数据库详细设计.md
> 适用：C++ 游戏服务器（现有代码库扩展）

---

## 一、现状分析与目标

### 1.1 现有代码库概况

| 模块 | 现状 | 完成度 |
|------|------|--------|
| 基础框架 (Framework) | 日志、单例、线程池、定时器、消息队列 | ✅ 完整 |
| 网络层 | TCP + WebSocket 双服务器、Session管理 | ✅ 完整 |
| 数据层 | MySQL Pool + Redis Pool + RabbitMQ | ✅ 完整 |
| 玩家管理 | PlayerManager、Session-Player映射 | ✅ 完整 |
| 场地管理 | Venue + VenueManager + Loader + Handler模式 | ✅ 完整 |
| 游戏实现 | 比鸡、掼蛋、逮狗腿、百人牛牛、标准麻将 | ✅ 5款完成 |
| **缺失游戏** | **桃江麻将、红中麻将、跑得快、长沙麻将、益阳歪胡子、沅江千分** | ❌ 未实现 |
| **MongoDB** | 回放数据存储 | ❌ 未集成 |
| **游戏引擎框架** | 公共组件（SecureRandom、ActionQueue等） | ⚠️ 部分实现 |
| **数据库表** | 按29张表设计 | ❌ 需新建 |

### 1.2 技术栈确认

```
语言: C++11
编译器: MSVC (Windows) / Clang (macOS/Linux)
构建: CMake 3.10+

第三方库:
  - Boost 1.70+ (log, filesystem, system, asio)
  - OpenSSL (Token校验、加密)
  - mysql-connector-cpp 8.0+ (MySQL 8.0)
  - redis-plus-plus (Redis 6.0+)
  - mongo-cxx-driver (MongoDB 6.0+)  ← 新增
  - rabbitmq-c (RabbitMQ)
  - msgpack-c (序列化)
  - jsoncpp / nlohmann_json (JSON解析)  ← 新增（WebSocket协议用）
```

---

## 二、开发阶段划分（对应 dev_plan.md 五阶段）

### 阶段一：基础平台重构（8-10周）→ 对应文档 9.1

#### 任务 1.1：数据库表结构初始化

**目标**：按 `数据库详细设计.md` 创建29张核心表

**实施方案**：

```sql
-- 执行顺序（按外键依赖）
1. games                    -- 游戏定义（6款游戏初始数据）
2. game_rule_versions      -- 规则版本
3. admin_roles            -- 角色（7个初始角色）
4. admin_permissions     -- 权限节点（树形）
5. admin_role_permissions -- 角色-权限关联
6. admin_users            -- 管理员
7. users                   -- 用户主表
8. user_devices           -- 设备绑定
9. wallets                -- 钱包余额
10. safeboxes             -- 保险箱
11. rooms                 -- 房间
12. room_players         -- 房间玩家
13. game_rounds          -- 牌局
14. round_actions         -- 牌局操作记录
15. room_fee_ledgers     -- 房费流水
16. wallet_ledgers_YYYYMM -- 积分流水（月分表，需自动建表脚本）
17. activities            -- 活动
18. activity_records      -- 活动记录
19. promotion_channels    -- 推广渠道
20. risk_rules            -- 风控规则
21. risk_events          -- 风控事件
22. customer_tickets      -- 客服工单
23. customer_ticket_replies -- 工单回复
24. app_versions         -- App版本
25. app_hotfixes         -- 热更新包
26. bug_tickets          -- Bug工单
27. audio_resources      -- 音效资源
28. vfx_resources       -- 特效资源
29. announcements        -- 公告
30. file_resources       -- 文件资源
31. system_configs       -- 系统配置
32. admin_audit_logs     -- 审计日志（按月分区）
```

**C++ 实现**：创建 `Framework/MySql/MySqlInitializer.h/cpp`
```cpp
class MysqlInitializer {
public:
    // 初始化数据库表结构
    bool initTables();
    
    // 插入初始数据（6款游戏 + 7角色 + 9风控规则）
    bool initSeedData();
    
    // 创建下月流水表（Cron定时任务调用）
    bool createNextMonthLedgerTable(int year, int month);
};
```

**交付物**：
- [ ] `Framework/MySql/MySqlInitializer.h/cpp`
- [ ] `scripts/init_tables.sql`（29张表DDL）
- [ ] `scripts/seed_data.sql`（初始数据）
- [ ] `scripts/create_ledger_table.sql`（月分表模板）

---

#### 任务 1.2：MongoDB 集成

**目标**：为回放数据添加 MongoDB 支持

**实施方案**：

1. 添加 `mongo-cxx-driver` 依赖（修改 `CMakeLists.txt`）
2. 创建 `Framework/MongoDB/MongoDBPool.h/cpp`（连接池，类似 RedisPool）
3. 实现 `game_replays` 集合的 CRUD

**数据结构**（对应 `数据库详细设计.md` 四）：

```cpp
// game_replays 文档结构
struct GameReplay {
    int64_t round_id;       // MySQL game_rounds.id
    int64_t room_id;
    std::string game_code;
    
    // 发牌信息
    struct {
        std::string seed_hash;
        int dealer_seat;
        std::vector<std::pair<int, std::vector<std::string>>> initial_tiles; // seat -> tiles
    } deal_info;
    
    // 操作流
    struct Action {
        int step;
        std::chrono::system_clock::time_point timestamp;
        int elapsed_ms;
        int seat;
        std::string player_id;
        std::string action;  // DRAW/DISCARD/CHI/PENG/GANG/HU/...
        nlohmann::json data;
        nlohmann::json state_snapshot;
    };
    std::vector<Action> actions;
    
    // 结算
    struct {
        std::vector<struct PlayerResult> players;
        std::string end_reason;
    } settlement;
    
    std::chrono::system_clock::time_point created_at;
};
```

**C++ 实现**：

```cpp
// Framework/MongoDB/MongoDBPool.h
class MongoDBPool : public Singleton<MongoDBPool> {
public:
    bool start(const std::string& uri, const std::string& db_name, 
               int keep_connections, int max_connections);
    void stop();
    
    // 插入回放数据
    bool insertReplay(const GameReplay& replay, std::string& replay_id);
    
    // 查询回放
    bool getReplay(const std::string& replay_id, GameReplay& replay);
    bool getReplaysByRoom(int64_t room_id, std::vector<GameReplay>& replays);
    
    // 创建索引
    bool createIndexes();
};
```

**交付物**：
- [ ] `3rdpart/mongo-cxx-driver` (或系统安装)
- [ ] `Framework/MongoDB/MongoDBPool.h/cpp`
- [ ] `Framework/MongoDB/MongoDBReplay.h/cpp`

---

#### 任务 1.3：JSON 解析库集成（WebSocket 协议用）

**目标**：支持 JSON 格式的 WebSocket 协议（对应 `WebSocket协议设计.md`）

**选型**：`nlohmann/json` (header-only, 易集成)

**修改**：
1. 下载 `json.hpp` 到 `3rdpart/json/`
2. 修改 `CMakeLists.txt` 添加 `include_directories(3rdpart/json)`
3. 修改 `Framework/Network/WebsocketServer.h/cpp` 支持 JSON 消息解析
4. 修改 `Framework/Message/NetMessage.h/cpp` 支持 JSON 打包/解包

**协议适配**：

```cpp
// WebSocket 协议格式（对应文档）
{
  "type": "game.action",
  "seq": 100,
  "ts": 1700000000000,
  "data": { ... }
}

// C++ 消息基类改为支持 JSON
class MsgBase {
public:
    virtual std::string packJson() const = 0;  // 新增：JSON打包
    virtual bool unpackJson(const std::string& json) = 0;  // 新增：JSON解包
    
    // 保留原有 msgpack 打包（兼容旧协议）
    virtual std::shared_ptr<std::string> pack() const = 0;
};
```

**交付物**：
- [ ] `3rdpart/json/json.hpp`
- [ ] `Framework/Message/JsonMessage.h/cpp`（JSON消息基类）
- [ ] 修改 `WebsocketServer` 支持 JSON 消息

---

#### 任务 1.4：用户服务 (User Service)

**目标**：实现用户注册、登录、设备管理

**数据库表**：`users`, `user_devices`

**C++ 实现**：

```cpp
// Server/User/UserManager.h
class UserManager : public Singleton<UserManager> {
public:
    // 手机号注册
    bool registerByPhone(const std::string& phone, const std::string& password, 
                          std::string& user_id, std::string& token);
    
    // 手机号登录
    bool loginByPhone(const std::string& phone, const std::string& password,
                     std::string& user_id, std::string& token);
    
    // 游客登录
    bool loginGuest(const std::string& device_id, std::string& user_id, std::string& token);
    
    // JWT Token 生成与验证
    bool generateToken(int64_t user_id, std::string& token, std::string& refresh_token);
    bool verifyToken(const std::string& token, int64_t& user_id);
    
    // 设备绑定
    bool bindDevice(int64_t user_id, const std::string& device_id, 
                    const std::string& device_model, const std::string& os_type);
};
```

**修改现有代码**：
- `Framework/Player/PlayerManager.h/cpp` 添加 `loadPlayerFromDb()` 方法（从 `users` 表加载）
- `Framework/Network/SecurityManager.h/cpp` 添加 JWT 支持（使用 OpenSSL JWT）

**交付物**：
- [ ] `Server/User/UserManager.h/cpp`
- [ ] `Server/User/UserMessages.h/cpp`（登录、注册消息）
- [ ] 修改 `PlayerManager` 支持从 DB 加载玩家
- [ ] 修改 `SecurityManager` 支持 JWT

---

#### 任务 1.5：钱包服务 (Wallet Service)

**目标**：积分/房卡/保险箱管理 + 全流水记账

**数据库表**：`wallets`, `wallet_ledgers_YYYYMM`, `safeboxes`

**C++ 实现**：

```cpp
// Server/Wallet/WalletManager.h
class WalletManager : public Singleton<WalletManager> {
public:
    // 查询余额
    bool getBalance(int64_t user_id, const std::string& wallet_type, int64_t& balance);
    
    // 变更余额（事务 + 乐观锁 + 流水记录）
    bool changeBalance(int64_t user_id, const std::string& wallet_type,
                       int64_t change_amount, const std::string& biz_type, 
                       const std::string& biz_id, const std::string& biz_title,
                       int64_t& new_balance);
    
    // 转账（A扣钱 → B加钱，事务保证）
    bool transfer(int64_t from_user_id, int64_t to_user_id, 
                   const std::string& wallet_type, int64_t amount,
                   const std::string& biz_type, const std::string& biz_id);
    
    // 查询流水
    bool getLedgers(int64_t user_id, const std::string& wallet_type,
                      const std::string& start_date, const std::string& end_date,
                      int page, int page_size, std::vector<WalletLedger>& ledgers, 
                      int& total);
    
    // 保险箱操作
    bool safeboxIn(int64_t user_id, int64_t amount, const std::string& password);
    bool safeboxOut(int64_t user_id, int64_t amount, const std::string& password);
    bool setSafeboxPassword(int64_t user_id, const std::string& new_password);
};
```

**关键实现细节**：
- 使用 MySQL 事务（`START TRANSACTION` / `COMMIT` / `ROLLBACK`）
- 乐观锁：`UPDATE wallets SET balance=?, version=version+1 WHERE id=? AND version=?`
- 流水表按月分表：`wallet_ledgers_202601`, `wallet_ledgers_202602`, ...

**交付物**：
- [ ] `Server/Wallet/WalletManager.h/cpp`
- [ ] `Server/Wallet/WalletMessages.h/cpp`
- [ ] 定时任务：每月25号创建下月流水表

---

#### 任务 1.6：房间服务 (Room Service)

**目标**：创建/加入/离开/解散房间，匹配系统

**数据库表**：`rooms`, `room_players`, `room_fee_ledgers`

**C++ 实现**：

```cpp
// Server/Room/RoomManager.h
class RoomManager : public Singleton<RoomManager> {
public:
    // 创建房间
    bool createRoom(int64_t user_id, int game_id, int rule_version_id,
                      const nlohmann::json& config, std::string& room_no);
    
    // 加入房间
    bool joinRoom(int64_t user_id, const std::string& room_no, int& seat);
    
    // 离开房间
    bool leaveRoom(int64_t user_id, const std::string& room_no);
    
    // 解散房间（投票）
    bool initiateDissolve(int64_t user_id, const std::string& room_no);
    bool voteDissolve(int64_t user_id, const std::string& room_no, bool agree);
    
    // 匹配系统
    bool startMatch(int64_t user_id, int game_id, int player_count, 
                       const nlohmann::json& config);
    bool cancelMatch(int64_t user_id);
};
```

**修改现有代码**：
- `Framework/Venue/Venue.h/cpp` 添加 `dissolveVote()` 方法
- `Server/BiJi/BiJiRoom.h/cpp` 等现有游戏添加解散投票逻辑

**交付物**：
- [ ] `Server/Room/RoomManager.h/cpp`
- [ ] `Server/Room/RoomMessages.h/cpp`
- [ ] 修改 `Venue` 基类支持解散投票

---

#### 任务 1.7：RabbitMQ 消息定义

**目标**：定义服务器间通信消息格式（对应 dev_plan.md 微服务拆分）

**实施方案**：虽然当前是单体架构，但使用 RabbitMQ 解耦模块，为未来微服务化做准备。

**消息类型**：

```cpp
// Server/RabbitMQ/MessageTypes.h
namespace RabbitMQMessage {
    // 玩家服务 → 房间服务
    const std::string PLAYER_LOGIN = "player.login";
    const std::string PLAYER_LOGOUT = "player.logout";
    
    // 房间服务 → 游戏引擎
    const std::string ROOM_CREATED = "room.created";
    const std::string ROOM_PLAYER_JOINED = "room.player_joined";
    
    // 游戏引擎 → 结算服务
    const std::string GAME_ROUND_FINISHED = "game.round_finished";
    
    // 结算服务 → 钱包服务
    const std::string SETTLEMENT_APPLY = "settlement.apply";
    
    // 风控服务 ← 所有服务
    const std::string RISK_EVENT = "risk.event";
}
```

**交付物**：
- [ ] `Server/RabbitMQ/MessageTypes.h`
- [ ] `Server/RabbitMQ/MessagePublisher.h/cpp`
- [ ] `Server/RabbitMQ/MessageConsumer.h/cpp`

---

### 阶段二：游戏引擎框架 + 首批游戏（10-14周）→ 对应文档 9.2

#### 任务 2.1：游戏引擎框架（公共组件）

**目标**：实现 `游戏引擎详细设计.md` 中的8个公共组件

**2.1.1 SecureRandomManager — 安全随机数管理器**

```cpp
// Framework/Game/SecureRandomManager.h
class SecureRandomManager : public Singleton<SecureRandomManager> {
public:
    // 生成安全随机种子（64字节）
    bool generateSeed(std::vector<unsigned char>& seed, std::string& seed_hash, 
                       std::string& seed_encrypted);
    
    // 使用种子洗牌
    template<typename T>
    void shuffle(std::vector<T>& array, const std::vector<unsigned char>& seed);
    
    // AES-GCM 加密/解密 seed
    bool encryptSeed(const std::vector<unsigned char>& seed, std::string& encrypted);
    bool decryptSeed(const std::string& encrypted, std::vector<unsigned char>& seed);
};
```

**修改现有代码**：
- `Server/Poker/PokerDealer.h/cpp` 修改为使用 `SecureRandomManager`

**2.1.2 SeatManager — 座位管理**

```cpp
// Framework/Game/SeatManager.h
class SeatManager {
public:
    SeatManager(int total_seats);
    
    // 分配座位（房主=0号座，后续按加入顺序分配）
    int assignSeat(const std::vector<int>& occupied_seats);
    
    // 座位方位映射（0=东/庄, 1=南, 2=西, 3=北）
    std::string getSeatDirection(int seat);
    
    // 下家/对家/上家计算
    int getNextSeat(int current_seat);
    int getOppositeSeat(int seat);
    int getPreviousSeat(int current_seat);
    
    // 庄家轮转规则
    int rotateDealer(int current_dealer, const std::string& rule, int winner_seat);
};
```

**2.1.3 TurnTimer — 回合计时器**

```cpp
// Framework/Game/TurnTimer.h
class TurnTimer {
public:
    TurnTimer(int base_seconds = 15, int max_seconds = 30);
    
    // 开始计时
    void start(int seat);
    
    // 取消计时（收到合法操作）
    void cancel(int seat);
    
    // 超时回调
    void setOnTimeoutCallback(std::function<void(int seat)> callback);
    
    // 定时检查（在 GameRoom::onTimer() 中调用）
    void checkTimeout();
    
private:
    int _base_seconds;
    int _max_seconds;
    std::unordered_map<int, std::chrono::steady_clock::time_point> _deadlines;
    std::function<void(int)> _on_timeout;
};
```

**2.1.4 ActionQueue — 操作队列**

```cpp
// Framework/Game/ActionQueue.h
enum class ActionPriority : int {
    P0_HU = 0,        // 胡牌（必须立即处理）
    P1_GANG = 1,      // 杠
    P2_PENG = 2,      // 碰/跑/提/偎
    P3_CHI = 3,        // 吃
    P4_DISCARD = 4,   // 出牌/过
};

struct QueuedAction {
    int seat;
    ActionPriority priority;
    nlohmann::json action_data;
    std::chrono::steady_clock::time_point enqueue_time;
};

class ActionQueue {
public:
    // 入队
    void enqueue(int seat, ActionPriority priority, const nlohmann::json& action);
    
    // 出队（按优先级 + 座位顺序）
    bool dequeue(QueuedAction& action);
    
    // 清空
    void clear();
    
    // 获取队列长度
    size_t size();
};
```

**2.1.5 ScoreLedgerWriter — 积分流水写入器**

```cpp
// Framework/Game/ScoreLedgerWriter.h
struct ScoreChange {
    int64_t user_id;
    std::string wallet_type;
    int64_t change_amount;  // 正增负减
    int64_t balance_before;
    int64_t balance_after;
    std::string biz_type;
    std::string biz_id;
    std::string biz_title;
    int64_t ref_ledger_id;  // 关联流水（A输B赢）
};

class ScoreLedgerWriter {
public:
    // 批量写入（事务保证）
    bool writeBatch(const std::vector<ScoreChange>& changes);
    
    // 冻结/解冻
    bool freeze(int64_t user_id, const std::string& wallet_type, int64_t amount);
    bool unfreeze(int64_t user_id, const std::string& wallet_type, int64_t amount);
};
```

**2.1.6 DisconnectHandler — 断线重连处理器**

```cpp
// Framework/Game/DisconnectHandler.h
struct DisconnectSnapshot {
    std::string room_no;
    int seat;
    int64_t last_seq_received;
    nlohmann::json game_state_snapshot;
    std::chrono::system_clock::time_point disconnected_at;
};

class DisconnectHandler {
public:
    // 记录断线
    void onDisconnect(int64_t user_id, const nlohmann::json& snapshot);
    
    // 重连恢复
    bool onReconnect(int64_t user_id, nlohmann::json& missed_messages, 
                        nlohmann::json& current_state);
    
    // 定时清理（超过30s的断开记录）
    void checkTimeout();
    
    // 自动托管
    void autoPlay(int64_t user_id);
};
```

**2.1.7 DissolveVoter — 解散投票管理器**

（部分已在任务1.6实现，这里完善）

**2.1.8 ReplayBuilder — 回放数据构建器**

```cpp
// Framework/Game/ReplayBuilder.h
class ReplayBuilder {
public:
    // 开始记录
    void start(int64_t round_id, int64_t room_id, const std::string& game_code);
    
    // 记录发牌
    void recordDeal(const std::string& seed_hash, int dealer_seat, 
                       const std::vector<std::pair<int, std::vector<std::string>>>& initial_tiles);
    
    // 记录操作
    void recordAction(int step, int seat, const std::string& action_type, 
                        const nlohmann::json& action_data, int elapsed_ms);
    
    // 记录结算
    void recordSettlement(const nlohmann::json& settlement);
    
    // 完成并持久化到 MongoDB
    bool finish(std::string& replay_id);
};
```

**交付物**：
- [ ] `Framework/Game/SecureRandomManager.h/cpp`
- [ ] `Framework/Game/SeatManager.h/cpp`
- [ ] `Framework/Game/TurnTimer.h/cpp`
- [ ] `Framework/Game/ActionQueue.h/cpp`
- [ ] `Framework/Game/ScoreLedgerWriter.h/cpp`
- [ ] `Framework/Game/DisconnectHandler.h/cpp`
- [ ] `Framework/Game/DissolveVoter.h/cpp`（完善）
- [ ] `Framework/Game/ReplayBuilder.h/cpp`

---

#### 任务 2.2：红中麻将插件

**目标**：实现红中麻将游戏逻辑（服务端 + 协议）

**数据库表**：`game_rule_versions`（红中麻将规则配置JSON）

**核心算法**（对应 `游戏引擎详细设计.md` 3.2）：

1. **LaiziCalculator** — 赖子计算器
2. **HuChecker** — 胡牌判断（增强版，支持赖子）
3. **TingCalculator** — 听牌计算器
4. **GangCalculator** — 杠分计算
5. **SettlementCalculator** — 结算

**文件结构**（参照现有 `StandardMahjong/` 目录）：

```
Server/HongZhongMahjong/
├── HongZhongMahjongRoom.h/cpp       // 继承自 GameRoom
├── HongZhongMahjongAvatar.h/cpp    // 继承自 GameAvatar
├── HongZhongMahjongRule.h/cpp      // 规则
├── HongZhongMahjongLoader.h/cpp    // 继承自 VenueLoader
├── HongZhongMahjongRoomHandler.h/cpp  // 继承自 VenueInnerHandler
├── HongZhongMahjongMessages.h/cpp  // 消息注册
├── algorithms/
│   ├── LaiziCalculator.h/cpp
│   ├── HuChecker.h/cpp
│   ├── TingCalculator.h/cpp
│   ├── GangCalculator.h/cpp
│   └── SettlementCalculator.h/cpp
```

**交付物**：
- [ ] `Server/HongZhongMahjong/` 完整实现
- [ ] 单元测试：`test_hu_check.cpp`, `test_settlement.cpp`
- [ ] 10000局随机模拟测试脚本

---

#### 任务 2.3：跑得快插件

**目标**：实现跑得快游戏逻辑

**核心算法**（对应 `游戏引擎详细设计.md` 3.3）：

1. **CardTypeValidator** — 牌型校验
2. **CardTypeComparator** — 牌型比较
3. **AutoPlayStrategy** — 托管AI策略

**文件结构**：

```
Server/PaoDeKuai/
├── PaoDeKuaiRoom.h/cpp
├── PaoDeKuaiAvatar.h/cpp
├── PaoDeKuaiRule.h/cpp
├── PaoDeKuaiLoader.h/cpp
├── PaoDeKuaiRoomHandler.h/cpp
├── PaoDeKuaiMessages.h/cpp
├── algorithms/
│   ├── CardTypeValidator.h/cpp
│   ├── CardTypeComparator.h/cpp
│   └── AutoPlayStrategy.h/cpp
```

**交付物**：
- [ ] `Server/PaoDeKuai/` 完整实现
- [ ] 单元测试

---

#### 任务 2.4：长沙麻将插件

**目标**：实现长沙麻将游戏逻辑

**核心算法**（对应 `游戏引擎详细设计.md` 3.4）：

1. **QishouHuChecker** — 起手胡检测（7种）
2. **ZhongNiaoSettlement** — 中鸟结算

**文件结构**：

```
Server/ChangShaMahjong/
├── ChangShaMahjongRoom.h/cpp
├── ChangShaMahjongAvatar.h/cpp
├── ChangShaMahjongRule.h/cpp
├── ChangShaMahjongLoader.h/cpp
├── ChangShaMahjongRoomHandler.h/cpp
├── ChangShaMahjongMessages.h/cpp
├── algorithms/
│   ├── QishouHuChecker.h/cpp
│   └── ZhongNiaoSettlement.h/cpp
```

**交付物**：
- [ ] `Server/ChangShaMahjong/` 完整实现
- [ ] 单元测试

---

#### 任务 2.5：结算服务 (Settlement Service)

**目标**：单局结算/整房结算/积分扣减

**C++ 实现**：

```cpp
// Server/Settlement/SettlementManager.h
class SettlementManager : public Singleton<SettlementManager> {
public:
    // 单局结算
    bool settleRound(int64_t room_id, int round_no, 
                       const nlohmann::json& game_result,
                       nlohmann::json& settlement_result);
    
    // 整房结算
    bool settleRoom(int64_t room_id, 
                       const std::vector<nlohmann::json>& round_settlements,
                       nlohmann::json& room_settlement_result);
    
    // 房费扣除
    bool deductRoomFee(int64_t room_id, const std::string& fee_mode);
    
private:
    // 调用 WalletManager 写入流水
    bool applyScoreChanges(const std::vector<ScoreChange>& changes);
};
```

**交付物**：
- [ ] `Server/Settlement/SettlementManager.h/cpp`
- [ ] `Server/Settlement/SettlementMessages.h/cpp`

---

#### 任务 2.6：回放服务 (Replay Service)

**目标**：回放数据生成/存储/查询/播放

**C++ 实现**：

```cpp
// Server/Replay/ReplayManager.h
class ReplayManager : public Singleton<ReplayManager> {
public:
    // 生成回放（调用 ReplayBuilder）
    bool generateReplay(int64_t round_id, const nlohmann::json& game_actions,
                           std::string& replay_id);
    
    // 查询回放
    bool getReplay(const std::string& replay_id, nlohmann::json& replay_data);
    
    // 按房间查询回放列表
    bool getReplaysByRoom(int64_t room_id, int page, int page_size,
                               nlohmann::json& replay_list);
    
    // 按玩家查询回放列表
    bool getReplaysByPlayer(int64_t user_id, int page, int page_size,
                                 nlohmann::json& replay_list);
};
```

**交付物**：
- [ ] `Server/Replay/ReplayManager.h/cpp`
- [ ] `Server/Replay/ReplayMessages.h/cpp`

---

### 阶段三：地方扩展（8-10周）→ 对应文档 9.3

#### 任务 3.1：桃江麻将插件

**目标**：实现桃江麻将（2人麻将）

**核心算法**（对应 `游戏引擎详细设计.md` 3.1）：

1. **TileSetManager** — 牌集管理（支持万/条/筒/风牌/箭牌，可选红中/赖子/花牌）
2. **HuChecker** — 胡牌判断（基本胡型: 4+N*3+2）
3. **TingCalculator** — 听牌提示
4. **SettlementCalculator** — 结算（底分 * 番数系数）

**文件结构**：

```
Server/TaoJiangMahjong/
├── TaoJiangMahjongRoom.h/cpp
├── TaoJiangMahjongAvatar.h/cpp
├── TaoJiangMahjongRule.h/cpp
├── TaoJiangMahjongLoader.h/cpp
├── TaoJiangMahjongRoomHandler.h/cpp
├── TaoJiangMahjongMessages.h/cpp
├── algorithms/
│   ├── TileSetManager.h/cpp
│   ├── HuChecker.h/cpp
│   ├── TingCalculator.h/cpp
│   └── SettlementCalculator.h/cpp
```

**交付物**：
- [ ] `Server/TaoJiangMahjong/` 完整实现

---

#### 任务 3.2：益阳歪胡子插件

**目标**：实现益阳歪胡子（字牌，3人）

**核心算法**（对应 `游戏引擎详细设计.md` 3.5）：

1. **HuxiAccumulator** — 胡息计算系统
2. **ChiOptionEnumerator** — 吃牌组合枚举（最大难点）

**文件结构**：

```
Server/YiYangWaiHuZi/
├── YiYangWaiHuZiRoom.h/cpp
├── YiYangWaiHuZiAvatar.h/cpp
├── YiYangWaiHuZiRule.h/cpp
├── YiYangWaiHuZiLoader.h/cpp
├── YiYangWaiHuZiRoomHandler.h/cpp
├── YiYangWaiHuZiMessages.h/cpp
├── algorithms/
│   ├── HuxiAccumulator.h/cpp
│   └── ChiOptionEnumerator.h/cpp
```

**交付物**：
- [ ] `Server/YiYangWaiHuZi/` 完整实现

---

#### 任务 3.3：沅江千分插件

**目标**：实现沅江千分（扑克，3-5人）

**核心算法**（对应 `游戏引擎详细设计.md` 3.6）：

1. **CallBidManager** — 叫分抢庄流程
2. **ScoringCardManager** — 计分牌机制
3. **UpgradeModeSettlement** — 升级模式结算
4. **CumulativeModeSettlement** — 累计模式结算

**文件结构**：

```
Server/YuanJiangQianFen/
├── YuanJiangQianFenRoom.h/cpp
├── YuanJiangQianFenAvatar.h/cpp
├── YuanJiangQianFenRule.h/cpp
├── YuanJiangQianFenLoader.h/cpp
├── YuanJiangQianFenRoomHandler.h/cpp
├── YuanJiangQianFenMessages.h/cpp
├── algorithms/
│   ├── CallBidManager.h/cpp
│   ├── ScoringCardManager.h/cpp
│   ├── UpgradeModeSettlement.h/cpp
│   └── CumulativeModeSettlement.h/cpp
```

**交付物**：
- [ ] `Server/YuanJiangQianFen/` 完整实现

---

#### 任务 3.4：游戏规则测试器（后台）

**目标**：输入手牌返回胡牌/得分（运营可自验规则）

**C++ 实现**：（提供 HTTP API，或由管理后台直接调用）

```cpp
// Server/RuleTester/RuleTester.h
class RuleTester {
public:
    // 测试胡牌
    bool testHu(const std::string& game_code, const nlohmann::json& hand_tiles,
                   const nlohmann::json& rule_config, nlohmann::json& result);
    
    // 测试结算
    bool testSettlement(const std::string& game_code, 
                            const nlohmann::json& game_result,
                            const nlohmann::json& rule_config, 
                            nlohmann::json& settlement_result);
};
```

**交付物**：
- [ ] `Server/RuleTester/RuleTester.h/cpp`
- [ ] `Server/RuleTester/RuleTesterMessages.h/cpp`

---

#### 任务 3.5：玩法版本管理系统

**目标**：草稿/审批/灰度/回滚

**数据库表**：`game_rule_versions`

**C++ 实现**：

```cpp
// Server/RuleVersion/RuleVersionManager.h
class RuleVersionManager : public Singleton<RuleVersionManager> {
public:
    // 创建规则版本（草稿）
    bool createVersion(int game_id, const std::string& version_no,
                           const nlohmann::json& config_json);
    
    // 提交审批
    bool submitForApproval(int64_t version_id, int64_t approver_id);
    
    // 审批
    bool approve(int64_t version_id, int64_t approver_id, bool approved);
    
    // 生效
    bool activate(int64_t version_id, const std::string& effective_time);
    
    // 灰度发布
    bool grayRelease(int64_t version_id, int gray_percent);
    
    // 回滚
    bool rollback(int64_t version_id, int64_t target_version_id);
};
```

**交付物**：
- [ ] `Server/RuleVersion/RuleVersionManager.h/cpp`
- [ ] `Server/RuleVersion/RuleVersionMessages.h/cpp`

---

### 阶段四：运营后台完善（6-8周）→ 对应文档 9.4

#### 任务 4.1：财务对账模块

**目标**：日对账/渠道对账/异常账

**C++ 实现**：

```cpp
// Server/Finance/FinanceManager.h
class FinanceManager : public Singleton<FinanceManager> {
public:
    // 日对账
    bool dailyReconciliation(const std::string& date, nlohmann::json& report);
    
    // 渠道对账
    bool channelReconciliation(const std::string& channel_code, 
                                  const std::string& start_date, 
                                  const std::string& end_date, 
                                  nlohmann::json& report);
    
    // 异常账处理
    bool handleAbnormalLedger(int64_t ledger_id, const std::string& reason);
};
```

**交付物**：
- [ ] `Server/Finance/FinanceManager.h/cpp`

---

#### 任务 4.2：活动奖励系统 (Activity Service)

**目标**：签到/新人/邀请/排行/补偿

**数据库表**：`activities`, `activity_records`

**C++ 实现**：

```cpp
// Server/Activity/ActivityManager.h
class ActivityManager : public Singleton<ActivityManager> {
public:
    // 创建活动
    bool createActivity(const nlohmann::json& activity_config);
    
    // 领取奖励
    bool claimReward(int64_t user_id, int64_t activity_id, 
                          nlohmann::json& reward_result);
    
    // 查询活动列表
    bool getActivities(int64_t user_id, const std::string& activity_type, 
                              nlohmann::json& activity_list);
};
```

**交付物**：
- [ ] `Server/Activity/ActivityManager.h/cpp`
- [ ] `Server/Activity/ActivityMessages.h/cpp`

---

#### 任务 4.3：推广渠道 (Promotion Service)

**目标**：渠道码/邀请链/效果统计

**数据库表**：`promotion_channels`

**交付物**：
- [ ] `Server/Promotion/PromotionManager.h/cpp`

---

#### 任务 4.4：客服工单 (Customer Service)

**目标**：工单提交/接单/处理/流转

**数据库表**：`customer_tickets`, `customer_ticket_replies`

**交付物**：
- [ ] `Server/Customer/CustomerManager.h/cpp`
- [ ] `Server/Customer/CustomerMessages.h/cpp`

---

#### 任务 4.5：风控中心 (Risk Service)

**目标**：规则配置/命中/处理

**数据库表**：`risk_rules`, `risk_events`

**C++ 实现**：

```cpp
// Server/Risk/RiskManager.h
class RiskManager : public Singleton<RiskManager> {
public:
    // 检测风控事件
    bool detect(int64_t user_id, const std::string& rule_code, 
                     const nlohmann::json& context, nlohmann::json& result);
    
    // 处理风控事件
    bool handleEvent(int64_t event_id, const std::string& action, 
                          int64_t handler_id, const std::string& note);
    
    // 配置规则
    bool configureRule(const std::string& rule_code, 
                             const nlohmann::json& threshold_config, 
                             const std::string& default_action);
};
```

**交付物**：
- [ ] `Server/Risk/RiskManager.h/cpp`
- [ ] `Server/Risk/RiskMessages.h/cpp`

---

#### 任务 4.6：BI 驾驶舱与数据报表 (Stats Service)

**目标**：核心指标/趋势图/导出

**C++ 实现**：

```cpp
// Server/Stats/StatsManager.h
class StatsManager : public Singleton<StatsManager> {
public:
    // 获取经营驾驶舱数据
    bool getDashboard(nlohmann::json& dashboard_data);
    
    // 获取核心指标
    bool getCoreMetrics(const std::string& start_date, const std::string& end_date,
                            nlohmann::json& metrics);
    
    // 导出报表
    bool exportReport(const std::string& report_type, 
                           const nlohmann::json& params, 
                           std::string& file_url);
};
```

**交付物**：
- [ ] `Server/Stats/StatsManager.h/cpp`
- [ ] `Server/Stats/StatsMessages.h/cpp`

---

### 阶段五：企业级稳定性（6-8周）→ 对应文档 9.5

#### 任务 5.1：版本升级管理中心

**目标**：App版本/热更新/Bug追踪/灰度/回滚

**数据库表**：`app_versions`, `app_hotfixes`, `bug_tickets`

**交付物**：
- [ ] `Server/Version/VersionManager.h/cpp`

---

#### 任务 5.2：音效/特效后台配置中心

**目标**：资源上传/事件绑定/灰度/一键关闭

**数据库表**：`audio_resources`, `vfx_resources`

**交付物**：
- [ ] `Server/Resource/ResourceManager.h/cpp`

---

#### 任务 5.3：监控告警 (Prometheus/Grafana)

**目标**：9项核心指标采集/告警规则

**C++ 实现**：

```cpp
// Framework/Monitor/MonitorManager.h
class MonitorManager : public Singleton<MonitorManager> {
public:
    // 启动监控
    bool start(const std::string& prometheus_url);
    
    // 记录指标
    void recordGauge(const std::string& metric_name, double value, 
                          const std::vector<std::string>& labels = {});
    void recordCounter(const std::string& metric_name, int64_t delta, 
                           const std::vector<std::string>& labels = {});
    void recordHistogram(const std::string& metric_name, double observation, 
                            const std::vector<std::string>& labels = {});
    
    // 采集核心指标
    void collectCoreMetrics();
};
```

**核心指标**（对应 dev_plan.md 12.3）：

```cpp
// 1. 在线人数
monitor.recordGauge("online_users", online_count);

// 2. WebSocket 连接数
monitor.recordGauge("ws_connections", ws_connection_count);

// 3. 房间创建 QPS
monitor.recordCounter("room_create_qps", room_create_count, {game_code});

// 4. 游戏操作延迟 P99
monitor.recordHistogram("game_action_latency_p99", p99_latency, {game_code});

// 5. 结算耗时 P99
monitor.recordHistogram("settlement_duration_p99", p99_duration, {game_code});

// 6. API 错误率
monitor.recordGauge("api_error_rate", error_rate, {api_endpoint});

// 7. Redis 延迟 P99
monitor.recordHistogram("redis_latency_p99", p99_latency);

// 8. 数据库慢查询
monitor.recordCounter("db_slow_queries", slow_query_count);

// 9. 风控命中数
monitor.recordCounter("risk_hits", risk_hit_count, {risk_rule});
```

**交付物**：
- [ ] `Framework/Monitor/MonitorManager.h/cpp`
- [ ] Prometheus 指标导出（使用 `prometheus-cpp` 库）

---

#### 任务 5.4：压力测试 & 性能调优

**目标**：并发报告/瓶颈优化，达到目标 QPS

**测试工具**：

1. **房间创建压力测试**：
   - 使用 `wrk` 或自研压测工具
   - 目标：1000 房间创建/秒

2. **游戏操作延迟测试**：
   - 模拟 4 人同时操作
   - 目标：P99 < 500ms

3. **结算性能测试**：
   - 目标：单局结算 < 50ms

4. **并发安全性测试**：
   - 模拟 1000 人同时游戏
   - 检测死锁/竞态/数据不一致

**C++ 实现**：创建 `tests/` 目录，编写压力测试代码

**交付物**：
- [ ] `tests/load_test.cpp`（压测工具）
- [ ] 性能报告

---

#### 任务 5.5：安全加固

**目标**：接口加密/防刷/数据脱敏/渗透测试

**C++ 实现**：

1. **接口加密**：
   - 使用 OpenSSL 对敏感字段加密（密码、token）
   - 使用 HTTPS（WebSocket over TLS）

2. **防刷**：
   - 使用 Redis 实现频率限制（`INCRBY` + `EXPIRE`）
   - 示例：`limit:user:10001:login` → 每分钟最多 5 次

3. **数据脱敏**：
   - 手机号：`138****1234`
   - 身份证号：`4306**********1234`

4. **审计日志**：
   - 所有管理操作记录到 `admin_audit_logs`

**交付物**：
- [ ] 安全加固报告
- [ ] 渗透测试报告

---

#### 任务 5.6：数据备份与容灾

**目标**：定期备份/恢复演练/容灾方案

**实施方案**：

1. **MySQL 备份**：
   - 每天全量备份（`mysqldump`）
   - 每小时增量备份（binlog）
   - 保留 30 天

2. **Redis 备份**：
   - RDB 快照（每小时）
   - AOF 持久化（每秒 `fsync`）

3. **MongoDB 备份**：
   - `mongodump` 每天全量备份

4. **恢复演练**：
   - 每月一次恢复演练

**交付物**：
- [ ] 备份脚本（`scripts/backup_mysql.sh`, `scripts/backup_redis.sh`, `scripts/backup_mongodb.sh`）
- [ ] 恢复文档

---

## 三、目录结构规划（最终）

```
server/
├── 3rdpart/              # 第三方库
│   ├── msgpack/
│   ├── json/            # ← 新增：nlohmann/json
│   ├── mongo-c-driver/  # ← 新增：MongoDB C Driver
│   ├── mongo-cxx-driver/ # ← 新增：MongoDB C++ Driver
│   ├── prometheus-cpp/  # ← 新增：Prometheus C++ Client
│   └── ...
│
├── Framework/             # 基础框架层（已有，扩展）
│   ├── Base/
│   ├── Constant/
│   ├── Database/        # MySQL Pool
│   ├── MongoDB/         # ← 新增：MongoDB Pool
│   ├── Monitor/         # ← 新增：监控指标采集
│   ├── Game/            # 游戏引擎框架
│   │   ├── GameRoom.h/cpp
│   │   ├── GameAvatar.h/cpp
│   │   ├── SecureRandomManager.h/cpp  # ← 新增
│   │   ├── SeatManager.h/cpp            # ← 新增
│   │   ├── TurnTimer.h/cpp               # ← 新增
│   │   ├── ActionQueue.h/cpp             # ← 新增
│   │   ├── ScoreLedgerWriter.h/cpp      # ← 新增
│   │   ├── DisconnectHandler.h/cpp       # ← 新增
│   │   ├── DissolveVoter.h/cpp          # ← 完善
│   │   ├── ReplayBuilder.h/cpp          # ← 新增
│   │   └── ...
│   ├── Message/
│   ├── MySql/           # MySQL 初始化
│   │   ├── MysqlInitializer.h/cpp  # ← 新增
│   │   └── ...
│   ├── Network/
│   ├── Player/
│   ├── Rabbitmq/
│   ├── Redis/
│   ├── Thread/
│   ├── Timer/
│   └── Venue/
│
├── Server/                # 服务器主程序 + 各游戏房间实现
│   ├── User/            # ← 新增：用户服务
│   ├── Wallet/          # ← 新增：钱包服务
│   ├── Room/            # ← 新增：房间服务
│   ├── Settlement/      # ← 新增：结算服务
│   ├── Replay/          # ← 新增：回放服务
│   ├── Activity/        # ← 新增：活动奖励
│   ├── Promotion/       # ← 新增：推广渠道
│   ├── Customer/        # ← 新增：客服工单
│   ├── Risk/            # ← 新增：风控中心
│   ├── Stats/           # ← 新增：数据报表
│   ├── Finance/         # ← 新增：财务对账
│   ├── RuleVersion/     # ← 新增：规则版本管理
│   ├── RuleTester/      # ← 新增：规则测试器
│   ├── Version/         # ← 新增：版本管理
│   ├── Resource/        # ← 新增：音效/特效配置
│   ├── RabbitMQ/        # ← 新增：消息定义
│   │
│   ├── StandardMahjong/    # 已有
│   ├── HongZhongMahjong/  # ← 新增：红中麻将
│   ├── PaoDeKuai/         # ← 新增：跑得快
│   ├── ChangShaMahjong/   # ← 新增：长沙麻将
│   ├── TaoJiangMahjong/   # ← 新增：桃江麻将
│   ├── YiYangWaiHuZi/     # ← 新增：益阳歪胡子
│   ├── YuanJiangQianFen/   # ← 新增：沅江千分
│   ├── BiJi/               # 已有
│   ├── GuanDan/            # 已有
│   ├── Lackey/             # 已有
│   ├── NiuNiu100/          # 已有
│   └── ...
│
├── scripts/               # ← 新增：SQL脚本、备份脚本
│   ├── init_tables.sql
│   ├── seed_data.sql
│   ├── create_ledger_table.sql
│   ├── backup_mysql.sh
│   ├── backup_redis.sh
│   └── backup_mongodb.sh
│
├── tests/                # ← 新增：压力测试
│   ├── load_test.cpp
│   ├── test_hu_check.cpp
│   └── ...
│
├── CMakeLists.txt
├── server.ini
└── server.sln
```

---

## 四、关键技术决策

### 4.1 单体 vs 微服务

**当前选择**：**单体架构**（所有服务在一个进程中）

**理由**：
1. 现有代码是单体架构，改动成本高
2. 使用 RabbitMQ 解耦模块，未来可拆分为微服务
3. 团队规模（13-17人）适合单体架构

**未来演进**：当 DAU > 10万时，可拆分为独立进程（修改 `CMakeLists.txt` 生成多个可执行文件）

---

### 4.2 协议选择

**当前**：自定义二进制协议（msgpack）

**新增**：JSON 格式（对应 `WebSocket协议设计.md`）

**兼容方案**：
- TCP 服务器：使用 msgpack（大厅消息、后台管理）
- WebSocket 服务器：使用 JSON（游戏内实时消息）

---

### 4.3 数据库选型确认

| 数据类型 | 存储方案 | 理由 |
|---------|---------|------|
| 结构化数据（用户、房间、流水） | MySQL 8.0 | 事务支持、乐观锁 |
| 缓存/会话/排行榜/分布式锁 | Redis Cluster | 高性能 |
| 回放数据（JSON） | MongoDB 6.0 | 灵活 schema、按 step 查询 |
| 消息队列 | RabbitMQ | 可靠投递、支持扇出 |

---

### 4.4 分库分表实现

**MySQL 分库分表**（对应 `数据库详细设计.md` 1.3）：

```cpp
// 用户库分片（user_id % 8）
int getUserDBShard(int64_t user_id) {
    return user_id % 8;
}

// 流水表按月分表
std::string getLedgerTableName(const std::string& date) {
    // date format: "2026-01-15" → "wallet_ledgers_202601"
    return "wallet_ledgers_" + date.substr(0, 7).replace(5, 1, "");
}

// 房间库分片（game_id % 4）
int getRoomDBShard(int game_id) {
    return game_id % 4;
}
```

**实现**：修改 `MysqlPool` 支持多个数据库连接（8个用户库 + 4个房间库）

---

## 五、开发排期（详细）

### 阶段一：基础平台（8-10周）

| 周次 | 任务 | 负责人 | 产出物 |
|-------|------|--------|---------|
| W1-W2 | 任务1.1：数据库表结构初始化 | 后端工程师 × 1 | SQL脚本、MysqlInitializer |
| W2-W3 | 任务1.2：MongoDB 集成 | 后端工程师 × 1 | MongoDBPool、连接池测试 |
| W3-W4 | 任务1.3：JSON 解析库集成 | 后端工程师 × 1 | json.hpp、WebSocket JSON协议适配 |
| W4-W6 | 任务1.4：用户服务 | 后端工程师 × 2 | UserManager、登录注册消息 |
| W6-W8 | 任务1.5：钱包服务 | 后端工程师 × 2 | WalletManager、流水记录 |
| W8-W9 | 任务1.6：房间服务 | 后端工程师 × 2 | RoomManager、解散投票 |
| W9-W10 | 任务1.7：RabbitMQ 消息定义 | 后端工程师 × 1 | 消息类型定义、发布/消费 |

---

### 阶段二：游戏引擎框架 + 首批游戏（10-14周）

| 周次 | 任务 | 负责人 | 产出物 |
|-------|------|--------|---------|
| W11-W13 | 任务2.1：游戏引擎框架（8个组件） | 游戏引擎开发 × 2 | SecureRandomManager、SeatManager、TurnTimer、ActionQueue、ScoreLedgerWriter、DisconnectHandler、DissolveVoter、ReplayBuilder |
| W13-W17 | 任务2.2：红中麻将插件 | 游戏引擎开发 × 2 | HongZhongMahjong/ 完整实现 |
| W17-W19 | 任务2.3：跑得快插件 | 游戏引擎开发 × 1 | PaoDeKuai/ 完整实现 |
| W19-W22 | 任务2.4：长沙麻将插件 | 游戏引擎开发 × 2 | ChangShaMahjong/ 完整实现 |
| W22-W23 | 任务2.5：结算服务 | 后端工程师 × 1 | SettlementManager |
| W23-W24 | 任务2.6：回放服务 | 后端工程师 × 1 | ReplayManager |

---

### 阶段三：地方扩展（8-10周）

| 周次 | 任务 | 负责人 | 产出物 |
|-------|------|--------|---------|
| W25-W27 | 任务3.1：桃江麻将插件 | 游戏引擎开发 × 2 | TaoJiangMahjong/ 完整实现 |
| W27-W30 | 任务3.2：益阳歪胡子插件 | 游戏引擎开发 × 2 | YiYangWaiHuZi/ 完整实现 |
| W30-W32 | 任务3.3：沅江千分插件 | 游戏引擎开发 × 1 | YuanJiangQianFen/ 完整实现 |
| W32-W33 | 任务3.4：游戏规则测试器 | 后端工程师 × 1 | RuleTester |
| W33-W34 | 任务3.5：玩法版本管理系统 | 后端工程师 × 1 | RuleVersionManager |

---

### 阶段四：运营后台完善（6-8周）

| 周次 | 任务 | 负责人 | 产出物 |
|-------|------|--------|---------|
| W35-W36 | 任务4.1：财务对账模块 | 后端工程师 × 1 | FinanceManager |
| W36-W37 | 任务4.2：活动奖励系统 | 后端工程师 × 1 | ActivityManager |
| W37-W38 | 任务4.3：推广渠道 | 后端工程师 × 1 | PromotionManager |
| W38-W39 | 任务4.4：客服工单 | 后端工程师 × 1 | CustomerManager |
| W39-W40 | 任务4.5：风控中心 | 后端工程师 × 1 + 算法工程师 × 1 | RiskManager |
| W40-W41 | 任务4.6：BI 驾驶舱 | 后端工程师 × 1 | StatsManager |

---

### 阶段五：企业级稳定性（6-8周）

| 周次 | 任务 | 负责人 | 产出物 |
|-------|------|--------|---------|
| W42-W43 | 任务5.1：版本升级管理 | 后端工程师 × 1 | VersionManager |
| W43-W44 | 任务5.2：音效/特效配置 | 后端工程师 × 1 | ResourceManager |
| W44-W45 | 任务5.3：监控告警 | DevOps × 1 + 后端工程师 × 1 | MonitorManager、Prometheus配置 |
| W45-W46 | 任务5.4：压力测试 | 测试工程师 × 1 + 后端工程师 × 1 | 压测工具、性能报告 |
| W46-W47 | 任务5.5：安全加固 | 安全工程师 × 1 + 后端工程师 × 1 | 安全加固报告 |
| W47-W48 | 任务5.6：数据备份与容灾 | DevOps × 1 | 备份脚本、恢复文档 |

---

## 六、风险评估与应对

| 风险 | 可能性 | 影响 | 应对措施 |
|------|--------|------|---------|
| 地方规则细节反复变更 | 高 | 算法返工 | 规则配置化 + 版本管理 + 早期锁定 |
| 游戏算法 Bug 导致结算错误 | 高 | 积分异常/用户投诉 | 10000局模拟测试 + 对账脚本 + 快速修复通道 |
| MongoDB 集成难度超预期 | 中 | 回放功能延迟 | 提前技术预研、准备 MySQL JSON 类型作为备选 |
|  WebSocket JSON 协议适配工作量超预期 | 中 | 客户端联调延迟 | 优先实现核心消息类型、非核心消息延后 |
| 合规风险 | 高 | 下架/法律问题 | 律师审核 + 风控只识别不操控 + 审计留痕 |
| 工期延期 | 中 | 上线推迟 | MVP思路：先上3款游戏 + 后台核心功能 |

---

## 七、MVP（最小可行产品）定义

如果需要**快速验证市场**，建议 MVP 范围如下（对应 dev_plan.md 十五）：

| 模块 | 内容 | 不包含 |
|------|------|--------|
| **游戏** | 红中麻将 + 跑得快（先做这2款最流行） | 其他4款游戏 |
| **端** | Android App + 管理后台Web | iOS（二期） |
| **房间** | 好友房 + 匹配房 | 俱乐部/比赛房 |
| **账户** | 手机号登录 + 游客模式 | 微信/Apple ID |
| **钱包** | 娱乐积分 + 房卡 | 保险箱（二期） |
| **后台** | 游戏/房间/玩家/规则/权限 | BI/风控/客服/活动（二期） |
| **音效/特效** | 基础音效 + L1-L3特效 | 高级特效/后台配置（三期） |
| **版本管理** | 基础更新检测 | 灰度/热更新（二期） |

**MVP 预估工期**：**18-22 周（约 5-6 个月），团队 6-8 人**

---

## 八、验收标准

### 8.1 数据库

- [ ] 29张核心表全部创建成功
- [ ] 分库分表策略生效（用户库8个、房间库4个）
- [ ] 流水表按月自动创建
- [ ] 审计日志按月分区

### 8.2 游戏引擎框架

- [ ] 8个公共组件全部实现
- [ ] 安全随机数通过 statistical test（卡方检验）
- [ ] 断线重连恢复成功率 > 99%
- [ ] 回放数据完整可回溯

### 8.3 游戏插件

- [ ] 6款游戏全部可玩
- [ ] 每款游戏通过10000局随机模拟测试
- [ ] 结算一致性：总积分变化 = 0
- [ ] 并发安全：无死锁/竞态

### 8.4 性能

- [ ] 在线人数：支持 1000 并发
- [ ] 游戏操作延迟：P99 < 500ms
- [ ] 结算耗时：P99 < 50ms
- [ ] API 错误率：< 1%

### 8.5 安全

- [ ] 所有敏感字段加密存储
- [ ] 接口频率限制生效
- [ ] 审计日志不可篡改
- [ ] 通过渗透测试

---

## 九、附录

### 9.1 参考文档索引

| 文档 | 位置 | 说明 |
|------|------|------|
| dev_plan.md | `/Users/ryan/workspace/poker/server/server/dev_plan.md` | 开发方案总览 |
| 游戏引擎详细设计.md | `/Users/ryan/workspace/poker/server/server/游戏引擎详细设计.md` | 引擎架构 + 算法流程 |
| 数据库详细设计.md | `/Users/ryan/workspace/poker/server/server/数据库详细设计.md` | 29张表DDL + 存储矩阵 |
| API接口详细设计.md | `/Users/ryan/workspace/poker/server/server/API接口详细设计.md` | RESTful API 定义 |
| WebSocket协议设计.md | `/Users/ryan/workspace/poker/server/server/WebSocket协议设计.md` | 实时通信协议 |
| 管理后台详细设计.md | `/Users/ryan/workspace/poker/server/server/管理后台详细设计.md` | 后台页面与权限 |
| 客户端技术设计.md | `/Users/ryan/workspace/poker/server/server/客户端技术设计.md` | Cocos Creator 实现 |
| 安全设计方案.md | `/Users/ryan/workspace/poker/server/server/安全设计方案.md` | 安全架构 |
| 部署架构设计方案.md | `/Users/ryan/workspace/poker/server/server/部署架构设计方案.md` | 部署拓扑 |

### 9.2 外部依赖

| 依赖 | 用途 | 版本 | 备注 |
|------|------|------|------|
| Boost | 日志、文件系统、网络 | 1.70+ | 已有 |
| OpenSSL | Token校验、加密 | 1.1+ | 已有 |
| MySQL Connector/C++ | MySQL 访问 | 8.0+ | 已有 |
| redis-plus-plus | Redis 访问 | 1.3+ | 已有 |
| rabbitmq-c | RabbitMQ 访问 | 0.13+ | 已有 |
| msgpack-c | 序列化 | 3.3+ | 已有 |
| **nlohmann/json** | JSON 解析 | 3.11+ | **新增** |
| **mongo-cxx-driver** | MongoDB 访问 | 3.7+ | **新增** |
| **prometheus-cpp** | 监控指标 | 1.1+ | **新增** |

### 9.3 团队配置建议（峰值）

| 角色 | 人数 | 主要职责 |
|------|--------|---------|
| 项目经理/产品 | 1 | 需求把控/进度管理 |
| 后端工程师 | 3-4 | 微服务/API/数据库/游戏引擎后端 |
| 游戏引擎开发 | 2-3 | 6款游戏插件/算法/结算逻辑 |
| DevOps/运维 | 1 | 部署/监控/CI-CD/安全加固 |
| 测试工程师 | 1-2 | 功能测试/性能测试/兼容性测试 |
| **合计** | **10-13人** | |

---

**文档版本**：v1.0  
**创建时间**：2026-05-25  
**作者**：Game Server 开发团队
