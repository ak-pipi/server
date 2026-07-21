# 阿里云 ECS 部署

这个服务不是 HTTP 网站，而是 C++ 游戏逻辑服。它通过 TCP `10086` 和 WebSocket `9098` 接受客户端连接，并依赖 MySQL、Redis、RabbitMQ。

当前 `Server/server.ini` 指向本机 MySQL（`127.0.0.1:3306`），因此应部署到安装了该 MySQL 数据库的 ECS。现有 Redis/RabbitMQ 配置使用 `8.156.81.51`；若该地址就是承载 MySQL 的 ECS，可按以下命令部署到该机。

## 一次部署

在本机同步代码时，不传输正式配置到代码目录：

```bash
ssh root@8.156.81.51 'mkdir -p /opt/niuma-game-server/source /root/niuma-private'
rsync -az --exclude '.git' --exclude 'build' --exclude 'bin' --exclude 'Server/server.ini' \
  ./ root@8.156.81.51:/opt/niuma-game-server/source/
scp Server/server.ini root@8.156.81.51:/root/niuma-private/server.ini
ssh root@8.156.81.51 \
  'chmod 600 /root/niuma-private/server.ini && cd /opt/niuma-game-server/source && \
   ./deploy/aliyun-deploy.sh install --config /root/niuma-private/server.ini --public-host 8.156.81.51'
```

首次构建会在 ECS 内下载并编译 RabbitMQ C 客户端、MySQL Connector/C++ 和游戏服务，因此耗时较长。建议使用至少 2 核 4 GB 内存、40 GB 磁盘的 Linux ECS，并在阿里云安全组只向玩家网络开放 TCP `10086` 与 `9098`。

不要向公网开放 `3306`、`6379` 或 `5672`。

## 后续更新

同步代码后运行：

```bash
cd /opt/niuma-game-server/source
sudo ./deploy/aliyun-deploy.sh update
```

配置变更时显式同步，避免更新代码时意外覆盖线上配置：

```bash
sudo ./deploy/aliyun-deploy.sh update \
  --config /root/niuma-private/server.ini --sync-config
```

可用 `status`、`logs`、`restart`、`stop`、`backup` 替代 `update` 查看或维护服务。`backup` 仅备份游戏服配置和日志；MySQL、Redis、RabbitMQ 必须按各自的备份策略处理。
