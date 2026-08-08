#!/usr/bin/env bash
# Deploy the C++ game server on an Alibaba Cloud ECS instance.
# Run this script on the ECS from a checked-out copy of this repository.

set -Eeuo pipefail
IFS=$'\n\t'

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
RUNTIME_DIR="${RUNTIME_DIR:-/opt/niuma-game-server}"
CONFIG_SOURCE="${CONFIG_SOURCE:-${PROJECT_DIR}/Server/server.ini}"
IMAGE_NAME="${IMAGE_NAME:-niuma-game-server:latest}"
CONTAINER_NAME="${CONTAINER_NAME:-niuma-game-server}"
DEFAULT_PUBLIC_HOST="8.156.81.51"
PUBLIC_HOST="${PUBLIC_HOST:-}"
TCP_PORT="${TCP_PORT:-10086}"
WS_PORT="${WS_PORT:-9098}"
BUILD_JOBS="${BUILD_JOBS:-2}"
DB_HOST="${DB_HOST:-127.0.0.1}"
DB_PORT="${DB_PORT:-3306}"
DB_USER="${DB_USER:-root}"
DB_PASSWORD="${DB_PASSWORD:-}"
DB_NAME="${DB_NAME:-niuma}"
SQL_FILES=()
APPLY_NIUMA_SQL=false
APPLY_NIUMA_BOOTSTRAP_SQL=false
RESET_PLAYER_DATA=false
SYNC_CONFIG=false
SKIP_FIREWALL=false
UPDATE_ENDPOINTS=false

log() { printf '[%(%F %T)T] %s\n' -1 "$*"; }
die() { printf '错误: %s\n' "$*" >&2; exit 1; }

usage() {
    cat <<'EOF'
用法: sudo ./deploy/aliyun-deploy.sh <命令> [选项]

命令:
  install       安装 Docker、保存运行配置、构建并启动服务
  update        重新构建并滚动更新容器（默认保留已部署配置）
  restart       重启容器
  stop          停止容器
  status        显示容器状态和最近日志
  logs          持续查看容器日志
  backup        备份运行配置和日志（不包含 MySQL 数据）

选项:
  --config PATH        server.ini 的私有位置；install 时必填或使用默认文件
  --public-host HOST   对客户端公布的域名/IP（install 默认 8.156.81.51）
  --runtime-dir PATH   运行配置、日志和备份目录（默认 /opt/niuma-game-server）
  --tcp-port PORT      TCP 游戏端口（默认 10086）
  --ws-port PORT       WebSocket 端口（默认 9098）
  --build-jobs N       Docker 构建并行数（默认 2，适合小规格 ECS）
  --sync-config        update 时以 --config 覆盖运行中的 server.ini
  --skip-firewall      不修改 ECS 本机防火墙
  --apply-sql PATH     install/update 前执行 SQL 文件，可重复传入
  --apply-niuma-sql    install/update 前执行项目内置 web_server SQL 迁移（v2_add、v3-v11）
  --apply-niuma-bootstrap-sql
                      install/update 前先执行 niuma.sql 与 v2_upgrade_step1.sql（包含 DROP TABLE，仅新库/重置库使用）
  --reset-player-data  install/update 前执行 v17 业务数据重置脚本（清除旧玩家/房间/流水，仅保留平台根与超级管理员）
  --db-host HOST       MySQL 地址（默认 127.0.0.1，也可用 DB_HOST）
  --db-port PORT       MySQL 端口（默认 3306，也可用 DB_PORT）
  --db-user USER       MySQL 用户（默认 root，也可用 DB_USER）
  --db-password PASS   MySQL 密码（也可用 DB_PASSWORD）
  --db-name NAME       MySQL 数据库（默认 niuma，也可用 DB_NAME）

示例:
  sudo ./deploy/aliyun-deploy.sh install --config /root/niuma-server.ini
  sudo ./deploy/aliyun-deploy.sh update
  sudo DB_PASSWORD='your-pass' ./deploy/aliyun-deploy.sh update --apply-niuma-sql
  sudo DB_PASSWORD='your-pass' ./deploy/aliyun-deploy.sh install --apply-niuma-bootstrap-sql
  sudo DB_PASSWORD='your-pass' ./deploy/aliyun-deploy.sh update --apply-niuma-sql --reset-player-data
EOF
}

