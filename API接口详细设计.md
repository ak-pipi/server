# B. API 接口详细设计

> 版本：v1.0 | 全部 RESTful API 定义
> 基础路径: `/api/v1` (玩家端) / `/admin/api/v1` (后台端)

---

## 一、通用规范

### 1.1 统一响应格式

```json
{
  "code": 0,
  "message": "success",
  "data": {},
  "request_id": "req_abc123",
  "timestamp": 1700000000000
}
```

### 1.2 分页参数（所有列表接口通用）

| 参数 | 类型 | 必填 | 默认 | 说明 |
|------|------|------|------|------|
| page | int | 否 | 1 | 页码(从1开始) |
| page_size | int | 否 | 20 | 每页条数(最大100) |
| sort_by | string | 否 | created_at | 排序字段 |
| sort_order | string | 否 | desc | asc/desc |

**分页响应**:
```json
{
  "code": 0,
  "data": {
    "list": [],
    "pagination": {
      "page": 1,
      "page_size": 20,
      "total": 500,
      "total_pages": 25
    }
  }
}
```

### 1.3 错误码体系

| 范围 | 模块 | 示例码 |
|------|------|--------|
| **0** | 成功 | 0 |
| **10xxx** | 参数校验错误 | 10001=参数缺失, 10002=参数格式错误, 10003=分页越界, 10004=枚举值非法, 10005=JSON格式错误 |
| **11xxx** | 认证授权 | 11001=Token缺失, 11002=Token过期, 11003=Token无效, 11004=权限不足, 11005=账号被冻结/封禁, 11006=设备绑定异常, 11007=异地登录风险 |
| **12xxx** | 业务逻辑-用户 | 12001=用户不存在, 12002=手机号已注册, 12003=验证码错误/过期, 12004=密码错误, 12005=昵称违规, 12006=邀请人不存在 |
| **13xxx** | 业务逻辑-钱包 | 13001=余额不足, 13002=余额冻结中, 13003=超出日限额, 13004=流水不一致, 13005=钱包类型不存在, 13006=保险箱密码错误(连续N次锁定) |
| **14xxx** | 业务逻辑-房间 | 14001=房间不存在, 14002=房间已满, 14003=房间已开始, 14004=房间已解散, 14005=不是房主, 14006=玩家不在房间, 14007=已在其他房间, 14008=重复准备, 14009=非房主不能操作, 14010=房间号格式错误, 14011=匹配超时, 14012=解散投票失败 |
| **15xxx** | 业务逻辑-游戏 | 15001=操作不合法(牌型/时机), 15002=未轮到该玩家, 15003=操作超时(已托管), 15004=游戏已结束, 15005=规则配置无效, 15006=插件加载失败, 15007=结算异常, 15008=回放数据不存在 |
| **16xxx** | 业务逻辑-运营 | 16001=活动未开始/已结束, 16002=已达领取上限, 16003=活动预算耗尽, 16004=渠道不存在/已封禁, 16005=工单不存在, 16006=工单状态不允许此操作 |
| **17xxx** | 风控 | 17001=触发风控规则, 17002=账号已被冻结, 17003=操作频率过高, 17004=IP被封禁 |
| **20xxx** | 系统异常 | 20001=内部错误, 20002=数据库错误, 20003=Redis错误, 20004=MQ发送失败, 20005=第三方服务调用失败, 20006=服务降级/熔断 |

### 1.4 请求头

```
# 玩家端
Authorization: Bearer {jwt_token}
X-Device-ID: {device_id}
X-App-Version: 2.3.0
X-Platform: android
X-Language: zh-CN

# 后台端
Authorization: Bearer {admin_jwt_token}
X-Request-ID: {uuid}   // 可选，用于链路追踪
```

---

## 二、玩家端 API

### 2.1 认证模块 — `/api/v1/auth`

