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
Venue（抽象） -> GameRoom -> [StandardMahjongRoom, BiJiRoom, LackeyRoom, NiuNiu100Room, GuanDanRoom]
GameAvatar -> MahjongAvatar, PokerAvatar
Session -> MsgSession（TCP）, EchoSession（调试）
MessageHandler -> PlayerSignatureHandler -> VenueInnerHandler, VenueOuterHandler
VenueLoader（抽象） -- 每种游戏类型的工厂类
```

### 模块说明

| 目录 | 用途 |
|------|------|
| `Framework/` | 核心框架静态库 |
| `Framework/Base/` | 配置（INI）、日志、单例模式 |
| `Framework/Network/` | TCP 服务器、WebSocket 服务器、会话（boost::asio） |
| `Framework/Message/` | 消息分发、消息处理线程池 |
| `Framework/Venue/` | 场地生命周期管理、内外部 Handler |
| `Framework/Player/` | 玩家加载、认证 |
| `Framework/Game/` | 游戏基类（GameRoom、GameAvatar、GameMessages） |
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
2. 在 `Server/<游戏名>/` 下实现游戏逻辑（Room、RoomHandler、Messages、Loader）
3. 如需可复用 `Poker/` 或 `Mahjong/` 中的共享逻辑
4. 在 `Server/main.cpp` 中注册 `VenueLoader` 并创建 `VenueInnerHandler` 实例
5. 在 `main.cpp` 中注册网络消息创建器

## 配置

运行时配置文件为 `Server/server.ini`，包含以下配置段：`[Server]`（服务器 ID、端口、线程数）、`[Websocket]`、`[Mysql]`、`[Redis]`、`[RabbitMQ]`。服务器启动后在 Redis 中注册自身信息，每 3 秒更新一次保活时间。

## 第三方依赖

内置于 `3rdpart/`：hiredis、jsoncpp、zlib-1.2.11、msgpack（header-only）。外部依赖（通过 CMake `find_package`）：Boost（log、log_setup、filesystem、system）、OpenSSL、mysql-concpp、rabbitmq-c。