require_root() {
    [[ "${EUID}" -eq 0 ]] || die "请使用 sudo 运行此命令"
}

require_number() {
    [[ "$2" =~ ^[1-9][0-9]*$ ]] || die "$1 必须是正整数"
}

ensure_docker() {
    if command -v docker >/dev/null 2>&1; then
        systemctl enable --now docker >/dev/null 2>&1 || true
        return
    fi

    log "安装 Docker…"
    if command -v apt-get >/dev/null 2>&1; then
        apt-get update
        apt-get install -y ca-certificates curl docker.io
    elif command -v dnf >/dev/null 2>&1; then
        dnf install -y docker
    else
        die "仅支持 Ubuntu/Debian 或 Alibaba Cloud Linux/RHEL 系统"
    fi
    systemctl enable --now docker
}

open_local_firewall() {
    ${SKIP_FIREWALL} && return

    if command -v firewall-cmd >/dev/null 2>&1 && firewall-cmd --state >/dev/null 2>&1; then
        firewall-cmd --permanent --add-port="${TCP_PORT}/tcp"
        firewall-cmd --permanent --add-port="${WS_PORT}/tcp"
        firewall-cmd --reload
    elif command -v ufw >/dev/null 2>&1 && ufw status | grep -q 'Status: active'; then
        ufw allow "${TCP_PORT}/tcp"
        ufw allow "${WS_PORT}/tcp"
    else
        log "未检测到启用的本机防火墙；仍需在阿里云安全组放行 ${TCP_PORT}/tcp 和 ${WS_PORT}/tcp。"
    fi
}