#### POST 登录
```
POST /api/v1/auth/login

Request:
{
  "login_type": "phone",        // phone/guest/wechat/apple/device
  "phone": "138****1234",       // login_type=phone 时必填(明文)
  "sms_code": "123456",         // 验证码(login_type=phone)
  "device_id": "dev_xxx",       // 必填
  "app_version": "2.3.0",       // 必填
  "os_version": "Android 15",   // 可选
  "client_width": 2400,          // 可选
  "client_height": 1080,         // 可选
  "safe_area": {                // 可选
    "left": 80, "right": 0, "top": 0, "bottom": 24
  },
  "inviter_code": "ABC123"      // 可选，邀请码
}

Response 200:
{
  "data": {
    "token": "eyJhbGciOiJIUzI1NiIs...",
    "refresh_token": "eyJhbG...",
    "token_expires_in": 604800,     // 7天秒
    "user": {
      "id": 10001,
      "nickname": "玩家A",
      "avatar": "https://...",
      "phone": "138****1234",
      "new_user": true              // 首次登录标记
    }
  }
}
```

#### POST 刷新 Token
```
POST /api/v1/auth/refresh
{ "refresh_token": "..." }

Response:
{
  "data": {
    "token": "新JWT",
    "token_expires_in": 604800
  }
}
```

#### POST 发送验证码
```
POST /api/v1/auth/sms/send
{
  "phone": "13800138000",
  "scene": "login"               // login/register/reset_password/bind_phone
}
// 限制: 同一手机号60s内只能发一次, 每日最多10次

Response:
{
  "data": {
    "captcha_required": false,    // 是否需要图形验证码
    "expire_seconds": 300         // 验证码有效期
  }
}
```

#### POST 登出
```
POST /api/v1/auth/logout
// 服务端将 token 加入 Redis 黑名单
```

---

### 2.2 用户模块 — `/api/v1/users/me` (需认证)

#### GET 个人信息
```
GET /api/v1/users/me
→ 200: { user: { id,nickname,avatar,phone,status,risk_level,total_games,total_wins,created_at } }
```

#### PUT 修改昵称
```
PUT /api/v1/users/me/nickname
{ "nickname": "新昵称" }
// 校验: 长度2-16, 不含敏感词
```

#### PUT 修改头像
```
PUT /api/v1/users/me/avatar
{ "avatar_url": "https://cdn.../avatar_xxx.jpg" }
// avatar_url 来自文件上传接口返回值
```

#### GET 我的设备列表
```
GET /api/v1/users/me/devices
→ [{ id, device_model, os_type, app_version, is_online, last_active_at }]
```

#### DELETE 解绑设备
```
DELETE /api/v1/users/me/devices/{device_id}
// 只能解绑非当前在线设备
```

#### GET 积分明细
```
GET /api/v1/users/me/wallet/{wallet_type}/ledgers?start_date=&end_date=&page=&page_size=
// wallet_type: fun_score / room_card / activity_coupon / safebox

→ { list: [...], pagination: {...} }
// 每条: { id, change_type, change_amount, balance_after, biz_title, created_at }
```

#### GET 战绩记录
```
GET /api/v1/users/me/records?game_code=&status=&page=&page_size=

→ list: [{
    room_no, game_name, game_type, role,          // role: owner/member
    player_count, total_rounds, current_round,
    score, result,                                  // win/lose/draw/dissolved
    room_fee, start_time, end_time, duration_sec,
    replay_available: bool
  }]
```

---

### 2.3 大厅模块 — `/api/v1/lobby` (需认证)

#### GET 游戏列表
```
GET /api/v1/lobby/games

→ list: [{
    id, code, name, type, status, icon_url, banner_url,
    min_players, max_players,
    online_count: 1200,           // 当前在线人数(实时)
    today_rooms: 3500,            // 今日开房数
    is_maintenance: false,         // 是否维护中
    maintenance_msg: ""            // 维护提示
  }]
```

#### GET 大厅公告
```
GET /api/v1/lobby/announcements?type=all
// type: all/system/activity/maintenance

→ list: [{ id, title, content, type, popup, start_time, end_time }]
```

#### GET 站内消息
```
GET /api/v1/lobby/messages?page=&page_size=&is_read=
// 标记已读: PUT /api/v1/lobby/messages/{id}/read
// 全部已读: PUT /api/v1/lobby/messages/read-all
// 未读数:   GET /api/v1/lobby/messages/unread-count
```

