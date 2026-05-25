# C. WebSocket 协议设计

> 版本：v1.0 | 实时通信协议完整定义
> 连接: `wss://{host}/ws?token={jwt}&device_id={dev}`

---

## 一、连接生命周期

### 1.1 状态机

```
  CONNECTED(已认证) ──┬── IN_ROOM(房间内)
                        ├── LOBBY(大厅)
                        └── MATCHING(匹配中)
                              │
                    断线 ──────▼
                  RECONNECTING(重连保留30s)
                        │
              恢复成功 ◄────┤  超时失败 ► DISCONNECTED
```

### 1.2 心跳机制

| 参数 | 值 |
|------|-----|
| 心跳间隔 | **30s** (客户端发) |
| 服务端回复 | Pong (原样返回) |
| 超时判定 | 服务端 45s 无心跳 = 掉线 |
| 客户端超时 | 60s 无回复 = 触发重连 |

**消息**: `{ "type": "heartbeat", "seq": 1, "ts": 1700000000 }`

---

## 二、统一信封格式

```typescript
interface WsMessage {
  type: string;    // 消息类型编码
  seq: number;     // 序列号(客户端请求递增, 服务端回复复用请求seq)
  ts: number;      // 时间戳(ms)
  data?: any;     // 消息体
}
```

---

## 三、客户端→服务端 (11类)

### 3.1 连接认证

#### heartbeat — 心跳
```json
{ "type":"heartbeat", "seq":1, "ts":1700000000 }
// → 回复相同结构
```

#### reconnect — 重连(断线后)
```json
{ "type":"reconnect", "seq":2,
  "data":{
    "session_id":"sess_abc123",
    "last_seq_received":1567,
    "client_state":{"room_no":"886688","seat":1,"round_step":23}
  }
}
```

### 3.2 房间操作

```json
{ "type":"room.join",       "seq":10, "data":{"room_no":"886688"} }
{ "type":"room.leave",      "seq":11, "data":{} }
{ "type":"room.ready",      "seq":12, "data":{} }
{ "type":"room.cancel_ready","seq":13,"data":{} }
{ "type":"room.kick",       "seq":14, "data":{"target_seat":2} }        // 仅房主
{ "type":"room.dissolve.initiate", "seq":15, "data":{"reason":""} }
{ "type":"room.dissolve.vote",    "seq":16, "data":{"agree":true} }
```

### 3.3 游戏操作（核心）

所有游戏操作统一入口:

```json
{ "type":"game.action", "seq":100,
  "data":{
    "action_type": "DISCARD",   // 操作类型
    // ---- 依 action_type 不同 ----

    // 出牌:
    "tile": "5w",

    // 吃牌(可能有多种组合选择):
    "tile":"3w", "combination":["2w","3w","4w"],

    // 碰/杠:
    "tiles":["7w","7w"],
    "gang_type":"ming_gang",   // ming_gang/an_gang/bu_gang

    // 胡(多选听牌时):
    "hu_tiles":["1w","5w","9w"],

    // 托管切换:
    "auto_play": true,

    // 聊天:
    "chat_type":"quick_text",
    "chat_content":"快点出啊!",

    // 千分叫分:
    "score": 20
  }
}
```

**action_type 完整枚举**:

| action_type | 适用游戏 | 说明 | 必需字段 |
|-------------|---------|------|---------|
| READY | 全部 | 准备 | - |
| DRAW | 全部 | 摸牌(服务端推送) | - |
| DISCARD | 全部 | 出牌 | `tile` |
| CHI | 麻将/歪胡 | 吃牌 | `tile`, `combination`(可选) |
| PENG | 麻将/歪胡 | 碰牌 | `tiles` |
| GANG | 麻将/歪胡/千分 | 杠 | `gang_type`, `tile` |
| HU | 麻将/歪胡 | 胡牌 | `hu_tiles`(可选) |
| PAO | 歪胡 | 跑牌 | - |
| WEI | 歪胡 | 偎牌 | - |
| TI | 歪胡 | 提牌 | - |
| PASS | 全部 | 过/不要 | - |
| DISSOLVE_VOTE | 全部 | 解散投票 | - |
| AUTO_PLAY_TOGGLE | 全部 | 托管开关 | `auto_play` |
| CHAT | 全部 | 聊天 | `chat_type`,`content` |
| SELECT_TILE | 跑得快/千分 | 选牌提示 | `tile` |
| CALL_SCORE | 千分 | 叫分 | `score` |
| BID_BANKER | 千分 | 抢庄 | `bid`:bool |

