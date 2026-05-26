# C++ 游戏服务器开发计划

> 基于 `new_rules` v2.3 需求文档，按 C++ 游戏服务器的功能边界拆分。本文档仅涵盖 **server/** 的开发任务。

---

## 一、现有基础

当前 C++ 服务器已实现：标准麻将、比鸡、逮狗腿、百人牛牛、掼蛋。核心框架（Venue+Handler 线程模型、TCP/WebSocket 网络、MessagePack 序列化、Redis/RabbitMQ/MySQL 集成）已稳定运行。

---

## 二、新增游戏开发

### 2.1 桃江麻将（游戏类型待分配 ID）

**定位：** 地方 2 人麻将，支持好友房、匹配房、练习房。

**核心开发任务：**

1. **牌张配置系统**
   - 支持后台可配置牌张集合：万、条、筒、风牌、箭牌
   - 可配置项：是否启用红中、是否启用赖子、是否启用花牌
   - 牌张配置不能写死，需通过规则配置 JSON 传入

2. **后台玩法配置对接**
   - 从 web_server 通过 RabbitMQ 下发规则配置 JSON
   - 解析配置项：player_count、round_count、base_score、max_score、room_fee_type、allow_peng/gang/chi/zimo/dianpao、laizi_enabled、hongzhong_enabled、banker_rule、dissolve_vote
   - 创建房间时保存玩法配置快照

3. **操作实现**
   - READY、DRAW、DISCARD、CHI（可选）、PENG、GANG、HU、PASS、DISSOLVE_VOTE

4. **结算模块**
   - 拆分为独立函数：calculateBaseScore()、calculateZimoScore()、calculateDianpaoScore()、calculateGangScore()、calculateBankerScore()、calculateMaxLimit()、calculateRoomFee()、writeScoreLedger()
   - 结算结果写入 MySQL，积分变动通过 MQ 通知 web_server

5. **回放数据生成**
   - 每步操作序列化（操作类型、牌张、玩家、时间戳）
   - 包含随机种子 hash、初始牌数据、网络状态
   - 回放 JSON 存入 MySQL 或对象存储

6. **文件结构**（参考现有 StandardMahjong/）
   ```
   Server/TaoJiangMahjong/
     TaoJiangMahjongRoom.h/.cpp
     TaoJiangMahjongAvatar.h/.cpp
     TaoJiangMahjongMessages.h/.cpp
     TaoJiangMahjongLoader.h/.cpp
   ```

### 2.2 红中麻将（游戏类型待分配 ID）

**定位：** 红中赖子玩法，胡牌算法复杂。

**核心开发任务：**

1. **红中赖子引擎**
   - 红中数量可配置（hongzhong_count）
   - 红中作为赖子的替代规则（hongzhong_as_laizi、laizi_replace_rule）
   - 是否允许红中参与胡牌（can_hu_with_hongzhong）

2. **胡牌算法**
   - 普通 3N+2
   - 七对（qidui_enabled）
   - 碰碰胡（pengpenghu_enabled）
   - 红中赖子替代 —— 多赖子最优组合
   - 听牌提示（ting-calculator）
   - 可胡牌列表计算

3. **杠分系统**
   - gang_score_enabled、zimo_double 配置
   - 杠分单独计算并记录

4. **文件结构**
   ```
   Server/HongZhongMahjong/
     HongZhongMahjongRoom.h/.cpp
     HongZhongMahjongAvatar.h/.cpp
     HongZhongMahjongMessages.h/.cpp
     HongZhongMahjongLoader.h/.cpp
   ```

### 2.3 跑得快（游戏类型待分配 ID）

**定位：** 扑克竞技，2 人 15 张牌。

**核心开发任务：**

1. **牌型识别与比较**
   - 单张、对子、三张、三带一、三带二、顺子、连对、飞机、炸弹、王炸
   - 复用 `Poker/` 模块中的牌面定义和组合逻辑

2. **出牌校验（服务端强校验）**
   - 出牌是否属于玩家手牌
   - 出牌牌型是否合法
   - 是否符合当前压牌规则
   - 是否轮到该玩家出牌
   - 是否存在必须出牌规则（must_include_spade3）
   - 是否触发炸弹结算
   - 是否出完牌

3. **配置项**
   - player_count、card_count、first_play_rule、must_include_spade3、bomb_score、bomb_double、allow_pass、auto_play_timeout、reconnect_timeout、max_round_score

4. **托管与断线**
   - 托管超时自动出牌（auto_play_timeout）
   - 断线保留房间状态（reconnect_timeout）
   - 逃跑惩罚结算

5. **文件结构**
   ```
   Server/PaoDeKuai/
     PaoDeKuaiRoom.h/.cpp
     PaoDeKuaiAvatar.h/.cpp
     PaoDeKuaiMessages.h/.cpp
     PaoDeKuaiLoader.h/.cpp
   ```

### 2.4 长沙麻将（游戏类型待分配 ID）

**定位：** 地方规则强，需重点支持起手胡和中鸟。

**核心开发任务：**

1. **起手胡检测**
   - 发牌后立即检测：缺一色、板板胡、大四喜、六六顺、节节高、三同、一枝花
   - 每种起手胡类型可后台开关
   - 起手胡触发时立即通知客户端

2. **中鸟结算**
   - 鸟牌数量可配置（bird_count）
   - 中鸟命中玩家计算
   - 加分方式、是否翻倍、是否封顶
   - 中鸟结果序列化推送

3. **配置项**
   - queyise_enabled、banbanhu_enabled、dasixi_enabled、liuliushun_enabled、jiejiegao_enabled、santong_enabled、yizhihua_enabled、zhongniao_enabled、bird_count、banker_rule、max_fan

4. **文件结构**
   ```
   Server/ChangShaMahjong/
     ChangShaMahjongRoom.h/.cpp
     ChangShaMahjongAvatar.h/.cpp
     ChangShaMahjongMessages.h/.cpp
     ChangShaMahjongLoader.h/.cpp
   ```

### 2.5 益阳歪胡子（游戏类型待分配 ID）

**定位：** 字牌类，胡息计算复杂，操作类型多。

**核心开发任务：**

1. **操作实现**
   - CHI（吃牌）、PENG（碰牌）、WEI（偎牌）、PAO（跑牌）、TI（提牌）、HU（胡牌）、PASS（过）

2. **胡息计算模块**
   - 吃牌胡息、碰牌胡息、偎牌胡息、跑牌胡息、提牌胡息
   - 胡牌底息、特殊牌型加息
   - 封顶处理
   - 最低胡息门槛（min_huxi）

3. **配置项**
   - player_count（通常 3 人）、card_set（大小字牌）、min_huxi、allow_chi/peng/wei/pao/ti、zhuang_rule、tun_score_rate、max_score

4. **文件结构**
   ```
   Server/YiYangWaiHuZi/
     YiYangWaiHuZiRoom.h/.cpp
     YiYangWaiHuZiAvatar.h/.cpp
     YiYangWaiHuZiMessages.h/.cpp
     YiYangWaiHuZiLoader.h/.cpp
   ```

### 2.6 沅江千分（游戏类型待分配 ID）

**定位：** 积分扑克，叫分抢庄 + 出牌记分。

**核心开发任务：**

1. **叫分/抢庄阶段**
   - call_score_enabled、banker_rule（叫分庄/随机庄/轮庄）
   - 叫分值记录、叫分耗时记录

2. **计分牌系统**
   - 后台可配置计分牌（score_cards JSON）：如 5→5分、10→10分、K→10分
   - deck_count 可配置
   - 本轮得分计算、累计得分维护

3. **流程控制**
   - 创建房间 → 加入 → 准备 → 发牌 → 叫分/抢庄 → 出牌 → 计分 → 判断目标分 → 单局结算 → 整房总结算

4. **配置项**
   - player_count、target_score、call_score_enabled、banker_rule、deck_count、bomb_enabled、score_cards、round_limit、escape_penalty、max_score

5. **文件结构**
   ```
   Server/YuanJiangQianFen/
     YuanJiangQianFenRoom.h/.cpp
     YuanJiangQianFenAvatar.h/.cpp
     YuanJiangQianFenMessages.h/.cpp
     YuanJiangQianFenLoader.h/.cpp
   ```

---

## 三、框架层通用改造

### 3.1 玩法规则动态加载

当前规则可能部分写死，需改造为：
- 创建房间时从 RabbitMQ 消息中获取规则配置 JSON
- 每个 Room 在创建时解析并缓存规则配置
- 支持规则版本号，随房间生命周期保持快照

### 3.2 积分钱包对接

- 游戏结算后通过 RabbitMQ 向 web_server 发送积分变动事件
- 事件类型：GAME_WIN、GAME_LOSE、ROOM_FEE
- 包含：user_id、wallet_type、change_amount、biz_type、biz_id、remark

### 3.3 回放系统增强

- 每局记录：房间号、局号、游戏类型、玩法配置快照、随机种子 hash、玩家座位、初始牌数据、每步操作（含时间戳和网络状态）、结算结果、积分流水 ID
- 回放数据格式统一（JSON），便于客户端和管理后台播放
- 支持按房间号、玩家、时间、异常标记查询

### 3.4 房间状态机完善

按需求文档增加房间状态：
- WAITING → READY → PLAYING → SETTLING → FINISHED
- DISSOLVED（解散）
- EXCEPTION（异常中断）

### 3.5 风控数据采集

- 每局采集：同桌玩家组合、IP 记录、设备 ID、操作耗时
- 异常检测辅助数据：频繁同桌、固定输赢关系、同 IP 多号、异常逃跑
- 通过 MQ 将风控数据推送给 web_server

### 3.6 合规随机算法

- 发牌使用服务端安全随机数（已有随机机制需审计）
- 每局生成唯一 random_seed_hash
- 牌局结束后记录 seed 明文或加密留档
- 后台不得提供指定玩家赢输、控制发牌等功能

### 3.7 版本管理支持

- 支持 web_server 通过 MQ 下发游戏引擎版本号
- 客户端连接时返回当前游戏引擎版本
- 支持灰度发布：按玩家 ID 或百分比分配新旧版本

---

## 四、数据库表（server 负责写入的部分）

服务端负责写入或影响的核心表：

| 表 | 职责 |
|---|---|
| room | 房间创建、状态更新 |
| game_round | 牌局记录、种子 hash、结算快照 |
| game_replay | 回放 JSON/URL |
| wallet_ledger | 积分变动（通过 MQ 通知 web_server 写入） |
| room_fee_ledger | 房费消耗 |

新增表（需 web_server 建表，server 通过 MQ 交互）：
- 无额外表需 server 直接操作，均通过 MQ 与 web_server 交互。

---

## 五、开发阶段

### 第一阶段：首批游戏引擎

1. 桃江麻将 Room/Avatar/Messages/Loader + 结算模块
2. 红中麻将 Room/Avatar/Messages/Loader + 胡牌算法
3. 玩法规则动态加载框架改造
4. 积分钱包 MQ 对接
5. 回放系统增强

### 第二阶段：扑克与字牌

1. 跑得快 Room/Avatar/Messages/Loader + 出牌校验
2. 长沙麻将 Room/Avatar/Messages/Loader + 起手胡/中鸟
3. 风控数据采集
4. 合规随机算法审计

### 第三阶段：地方扩展

1. 益阳歪胡子 Room/Avatar/Messages/Loader + 胡息计算
2. 沅江千分 Room/Avatar/Messages/Loader + 叫分抢庄
3. 版本管理与灰度发布支持
4. 压力测试与异常恢复

---

## 六、验收标准

1. 每款游戏有独立 Room/Avatar/Messages/Loader，在 main.cpp 中注册
2. 每款游戏的结算算法可独立测试
3. 每局生成完整回放数据
4. 所有积分变动通过 MQ 通知 web_server
5. 发牌随机性可通过 seed hash 审计
6. 托管、断线重连、逃跑惩罚正常工作
7. 玩法配置通过 JSON 动态加载，不写死
8. 房间状态机完整，支持异常中断和投票解散