#### GET 版本检测
```
POST /api/v1/app/version/check
{
  "platform": "android",
  "channel": "official",
  "app_version": "2.3.0",
  "hotfix_version": "hotfix_2026_001",
  "res_version": "res_2026_001",
  "device_model": "Xiaomi 14",
  "os_version": "Android 15",
  "screen_width": 2400,
  "screen_height": 1080,
  "safe_area": { "left":80,"right":0,"top":0,"bottom":24 }
}

→ {
    need_update: true/false,
    force_update: false,             // 是否强制更新
    update_type: "hotfix",           // normal/hotfix/resource
    latest_app_version: "2.3.1",
    latest_hotfix_version: "...",
    latest_res_version: "...",
    title: "发现新版本",
    description: "修复...",
    download_url: "https://cdn...",
    file_size: 24800000,
    sha256: "xxxx",
    gray_strategy: "20_percent"
  }
```

#### POST 上报更新结果
```
POST /api/v1/app/version/report
{ from_version, to_version, status, error_message, duration_ms }
```

#### POST 上报性能数据
```
POST /api/v1/app/perf/report
{ device_model, os, client_version, audio_version, vfx_version,
  fps_avg, memory_mb, crash_count, load_failed_count, failed_resources }
```

#### GET 音效特效资源配置
```
GET /api/v1/app/resource-config?game_code=mahjong_changsha&client_version=2.3.0&device_level=high

→ {
    audio_version: "audio_2026_001",
    vfx_version: "vfx_2026_001",
    effect_level: "high",
    audio: [{ event_code, url, volume, enabled }],
    vfx: [{ event_code, url, level, enabled }]
  }
```

#### POST 上报资源加载结果
```
POST /api/v1/app/resource-report
{ user_id, device_model, client_version, audio_version, vfx_version,
  load_success, failed_resources, fps_avg, memory_mb }
```

---

### 2.4 房间模块 — `/api/v1/rooms` (需认证)

#### POST 创建房间
```
POST /api/v1/rooms
{
  "game_code": "mahjong_changsha",
  "rule_config": {                  // 各游戏不同，参考 rules.md 第4章各节
    "player_count": 4,
    "round_count": 8,
    "base_score": 1,
    "room_fee_type": "AA",
    "queyise_enabled": true,        // 长沙麻将特有
    "banbanhu_enabled": true,
    ...
  }
}

→ {
    room: { id, room_no, game_name, status, max_players, config_snapshot, created_at },
    share_text: "【长沙麻将】房间号: 886688, 快来加入吧!",
    share_qrcode: "https://..."
  }

业务规则:
  - 检查用户是否在其他进行中的房间(是则拒绝)
  - 检查 rule_config 合法性(通过该游戏的 schema 校验)
  - 扣除房卡(如果 fee_mode != free)，写入 room_fee_ledger
  - 创建者自动加入并设为 seat 0(庄家位)
  - 房号生成: 6位数字+字母混合, 避免与现有房间冲突
  - 有效期: 创建后30分钟无人加入自动清理(Redis TTL)
```

#### POST 加入房间
```
POST /api/v1/rooms/{room_no}/join

→ {
    room: { id, room_no, game_code, game_name, status, players: [...] },
    seat_assigned: 2                   // 分配的座位号
  }

业务规则:
  - 房间必须存在且状态为 WAITING/READY
  - 房间未满
  - 用户不在此房间(防止重复加入)
  - 用户不在其他 PLAYING 状态的房间
  - 如果需要房费且非 owner 支付模式, 加入时扣费(AA模式)
  - 返回当前房间所有玩家信息(含座位和准备状态)
```

#### POST 离开房间
```
POST /api/v1/rooms/{room_no}/leave

// WAITING状态: 直接离开, 若是房主且有人则转让房主
// READY/PLAYING状态: 仅允许发起解散投票(或托管后等待)
```

#### POST 准备
```
POST /api/v1/rooms/{room_no}/ready
// 取消准备: DELETE /api/v1/rooms/{room_no}/ready
// 全部准备后自动通知 Game Engine 开始发牌
```