### 3.4 匹配

```json
{ "type":"match.start", "seq":20,
  "data":{"game_code":"mahjong_hongzhong","player_count":4,"rule_preset":"classic"} }
{ "type":"match.cancel", "seq":21, "data":{} }
```

---

## 四、服务端→客户端 推送 (10类)

### 4.1 认证与连接

```json
// conn.authed — 认证成功
{ "type":"conn.authed", "seq":0,
  "data":{
    "session_id":"sess_abc123def",
    "server_time":1700000000000,
    "config":{"heartbeat_interval":30,"action_timeout":15,"reconnect_window":30},
    "user_info":{"id":10001,"nickname":"玩家A"}
  }
}

// reconnect.resumed — 重连恢复成功
{ "type":"reconnect.resumed",
  "data":{
    "missed_messages":[/*断线期间错过的消息*/],
    "current_room_state":{
      "room_no":"886688","status":"PLAYING",
      "players":[
        {"seat":0,"uid":10001,"nick":"A","online":true,"ready":true,"score":50},
        {"seat":1,"uid":10002,"nick":"B","online":true,"ready":true,"score":-20}
      ],
      "round_info":{"round_no":3,"dealer_seat":0,"current_turn":1,"remaining_tiles":48,
        "my_hand_count":14,
        "discards_by_seat":{"0":["3w","7s"],"1":["2w","5w"]},
        "melds_by_seat":{"1":[{"type":"PENG","tiles":["5w","5w"]}]}
      }
  }
}

// reconnect.failed — 重连失败
{ "type":"reconnect.failed","data":{"reason":"room_already_finished","fallback_url":"/lobby"} }
```

### 4.2 房间事件

```json
// room.joined — 加入成功
{ "type":"room.joined","seq":10,
  "data":{"room_no":"886688","my_seat":1,"is_owner":false,
    "game_code":"mahjong_changsha","game_name":"长沙麻将",
    "players":[...] } }

// room.player_joined / player_left / player_ready_changed
{ "type":"room.player_joined","data":{"player":{"seat":2,"uid":10003,"nick":"C"}} }
{ "type":"room.player_left","data":{"seat":2,"uid":10003,"reason":"leave"} }
{ "type":"room.player_ready_changed","data":{"seat":2,"is_ready":true} }

// room.left — 离开确认/被踢
{ "type":"room.left","data":{"reason":"kicked_by_owner"} }

// room.dissolve.* 系列
{ "type":"room.dissolve.requested",
  "data":{"initiator_seat":1,"initiator_nick":"B","deadline_ts":1700000030000,"votes_needed":3} }
{ "type":"room.dissolve.voted","data":{"seat":2,"agree":true,"remain_votes":2} }
{ "type":"room.dissolve.result","data":{"dissolved":true,"agree_count":4} }

// room.force_dissolved — 强制解散
{ "type":"room.force_dissolved","data":{"reason":"all_offline","message":"所有玩家离线"}}
```

### 4.3 游戏核心事件

