// VersionManager.h

#ifndef _NIU_MA_VERSION_MANAGER_H_
#define _NIU_MA_VERSION_MANAGER_H_

#include "Base/Singleton.h"

#include <string>
#include <mutex>
#include <unordered_map>
#include <atomic>

namespace NiuMa
{
	/**
	 * 游戏引擎版本管理器
	 *
	 * 功能:
	 *   1. 接收 web_server 通过 MQ 下发的游戏引擎版本号
	 *   2. 客户端连接时返回当前游戏引擎版本
	 *   3. 支持灰度发布：按玩家ID或百分比分配新旧版本
	 *
	 * 版本配置通过 MQ 消息 MsgGameVersionUpdate 更新，
	 * 也可从 Redis 恢复（重启后持久化）。
	 *
	 * 灰度规则:
	 *   - 按玩家ID列表：指定玩家使用新版本
	 *   - 按百分比：根据玩家ID hash 值的取模分配
	 */
	class VersionManager : public Singleton<VersionManager> {
	private:
		VersionManager();

	public:
		virtual ~VersionManager();
		friend class Singleton<VersionManager>;

	public:
		/**
		 * 初始化版本管理器
		 * 从 Redis 加载已保存的版本配置，并注册 MQ 消息处理器
		 * @param directExchange RabbitMQ定向交换机名称
		 * @param directConsumerTag RabbitMQ定向消费者标签
		 */
		void init(const std::string& directExchange, const std::string& directConsumerTag);

		/**
		 * 获取当前游戏引擎版本号
		 * @return 版本号字符串，例如 "1.0.0"
		 */
		std::string getCurrentVersion() const;

		/**
		 * 获取当前最低兼容版本号
		 * 低于此版本的客户端需要强制更新
		 * @return 最低兼容版本号字符串
		 */
		std::string getMinCompatibleVersion() const;

		/**
		 * 获取指定玩家的游戏引擎版本
		 * 根据灰度发布规则，某些玩家可能使用不同的版本
		 * @param playerId 玩家ID
		 * @return 该玩家应使用的版本号
		 */
		std::string getPlayerVersion(const std::string& playerId) const;

		/**
		 * 判断指定玩家的版本是否兼容（是否需要强制更新）
		 * @param clientVersion 客户端当前版本
		 * @param playerId 玩家ID
		 * @return true-兼容无需更新，false-需要强制更新
		 */
		bool isVersionCompatible(const std::string& clientVersion, const std::string& playerId) const;

	private:
		/**
		 * 处理MQ版本更新消息
		 * @param msgType 消息类型
		 * @param json 消息体JSON
		 */
		void handleMessage(const std::string& msgType, const std::string& json);

		/**
		 * 更新版本配置
		 * @param version 当前版本
		 * @param minVersion 最低兼容版本
		 * @param grayPercent 灰度百分比(0-100)
		 * @param grayPlayerIds 灰度玩家ID列表(逗号分隔)
		 */
		void updateVersion(const std::string& version, const std::string& minVersion,
			int grayPercent, const std::string& grayPlayerIds);

		/**
		 * 从 Redis 加载版本配置
		 */
		void loadFromRedis();

		/**
		 * 保存版本配置到 Redis
		 */
		void saveToRedis();

		/**
		 * 对玩家ID做 hash 取模判断是否命中灰度百分比
		 * @param playerId 玩家ID
		 * @return true-命中灰度
		 */
		bool hitGrayPercent(const std::string& playerId) const;

	private:
		// 当前游戏引擎版本
		std::string _currentVersion;

		// 最低兼容版本（低于此版本需要强制更新）
		std::string _minCompatibleVersion;

		// 灰度发布百分比(0-100)，0表示不启用灰度
		int _grayPercent;

		// 灰度发布的版本号（命中灰度的玩家使用此版本）
		std::string _grayVersion;

		// 灰度玩家ID集合（这些玩家强制使用灰度版本）
		std::unordered_map<std::string, bool> _grayPlayerIds;

		// 信号量
		mutable std::mutex _mtx;

		// 初始化标志
		std::atomic_bool _initFlag;

		// RabbitMQ定向交换机名称
		std::string _directExchange;

		// RabbitMQ定向消费者标签
		std::string _directConsumerTag;
	};
}

#endif // !_NIU_MA_VERSION_MANAGER_H_