#### POST 发起解散投票
```
POST /api/v1/rooms/{room_no}/dissolve
{ "reason": "" }                    // 可选

// 投票: POST /api/v1/rooms/{room_no}/dissolve/vote
// { agree: true/false }

// 规则: 过半数同意即解散; 30秒内未投票视为同意;
//      PLAYING状态下发起需所有人同意(防止恶意打断);
//      每局每人限发起1次解散
```

#### GET 房间详情
```
GET /api/v1/rooms/{room_no}
// 返回完整房间信息 + 所有玩家状态 + 当前局进度
```

#### POST 快速匹配
```
POST /api/v1/rooms/match
{
  "game_code": "mahjong_hongzhong",
  "player_count": 4,
  "rule_preset": "classic"           // classic/fast/friendly
}
// 进入匹配队列 → WebSocket 通知匹配成功 → 自动创建并加入房间
// 匹配超时: 60秒 → 返回错误 + 推荐好友房
```

---

### 2.5 钱包模块 — `/api/v1/wallet` (需认证)

#### GET 钱包总览
```
GET /api/v1/wallet/summary

→ {
    wallets: [
      { type: "fun_score", label: "娱乐积分", balance: 125800, frozen: 5000 },
      { type: "room_card",  label: "房卡", balance: 50, frozen: 2 },
      { type: "activity_coupon", label: "活动券", balance: 10, frozen: 0 }
    ],
    safebox: { balance: 50000, status: 1 }    // 保险箱概要
  }
```

#### GET 单个钱包详情
```
GET /api/v1/wallet/{wallet_type}
→ { type, balance, frozen_amount, total_in, total_out, ledgers_today }
```

#### GET 流水明细
```
GET /api/v1/wallet/{wallet_type}/ledgers?page=&page_size=&change_type=&start_date=&end_date=
→ { list: [...], pagination: {} }
```

---

### 2.6 保险箱模块 — `/api/v1/safebox` (需认证)

#### GET 保险箱信息
```
GET /api/v1/safebox
→ { balance, status, locked_reason(如适用), flow_today: { in, out } }
```

#### POST 存入
```
POST /api/v1/safebox/deposit
{ amount: 10000, password: "654321" }
// 校验: 密码正确, 钱包(fun_score)余额充足, amount > 0
// 写入两条流水: wallet(SAFEBOX_OUT) + safebox(SAFEBOX_IN)
```

#### POST 取出
```
POST /api/v1/safebox/withdraw
{ amount: 5000, password: "654321" }
// 校验同上 + 保险箱余额充足
// 连续5次密码错误锁定24小时
```

#### PUT 设置/修改二级密码
```
PUT /api/v1/safebox/password
{ old_password: "", new_password: "", confirm_password: "" }
// 首次设置时 old_password 为空
// 密码要求: 6-16位, 含数字+字母
```

#### POST 忘记密码申诉
```
POST /api/v1/safebox/password/reset-apply
{ reason: "忘记了" }
// 提交工单类型为 safebox_password 的客服工单
```

---

### 2.7 活动模块 — `/api/v1/activity` (需认证)

#### GET 活动列表
```
GET /api/v1/activities?status=active&type=
// 返回当前可参与的活动
→ list: [{ id, name, type, reward_type, reward_amount_icon, status, progress }]
```

#### POST 领取奖励
```
POST /api/v1/activities/{id}/claim
// 校验: 活动进行中, 未达领取上限, 预算充足
→ { claimed: true, reward_amount: 100, message: "领取成功!" }
```

#### GET 我的活动记录
```
GET /api/v1/activities/my-records?activity_id=&date=
→ list: [{ activity_name, reward_amount, record_date, claim_status }]
```

---

### 2.8 客服模块 — `/api/v1/customer` (需认证)

#### POST 提交工单
```
POST /api/v1/customer/tickets
{
  "ticket_type": "score_issue",    // login/card/score/dispute/frozen/safebox/report
  "title": "积分少了",
  "content": "刚才一局结束后发现积分不对...",
  "attach_urls": ["https://cdn.../img1.jpg"],
  "related_room_no": "886688"       // 可选
}
→ { ticket_no: "TS20260525001", message: "工单已提交, 请耐心等待" }
```