```json
// game.start — 发牌开始
{ "type":"game.start",
  "data":{
    "round_no":1,"dealer_seat":0,
    "my_hand":["1w","2w","3w","4w","5s","6s","7s","8s","9s","1c","2c","3c","hong"],
    "other_players_hand_count":{"0":13,"2":13,"3":13},"remaining_tiles":56,
    "current_player":0
  }
}

// game.deal — 摸牌
{ "type":"game.deal",
  "data":{
    "drawn_tile":"4w","hand_after_draw":[...],
    "hand_counts":{"0":14,"1":13,"2":14,"3":13},"remaining_tiles":55,
    "available_actions":[],"turn_info":{"current_seat":0,"deadline_ms":15000}
  }
}

// game.turn — 轮到某人操作 ★ 最重要的事件
{ "type":"game.turn",
  "data":{
    "seat":1,
    "available_actions":[
      {"action":"DISCARD","hint_tiles":[]},
      {"action":"CHI","hint_combos":[["2w","3w","4w"]]},
      {"action":"PENG","hint_tiles":["7w"]},
      {"action":"GANG","hint_tiles":["9w"]},
      {"action":"HU","hint_patterns":[{...}]}
    ],
    "deadline_ts":1700000015000,"is_me":true
  }
}

// game.action_result — 操作结果广播 ★ 所有人都收
{ "type":"game.action_result",
  "data":{
    "seat":0,"player_uid":10001,"player_nick":"A",
    "action":"DISCARD","action_data":{"tile":"5w"},
    "table_state_update":{
      "discards_0":["5w"],"hand_count_0":12
    },
    "next_turn":{
      "seat":1,
      "available_actions_for_others":[
        {"action":"CHI","from_seat":0,"combinations":[["4w","5w","6w"]]},
        {"action":"PENG","from_seat":0,"tile":"5w"},
        {"action":"PASS"}
      ]
    },
    "elapsed_ms":1200
  }
}

// game.settlement — 单局结算
{ "type":"game.settlement",
  "data":{
    "round_no":1,"end_reason":"normal","duration_sec":180,
    "settlement_detail":[
      {"seat":0,"uid":10001,"round_score":80,"hu_type":null,"fee_paid":2},
      {"seat":1,"uid":10002,"round_score":-30,...}
    ],
    "next_round":{"will_start":true,"new_dealer_seat":1},
    "replay_available":true
  }
}

// game.round_settlement — 整房总结算(最后一局)
{ "type":"game.round_settlement",
  "data":{
    "total_rounds":8,"total_duration_sec":1440,
    "final_scores":[
      {"seat":0,"uid":10001,"total_score":250,"wins":3,"rank":1},
      ...
    ],
    "winner_seat":0,
    "room_fee_summary":{"mode":"AA","total_fee":16,"per_player_fee":4}
  }
}
```

### 4.4 系统通知

```json
// error — 操作错误
{ "type":"error","seq":100,
  "data":{"code":15003,"message":"未轮到该玩家操作","show_toast":true,"recover_action":"PASS"}}

// risk.notice — 风控提示
{ "type":"risk.notice","data":{"level":"warning","message":"您的账号存在异常行为"}}

// user.reconnect / offline — 玩家重连/掉线
{ "type":"user.reconnect","data":{"seat":2,"uid":10003,"nick":"C"} }
{ "type":"user.offline","data":{"seat":2,"uid":10003,"auto_play_enabled":true} }

// notification — 站内通知
{ "type":"notification","data":{"title":"签到奖励","body":"恭喜获得100积分!","link":"/activity/checkin"} }
```

---

## 五、消息序号与可靠性

### Seq 规则

| 方向 | 规则 |
|------|------|
| C → S | 客户端递增(从1), 每个需要回复的请求携带唯一seq |
| S → C | 服务端独立递增计数器 |
| S 回复 C 的请求 | 复用请求的 seq (便于匹配 request-response) |
| 心跳/系统通知 | seq = 0 |

### 丢包处理

1. **检测缺口**: 客户端发现收到的 seq 不连续
2. **上报缺失**: 在心跳/下次操作中附带 `last_seq_received`
3. **服务端补发**: 返回 missed_messages 数组
4. **关键消息必补**: game.turn / game.action_result / settlement 必须补发
5. **非关键可丢**: chat / notification 可不补发

---

## 六、异常场景处理

| 场景 | 服务端行为 | 客户端行为 |
|------|----------|----------|
| **操作超时(15s)** | 自动 PASS/AUTO_PLAY | 显示倒计时, 超时更新UI为托管状态 |
| **并发操作冲突** | 以服务端收到的第一条为准, 后续拒绝 | 锁定UI防重复点击 |
| **非法操作(如不能吃却发了吃)** | 返回 error + 强制 PASS | 显示错误Toast, 自动过 |
| **断线重连中有人操作** | 缓存操作结果, 在 resumed 中一次性下发 | 按 missed_messages 顺序回放动效 |
| **所有人离线** | 30秒后自动 force_dissolved | 回到大厅显示"房间已解散" |
| **服务端重启** | reconnect.failed(session_expired) | 提示用户重新进入大厅 |
| **网络抖动(短暂断开)** | 保留 session 30s | 自动重连, 用户无感知 |
| **消息乱序** | 客户端按 seq 排序显示 | 缓冲区排序后依次渲染 |