set_ini_value() {
    local file="$1" section="$2" key="$3" value="$4" temporary
    temporary="${file}.tmp.$$"
    if ! awk -v wanted_section="$section" -v wanted_key="$key" -v wanted_value="$value" '
        $0 == "[" wanted_section "]" { in_section = 1 }
        /^\[/ && $0 != "[" wanted_section "]" { in_section = 0 }
        in_section && $0 ~ "^[[:space:]]*" wanted_key "[[:space:]]*=" {
            print wanted_key "=" wanted_value
            changed = 1
            next
        }
        { print }
        END { if (!changed) exit 2 }
    ' "$file" > "$temporary"; then
        rm -f "$temporary"
        die "配置文件缺少 [${section}] 中的 ${key}"
    fi
    chmod 600 "$temporary"
    mv "$temporary" "$file"
}

check_config() {
    [[ -f "$CONFIG_SOURCE" ]] || die "找不到配置文件：${CONFIG_SOURCE}"
    if grep -Eq '=[[:space:]]*(CHANGE_ME|REPLACE_ME)[[:space:]]*$' "$CONFIG_SOURCE"; then
        die "配置文件仍有 CHANGE_ME/REPLACE_ME，请补全后再部署"
    fi
}

run_sql_migrations() {
    [[ "${#SQL_FILES[@]}" -eq 0 ]] && return
    command -v mysql >/dev/null 2>&1 || die "需要 mysql 客户端才能执行 --apply-sql"
    require_number "--db-port" "$DB_PORT"
    [[ -n "$DB_USER" ]] || die "--db-user 不能为空"
    [[ -n "$DB_NAME" ]] || die "--db-name 不能为空"

    local mysql_args
    mysql_args=(--host="$DB_HOST" --port="$DB_PORT" --user="$DB_USER" "$DB_NAME")
    if [[ -n "$DB_PASSWORD" ]]; then
        export MYSQL_PWD="$DB_PASSWORD"
    fi
    for sql_file in "${SQL_FILES[@]}"; do
        [[ -f "$sql_file" ]] || die "SQL 文件不存在：${sql_file}"
        log "执行 SQL 迁移：${sql_file}"
        mysql "${mysql_args[@]}" < "$sql_file"
    done
    if [[ -n "$DB_PASSWORD" ]]; then
        unset MYSQL_PWD
    fi
}

append_builtin_sql_migrations() {
    local web_sql_dir="${PROJECT_DIR}/../web_server/web-server/sql"
    if ${APPLY_NIUMA_BOOTSTRAP_SQL}; then
        SQL_FILES+=(
            "${web_sql_dir}/niuma.sql"
            "${web_sql_dir}/v2_upgrade_step1.sql"
        )
    fi
    if ${APPLY_NIUMA_SQL}; then
        SQL_FILES+=(
            "${web_sql_dir}/v2_add_regional_games.sql"
            "${web_sql_dir}/v3_add_rule_config.sql"
            "${web_sql_dir}/v4_add_record_tables.sql"
            "${web_sql_dir}/v5_add_doudizhu.sql"
            "${web_sql_dir}/v6_add_game_record_tables.sql"
            "${web_sql_dir}/v7_game_record_retention.sql"
            "${web_sql_dir}/v7_fix_doudizhu_two_player_rule.sql"
            "${web_sql_dir}/v8_fix_doudizhu_standard_hand_count.sql"
            "${web_sql_dir}/v9_fix_paodekuai_turn_timeout.sql"
            "${web_sql_dir}/v10_agency_commission.sql"
            "${web_sql_dir}/v11_fixed_score_and_room_options.sql"
            "${web_sql_dir}/v13_add_system_log_tables.sql"
            "${web_sql_dir}/v14_agent_workbench_and_menu_cleanup.sql"
            "${web_sql_dir}/v15_fix_game_management_menu_encoding.sql"
            "${web_sql_dir}/v16_permanent_agency_invite_codes.sql"
        )
    fi
    ${RESET_PLAYER_DATA} && SQL_FILES+=("${web_sql_dir}/v17_player_id_invite_binding_reset.sql")
    ${APPLY_NIUMA_SQL} && SQL_FILES+=("${web_sql_dir}/v18_restore_register_invite_codes.sql")
}

prepare_runtime_config() {
    local deployed_config="${RUNTIME_DIR}/config/server.ini"
    mkdir -p "${RUNTIME_DIR}/config" "${RUNTIME_DIR}/log" "${RUNTIME_DIR}/backup"

    if [[ ! -f "$deployed_config" || "$SYNC_CONFIG" == true ]]; then
        check_config
        install -m 600 "$CONFIG_SOURCE" "$deployed_config"
    fi

    # update does not silently replace the address kept in a working production config.
    if [[ "$UPDATE_ENDPOINTS" == true ]]; then
        PUBLIC_HOST="${PUBLIC_HOST:-$DEFAULT_PUBLIC_HOST}"
        set_ini_value "$deployed_config" "Server" "port" "$TCP_PORT"
        set_ini_value "$deployed_config" "Server" "access_address" "${PUBLIC_HOST}:${TCP_PORT}"
        set_ini_value "$deployed_config" "Server" "ws_address" "ws://${PUBLIC_HOST}:${WS_PORT}/"
        set_ini_value "$deployed_config" "Websocket" "port" "$WS_PORT"
    fi
}

build_image() {
    [[ -f "${PROJECT_DIR}/Dockerfile" ]] || die "Dockerfile 不存在：${PROJECT_DIR}/Dockerfile"
    log "构建镜像（首次构建会下载编译依赖，耗时较长）…"
    docker build --pull --build-arg "BUILD_JOBS=${BUILD_JOBS}" --tag "$IMAGE_NAME" "$PROJECT_DIR"
}

start_container() {
    local config_file="${RUNTIME_DIR}/config/server.ini"
    docker rm -f "$CONTAINER_NAME" >/dev/null 2>&1 || true

    # Host network keeps 127.0.0.1 in server.ini pointing to MySQL on this ECS.
    docker run -d \
        --name "$CONTAINER_NAME" \
        --network host \
        --restart unless-stopped \
        --mount "type=bind,src=${config_file},dst=/app/server.ini,readonly" \
        --mount "type=bind,src=${RUNTIME_DIR}/log,dst=/app/log" \
        --log-driver json-file \
        --log-opt max-size=100m \
        --log-opt max-file=3 \
        "$IMAGE_NAME" >/dev/null

    sleep 5
    docker inspect --format '{{.State.Running}}' "$CONTAINER_NAME" | grep -qx true || {
        docker logs --tail 100 "$CONTAINER_NAME" >&2 || true
        die "容器未能启动；上方为最近 100 行日志"
    }
}

install_or_update() {
    require_number "--tcp-port" "$TCP_PORT"
    require_number "--ws-port" "$WS_PORT"
    require_number "--build-jobs" "$BUILD_JOBS"
    ensure_docker
    open_local_firewall
    prepare_runtime_config
    append_builtin_sql_migrations
    run_sql_migrations
    build_image
    start_container
    log "部署完成；请使用 status 或 logs 确认服务连接到了 MySQL、Redis 和 RabbitMQ。"
}

show_status() {
    docker ps --filter "name=^/${CONTAINER_NAME}$" --format 'table {{.Names}}\t{{.Status}}\t{{.Image}}'
    docker logs --tail 50 "$CONTAINER_NAME" 2>&1 || true
}

backup() {
    local timestamp archive
    [[ -d "$RUNTIME_DIR" ]] || die "运行目录不存在：${RUNTIME_DIR}"
    timestamp="$(date +%Y%m%d_%H%M%S)"
    archive="${RUNTIME_DIR}/backup/game-server_${timestamp}.tar.gz"
    tar -C "$RUNTIME_DIR" -czf "$archive" config log
    find "${RUNTIME_DIR}/backup" -type f -name 'game-server_*.tar.gz' -mtime +14 -delete
    log "备份已创建：${archive}（不包含 MySQL、Redis 或 RabbitMQ 数据）"
}

ACTION="${1:-install}"
if [[ $# -gt 0 ]]; then
    shift
fi

case "$ACTION" in
    help|-h|--help)
        usage
        exit 0
        ;;
esac

while [[ $# -gt 0 ]]; do
    case "$1" in
        --config|--public-host|--runtime-dir|--tcp-port|--ws-port|--build-jobs|--apply-sql|--db-host|--db-port|--db-user|--db-password|--db-name)
            [[ $# -ge 2 ]] || die "选项 $1 缺少参数"
            case "$1" in
                --config) CONFIG_SOURCE="$2" ;;
                --public-host) PUBLIC_HOST="$2"; UPDATE_ENDPOINTS=true ;;
                --runtime-dir) RUNTIME_DIR="$2" ;;
                --tcp-port) TCP_PORT="$2"; UPDATE_ENDPOINTS=true ;;
                --ws-port) WS_PORT="$2"; UPDATE_ENDPOINTS=true ;;
                --build-jobs) BUILD_JOBS="$2" ;;
                --apply-sql) SQL_FILES+=("$2") ;;
                --db-host) DB_HOST="$2" ;;
                --db-port) DB_PORT="$2" ;;
                --db-user) DB_USER="$2" ;;
                --db-password) DB_PASSWORD="$2" ;;
                --db-name) DB_NAME="$2" ;;
            esac
            shift 2
            ;;
        --sync-config) SYNC_CONFIG=true; shift ;;
        --apply-niuma-sql) APPLY_NIUMA_SQL=true; shift ;;
        --apply-niuma-bootstrap-sql) APPLY_NIUMA_BOOTSTRAP_SQL=true; APPLY_NIUMA_SQL=true; shift ;;
        --reset-player-data) RESET_PLAYER_DATA=true; APPLY_NIUMA_SQL=true; shift ;;
        --skip-firewall) SKIP_FIREWALL=true; shift ;;
        -h|--help|help) usage; exit 0 ;;
        *) die "未知选项：$1" ;;
    esac
done

if [[ "$ACTION" == "install" ]]; then
    SYNC_CONFIG=true
    UPDATE_ENDPOINTS=true
fi

require_root
case "$ACTION" in
    install|update) install_or_update ;;
    restart) docker restart "$CONTAINER_NAME" ;;
    stop) docker stop "$CONTAINER_NAME" ;;
    status) show_status ;;
    logs) docker logs -f "$CONTAINER_NAME" ;;
    backup) backup ;;
    *) usage; exit 1 ;;
esac