#### GET 我的工单列表
```
GET /api/v1/customer/tickets?status=&page=
→ list: [{ ticket_no, type, title, status, priority, reply_count, created_at, updated_at }]
```

#### GET 工单详情+回复
```
GET /api/v1/customer/tickets/{ticket_no}
→ {
    ticket: { ... },
    replies: [
      { type: "user", content: "...", attach_urls:[], created_at:"..." },
      { type: "cs", replier_name: "客服小王", content: "您好, 已为您查询...", created_at:"..." }
    ]
  }
```

#### POST 补充回复
```
POST /api/v1/customer/tickets/{ticket_no}/reply
{ content: "补充说明...", attach_urls: [] }
// 只能在 pending/processing 状态回复
```

#### POST 关闭工单
```
POST /api/v1/customer/tickets/{ticket_no}/close
{ satisfaction: 5, comment: "很满意" }  // satisfaction可选
```

---

### 2.9 文件上传 — `/api/v1/files` (需认证)

#### POST 上传文件
```
POST /api/v1/files/upload
Content-Type: multipart/form-data

Parameters:
  file: (binary)                    // 必填, 最大 10MB
  usage_type: avatar/image/audio/document  // 必填

→ {
    file_id: 1001,
    file_url: "https://cdn.example.com/uploads/avatar_10001_20260525.jpg",
    file_size: 152400,
    mime_type: "image/jpeg"
  }

// 允许格式:
// 图片: jpg/jpeg/png/gif/webp (最大 5MB)
// 音频: ogg/wav/mp3/aac (最大 20MB)
// 文档: pdf/doc/xls/txt (最大 10MB)
// 安全检查: 文件头魔数校验 + 病毒扫描 + 内容审核
```

---

## 三、后台管理 API — `/admin/api/v1`

### 3.1 认证模块 — `/admin/api/v1/auth`

#### POST 后台登录
```
POST /admin/api/v1/auth/login
{ "username": "admin", "password": "xxxxx" }
// 返回 JWT + 角色 + 权限菜单树
→ {
    token: "eyJ...",
    admin: { id, username, real_name, avatar, role_name },
    permissions: [ "game:view", "game:create", "user:list", ... ],
    menus: [ /* 前端路由菜单树 */ ]
  }
```

#### POST 修改密码
```
PUT /admin/api/v1/auth/password
{ old_password, new_password, confirm_password }
// pwd_changed_at 更新
```

#### GET 当前管理员信息
```
GET /admin/api/v1/auth/profile
→ { admin, role, permissions, menus }
```

---

### 3.2 经营驾驶舱 — `/admin/api/v1/dashboard`

#### GET 核心指标
```
GET /admin/api/v1/dashboard/metrics?date_range=today/week/month/custom&start=&end=

→ {
    today_new_users: 156,
    today_active_users: 2340,
    current_online: 1258,
    current_online_rooms: 320,
    today_room_created: 1580,
    today_rounds_finished: 4200,
    today_room_card_consumed: 3150,
    today_score_flow: 12500000,
    abnormal_rooms: 3,
    pending_tickets: 12,

    trends: {
      online_users: [/* 近7天每日数据点 */],
      rounds_per_day: [],
      room_card_cost: [],
      game_distribution: [
        { game: "红中麻将", count: 1800, percent: 43 },
        { game: "跑得快", count: 1200, percent: 29 },
        ...
      ],
      new_user_sources: []
    }
}
```

---

### 3.3 游戏管理 — `/admin/api/v1/games`

| 方法 | 路径 | 说明 | 权限 |
|------|------|------|------|
| GET | /games | 游戏列表(支持筛选状态/类型) | game:view |
| GET | /games/{id} | 游戏详情(含在线/今日数据) | game:view |
| POST | /games | 新增游戏 | game:create |
| PUT | /games/{id} | 编辑基本信息 | game:update |
| PUT | /games/{id}/status | 上下架/维护切换 | game:manage |
| GET | /games/{id}/rooms | 该游戏房间列表 | game:view |
| GET | /games/{id}/stats | 该游戏统计数据 | game:view |