---

## 七、各游戏特殊 WS 消息扩展

### 7.1 红中麻将额外消息

```json
// game.laizi_state — 赖子状态变化(每次有赖子变化时推送)
{ "type":"game.laizi_state",
  "data":{
    "laizi_tile":"hongzhong",          // 当前赖子牌
    "laizi_holder_seat":2,            // 谁拿着红中(如果适用)
    "available_replacements":["1w","9s","5s"]  // 赖子可以替代的牌
  }
}

// game.ting_info — 听牌信息
{ "type":"game.ting_info",
  "data":{
    "is_ting": true,
    "ting_patterns":[                 // 可胡的牌型列表
      {"tiles":["1w","5w","9w"],"pattern":"七对","fan":2,"score":8},
      {"tiles":["4w"],"pattern":"碰碰胡","fan":4,"score":16}
    ]
  }
}
```

### 7.2 长沙麻将额外消息

```json
// game.qishou_hu — 起手胡触发
{ "type":"game.qishou_hu",
  "data":{
    "triggered_types":["banbanhu","liuliushun"],  // 触发的起手胡类型
    "player_seat":0,
    "bonus_score":200
  }
}

// game.zhongniao — 中鸟动画
{ "type":"game.zhongniao",
  "data":{
    "bird_tile":"5s",
    "hit_seats":[1,3],               // 命中的座位
    "bonus_scores":{"1":10,"3":10}
  }
}
```

### 7.3 跑得快额外消息

```json
// game.bomb — 炸弹特效
{ "type":"game.bomb",
  "data":{
    "seat":1,"bomb_type":"normal",   // normal/king_bomb
    "cards":["4","4","4","4"],
    "score_penalty":20                // 对其他人的罚分
  }
}

// game.hint_cards — 出牌提示
{ "type":"game.hint_cards",
  "data":{
    "recommended":["3s","7d"],       // AI推荐的出牌
    "must_play":null                  // 如果有必出规则(如首出带黑桃3)
  }
}
```

### 7.4 益阳歪胡子额外消息

```json
// game.huxi_update — 胡息面板更新(每次产生胡息时推送)
{ "type":"game.huxi_update",
  "data":{
    "seat":1,
    "huxi_delta":3,                   // 本次增加的胡息
    "huxi_total":15,                   // 当前累计胡息
    "huxi_source":"wei",               // 来源: chi/peng/gang/pao/ti/hu_base
    "source_detail":{"tiles":["大字","大字"]}  // 来源牌详情
  }
}
```

### 7.5 音效/特效同步标记

所有 game.action_result 中可选携带:

```json
{
  "vfx_trigger": {                     // 特效触发标记(客户端根据此决定播放什么特效)
    "event_code": "mahjong.peng",      // 对应 audio_resources.event_code
    "effect_level": "L4",             // 特效等级
    "player_seat": 0                   // 特效目标位置
  },
  "audio_sync": {                      // 音效同步(确保多人音效一致)
    "play_at_ts": 1700000012000        // 绝对时间戳, 所有客户端同时播放
  }
}
```

---

## 八、WebSocket 与 REST API 职责划分

| 功能 | 用 WS | 用 REST API |
|------|:---:|:-----------|
| 登录/注册/Token刷新 | - | ✅ POST /auth/login, /auth/refresh |
| 房间内实时操作 | ✅ game.action, room.ready | - |
| 创建/加入房间 | ✅ room.join (也支持REST备用) | ✅ POST /rooms, POST /rooms/{no}/join |
| 查询历史数据(流水/战绩) | - | ✅ GET /users/me/wallet/ledgers, /records |
| 后台管理操作 | - | ✅ 全部 /admin/api/v1/* |
| 文件上传 | - | ✅ POST /files/upload (multipart) |
| 版本检测/资源下载 | - | ✅ POST /app/version/check, GET resource-config |
| 收到推送通知 | ✅ notification | - |
| 心跳保活 | ✅ heartbeat | - |