**列表返回字段**: id, code, name, type, status(icon/banners), min/max_players, online_count(today_rooms, today_rounds, today_fees), plugin_version, updated_at

---

### 3.4 玩法规则管理 — `/admin/api/v1/rules`

| 方法 | 路径 | 说明 | 权限 |
|------|------|------|------|
| GET | /rules | 规则版本列表(按game_id) | rule:view |
| GET | /rules/{id} | 版本详情(config_json) | rule:view |
| POST | /rules/draft | 保存草稿 | rule:edit |
| POST | /rules/{id}/submit | 提交审批 | rule:edit |
| POST | /rules/{id}/approve | 审批通过 | rule:approve |
| POST | /rules/{id}/reject | 审批驳回 | rule:approve |
| POST | /rules/{id}/publish | 发布(灰度) | rule:publish |
| POST | /rules/{id}/rollback | 回滚到旧版本 | rule:publish |
| GET | /rules/{gameId}/history | 某游戏版本历史 | rule:view |

---

### 3.5 房间管理 — `/admin/api/v1/rooms`

| 方法 | 路径 | 说明 | 权限 |
|------|------|------|------|
| GET | /rooms | 在线房间列表(支持game/status/risk/时间筛选) | room:view |
| GET | /rooms/{id} | 房间详情(含所有玩家+每局+流水+回放) | room:view |
| GET | /rooms/{id}/rounds | 牌局列表 | room:view |
| GET | /rooms/{id}/players | 玩家列表 | room:view |
| POST | /rooms/{id}/force-dissolve | 强制解散 | room:manage |
| POST | rooms/{id}/mark-risk | 标记争议/异常 | risk:handle |
| GET | /rooms/export | 导出房间数据(CSV) | room:export |

**房间列表字段**: room_no, game_name, owner_nick, players(seats/count), round(current/total), status, fee_mode, base_score, risk_flag, duration, created_at

---

### 3.6 玩家管理 — `/admin/api/v1/users`

| 方法 | 路径 | 说明 | 权限 |
|------|------|------|------|
| GET | /users | 玩家列表(支持手机/昵称/status/risk/时间) | user:view |
| GET | /users/{id} | 详情(基础+设备+IP+钱包+战绩+风控+工单+后台操作) | user:view |
| PUT | /users/{id}/freeze | 冻结账号 | user:manage |
| PUT | /users/{id}/unfreeze | 解冻 | user:manage |
| PUT | /users/{id}/ban | 封禁 | user:manage |
| PUT | /users/{id}/risk-note | 添加风控备注 | risk:handle |
| GET | /users/{id}/wallet-ledgers | 流水明细 | finance:view |
| POST | /users/{id}/wallet-adjust | 后台调整积分 | finance:adjust |
| GET | /users/{id}/rooms | 参与过的房间 | user:view |
| GET | /users/{id}/records | 战绩记录 | user:view |
| GET | /users/export | 导出玩家数据 | user:export |

**调整积分请求**:
```json
{
  "wallet_type": "fun_score",
  "amount": 1000,                     // 正数=增加, 负数=扣减
  "reason": "系统补偿-结算异常",
  "need_approval": true               // 大额(>阈值)需要二级审批
}
// 大额阈值由系统配置 system_configs 控制
```

---

### 3.7 风控中心 — `/admin/api/v1/risk`

| 方法 | 路径 | 说明 | 权限 |
|------|------|------|------|
| GET | /risk/events | 风控事件列表(按rule/user/level/status/时间) | risk:view |
| GET | /risk/events/{id} | 事件详情(命中详情+上下文) | risk:view |
| POST | /risk/events/{id}/handle | 处理风控事件 | risk:handle |
| GET | /risk/rules | 规则配置列表 | risk:manage |
| PUT | /risk/rules/{id} | 编辑规则(阈值/动作) | risk:manage |
| PUT | /risk/rules/{id}/toggle | 启用/禁用规则 | risk:manage |
| GET | /risk/users/{userId}/profile | 某用户风控画像 | risk:view |
| GET | /risk/statistics | 风控统计(命中趋势/处理率) | risk:view |

**处理动作选项**: mark_observed / limit_match / freeze_account / freeze_safebox / ban_create_room / force_offline / submit_review / submit_audit

---

### 3.8 牌局回放审计 — `/admin/api/v1/replay`

| 方法 | 路径 | 说明 | 权限 |
|------|------|------|------|
| GET | /replays | 回放列表(按房间/玩家/游戏/时间) | replay:view |
| GET | /replays/{roundId} | 回放数据(MongoDB获取actions数组) | replay:view |
| GET | /replays/{roundId}/actions | 操作步骤列表(支持step范围) | replay:view |
| POST | /replays/{roundId}/export-evidence | 导出争议证据包(PDF/ZIP) | replay:export |

**回放数据返回**:
```json
{
  "round_id": 5001,
  "room_no": "886688",
  "game_code": "mahjong_changsha",
  "deal_info": { seed_hash, dealer_seat, initial_tiles: [...] },
  "actions": [ { step, action, seat, data, elapsed_ms, server_ts }, ... ],
  "settlement": { players: [...], end_reason },
  "meta": { total_steps: 156, duration_sec: 480 }
}
```

---

### 3.9 活动奖励 — `/admin/api/v1/activities`

| 方法 | 路径 | 说明 | 权限 |
|------|------|------|------|
| GET | /activities | 活动列表 | activity:view |
| POST | /activities | 创建活动 | activity:create |
| PUT | /activities/{id} | 编辑 | activity:update |
| PUT | /activities/{id}/status | 上下线/停止 | activity:manage |
| GET | /activities/{id}/records | 参与记录 | activity:view |
| GET | /activities/{id}/statistics | 效果统计(领取率/成本) | activity:view |
| GET | /activities/export | 导出 | activity:export |

**活动字段**: id, name, type, reward_type/amount, time_range, limit(daily/user/total), budget(consumed/total), target_games, status

---

### 3.10 推广渠道 — `/admin/api/v1/channels`

| 方法 | 路径 | 说明 | 权限 |
|------|------|------|------|
| GET | /channels | 渠道列表 | channel:view |
| POST | /channels | 创建渠道码 | channel:create |
| GET | /channels/{id}/detail | 效果明细(邀请用户/活跃/消耗) | channel:view |
| PUT | /channels/{id}/ban | 封禁渠道 | channel:manage |
| GET | /channels/export | 导出 | channel:export |

---

### 3.11 客服后台 — `/admin/api/v1/cs`

| 方法 | 路径 | 说明 | 权限 |
|------|------|------|------|
| GET | /tickets | 工单列表(按type/priority/status/assignee) | cs:view |
| GET | /tickets/{no} | 工单详情+回复 | cs:view |
| POST | /tickets/{no}/reply | 客服回复 | cs:handle |
| POST | /tickets/{no}/assign | 分配客服 | cs:assign |
| POST | /tickets/{no}/transfer | 转部门(finance/risk/tech) | cs:handle |
| POST | /tickets/{no}/close | 关闭工单 | cs:handle |
| GET | /cs/statistics | 工作量统计 | cs:view |

---

### 3.12 权限角色 — `/admin/api/v1/admin`

| 方法 | 路径 | 说明 | 权限 |
|------|------|------|------|
| GET | /admins | 管理员列表 | admin:view |
| POST | /admins | 新增管理员 | admin:create |
| PUT | /admins/{id} | 编辑(姓名/角色/状态) | admin:update |
| PUT | /admins/{id}/reset-pwd | 重置密码 | admin:manage |
| GET | /roles | 角色列表 | admin:view |
| GET | /roles/{id} | 角色权限详情 | admin:view |
| PUT | /roles/{id}/permissions | 修改角色权限 | admin:manage |
| GET | /permissions | 权限树(全部节点) | admin:view |

---

### 3.13 审计日志 — `/admin/api/v1/audit`

| 方法 | 路径 | 说明 | 权限 |
|------|------|------|------|
| GET | /logs | 日志列表(module/action/operator/target/time) | audit:view |
| GET | /logs/{id} | 日志详情(前后快照对比) | audit:view |
| GET | /logs/export | 导出CSV | audit:export |

---

### 3.14 版本管理 — `/admin/api/v1/versions`

| 方法 | 路径 | 说明 | 权限 |
|------|------|------|------|
| GET | /versions/app | App版本列表 | version:view |
| POST | /versions/app | 创建版本 | version:create |
| PUT | /versions/app/{id} | 编辑 | version:update |
| POST | /versions/app/{id}/publish | 发布(灰度/全量) | version:publish |
| POST | /versions/app/{id}/rollback | 回滚 | version:rollback |
| GET | /versions/hotfix | 热更新列表 | version:view |
| POST | /versions/hotfix | 创建热更新包 | version:create |
| POST | /versions/hotfix/{id}/publish | 发布热更新 | version:publish |
| GET | /versions/bugs | Bug工单列表 | version:view |
| POST | /versions/bugs | 创建Bug | version:create |
| PUT | /versions/bugs/{id} | 更新状态/方案 | version:update |
| GET | /versions/stats | 版本统计(更新成功率/崩溃率/灰度效果) | version:view |

---

### 3.15 音效特效配置 — `/admin/api/v1/resources`

| 方法 | 路径 | 说明 | 权限 |
|------|------|------|------|
| GET | /resources/audio | 音效列表(可按game/scene/event筛选) | resource:view |
| POST | /resources/audio | 上传音效+配置 | resource:create |
| PUT | /resources/audio/{id} | 编辑配置 | resource:update |
| PUT | /resources/audio/{id}/toggle | 启用/停用 | resource:manage |
| POST | /resources/audio/{id}/gray-publish | 灰度发布 | resource:publish |
| GET | /resources/vfx | 特效列表 | resource:view |
| POST | /resources/vfx | 上传特效+配置 | resource:create |
| PUT | /resources/vfx/{id} | 编辑 | resource:update |
| PUT | /resources/vfx/{id}/toggle | 启停 | resource:manage |
| GET | /resources/stats | 加载失败率/崩溃率/帧率统计 | resource:view |

---

### 3.16 系统配置 — `/admin/api/v1/config`

| 方法 | 路径 | 说明 | 权限 |
|------|------|------|------|
| GET | /configs | 配置列表(按group筛选) | config:view |
| PUT | /configs/{key} | 修改配置值 | config:edit |
| POST | /configs/{key}/reset | 重置为默认值 | config:manage |
| GET | /announcements | 公告管理 | announcement:* |
| POST | /announcements | 创建公告 | announcement:create |

---

## 四、接口统计汇总

| 模块 | 玩家端接口数 | 后台接口数 | 总计 |
|------|------------|-----------|------|
| 认证 | 4 | 3 | 7 |
| 用户 | 6 | 8(+导出) | ~15 |
| 大厅 | 9 | — | 9 |
| 房间 | 7 | 7 | 14 |
| 钱包/保险箱 | 9 | 4 | 13 |
| 活动 | 3 | 7 | 10 |
| 客服(玩家) | 5 | 7 | 12 |
| 文件上传 | 1 | — | 1 |
| 驾驶舱 | — | 1 | 1 |
| 游戏管理 | — | 7 | 7 |
| 规则管理 | — | 9 | 9 |
| 房间管理(后台)| — | 7 | 7 |
| 风控 | — | 8 | 8 |
| 回放审计 | — | 4 | 4 |
| 推广渠道 | — | 5 | 5 |
| 权限角色 | — | 7 | 7 |
| 审计日志 | — | 3 | 3 |
| 版本管理 | 3 | 13 | 16 |
| 资源配置 | 2 | 12 | 14 |
| 系统配置 | — | 5 | 5 |
| **合计** | **~49** | **~117** | **~166** |

> 注：实际开发中每个 CRUD 可能还有 batch-delete、sort-change、toggle-status 等衍生接口，最终约 **180-200 个**。
