// VersionManager.cpp

#include "VersionManager.h"
#include "Base/Log.h"
#include "Base/BaseUtils.h"
#include "Constant/RedisKeys.h"
#include "Redis/RedisPool.h"
#include "Rabbitmq/RabbitmqClient.h"
#include "Rabbitmq/RabbitmqConsumer.h"
#include "Rabbitmq/RabbitmqMessageJsonHandler.h"

#include <sstream>
#include <functional>
#include <json/json.h>

namespace NiuMa
{
	template<> VersionManager* Singleton<VersionManager>::_inst = nullptr;

	// Redis key for version config persistence
	static const std::string REDIS_KEY_VERSION_CONFIG = "game_engine_version_config";
	static const std::string REDIS_KEY_VERSION_FIELD = "current_version";
	static const std::string REDIS_KEY_MIN_VERSION_FIELD = "min_compatible_version";
	static const std::string REDIS_KEY_GRAY_PERCENT_FIELD = "gray_percent";
	static const std::string REDIS_KEY_GRAY_VERSION_FIELD = "gray_version";
	static const std::string REDIS_KEY_GRAY_PLAYERS_FIELD = "gray_player_ids";

	VersionManager::VersionManager()
		: _grayPercent(0)
		, _initFlag(false)
	{}

	VersionManager::~VersionManager() {}

	void VersionManager::init(const std::string& directExchange, const std::string& directConsumerTag) {
		if (_initFlag)
			return;
		_initFlag = true;
		_directExchange = directExchange;
		_directConsumerTag = directConsumerTag;

		// 从 Redis 加载版本配置
		loadFromRedis();

		// 默认版本号
		if (_currentVersion.empty())
			_currentVersion = "1.0.0";
		if (_minCompatibleVersion.empty())
			_minCompatibleVersion = "1.0.0";
		if (_grayVersion.empty())
			_grayVersion = _currentVersion;

		// 注册 MQ 消息处理器，监听 web_server 的版本更新消息
		class VersionUpdateHandler : public RabbitmqMessageJsonHandler {
		public:
			VersionUpdateHandler(const std::string& tag)
				: RabbitmqMessageJsonHandler(tag)
			{}

			virtual ~VersionUpdateHandler() {}

		protected:
			virtual bool receive(const std::string& message) override {
				// 只处理版本更新相关消息
				return (message.find("MsgGameVersionUpdate") != std::string::npos);
			}

			virtual void handleImpl(const std::string& msgType, const std::string& json) override {
				VersionManager::getSingleton().handleMessage(msgType, json);
			}
		};
		RabbitmqMessageHandler::Ptr handler(new VersionUpdateHandler(directConsumerTag));
		RabbitmqConsumer::getSingleton().addHandler(handler);

		{
			std::ostringstream oss;
			oss << "版本管理器初始化完成，当前版本: " << _currentVersion << "，最低兼容版本: " << _minCompatibleVersion;
			LOG_INFO(oss.str());
		}
	}

	std::string VersionManager::getCurrentVersion() const {
		std::lock_guard<std::mutex> lck(_mtx);
		return _currentVersion;
	}

	std::string VersionManager::getMinCompatibleVersion() const {
		std::lock_guard<std::mutex> lck(_mtx);
		return _minCompatibleVersion;
	}

	std::string VersionManager::getPlayerVersion(const std::string& playerId) const {
		std::lock_guard<std::mutex> lck(_mtx);

		// 1. 检查是否在灰度玩家列表中
		auto it = _grayPlayerIds.find(playerId);
		if (it != _grayPlayerIds.end())
			return _grayVersion;

		// 2. 检查灰度百分比
		if (_grayPercent > 0 && hitGrayPercent(playerId))
			return _grayVersion;

		// 3. 默认使用当前版本
		return _currentVersion;
	}

	bool VersionManager::isVersionCompatible(const std::string& clientVersion, const std::string& playerId) const {
		// 获取该玩家应使用的最低兼容版本
		std::string minVersion = getMinCompatibleVersion();
		if (minVersion.empty())
			return true;
		if (clientVersion.empty())
			return false;

		// 简单的语义版本比较 (major.minor.patch)
		// 将版本号拆分为整数数组比较
		std::vector<int> clientParts, minParts;
		std::istringstream cs(clientVersion), ms(minVersion);
		std::string token;
		while (std::getline(cs, token, '.')) {
			try { clientParts.push_back(std::stoi(token)); }
			catch (...) { clientParts.push_back(0); }
		}
		while (std::getline(ms, token, '.')) {
			try { minParts.push_back(std::stoi(token)); }
			catch (...) { minParts.push_back(0); }
		}

		// 补齐到相同长度
		while (clientParts.size() < minParts.size()) clientParts.push_back(0);
		while (minParts.size() < clientParts.size()) minParts.push_back(0);

		// 逐段比较
		for (size_t i = 0; i < clientParts.size() && i < minParts.size(); i++) {
			if (clientParts[i] < minParts[i])
				return false;
			if (clientParts[i] > minParts[i])
				return true;
		}
		return true;
	}

	void VersionManager::handleMessage(const std::string& msgType, const std::string& json) {
		if (msgType != "MsgGameVersionUpdate")
			return;

		Json::Value obj;
		Json::CharReaderBuilder builder;
		Json::CharReader* reader = builder.newCharReader();
		std::string errors;
		try {
			reader->parse(json.c_str(), json.c_str() + json.size(), &obj, &errors);
		}
		catch (...) {
			delete reader;
			LOG_ERROR("版本更新消息JSON解析失败");
			return;
		}
		delete reader;

		std::string version, minVersion, grayVersion, grayPlayerIds;
		int grayPercent = 0;

		if (obj.isMember("current_version") && obj["current_version"].isString())
			version = obj["current_version"].asString();
		if (obj.isMember("min_compatible_version") && obj["min_compatible_version"].isString())
			minVersion = obj["min_compatible_version"].asString();
		if (obj.isMember("gray_percent") && obj["gray_percent"].isInt())
			grayPercent = obj["gray_percent"].asInt();
		if (obj.isMember("gray_version") && obj["gray_version"].isString())
			grayVersion = obj["gray_version"].asString();
		if (obj.isMember("gray_player_ids") && obj["gray_player_ids"].isString())
			grayPlayerIds = obj["gray_player_ids"].asString();

		updateVersion(version, minVersion, grayPercent, grayPlayerIds);

		// 如果消息中指定了灰度版本，使用它；否则灰度版本等于当前版本
		if (!grayVersion.empty()) {
			std::lock_guard<std::mutex> lck(_mtx);
			_grayVersion = grayVersion;
		}

		saveToRedis();
		{
			std::ostringstream oss;
			oss << "收到版本更新通知，当前版本: " << version << "，最低兼容版本: " << minVersion
				<< "，灰度百分比: " << grayPercent << "%";
			LOG_INFO(oss.str());
		}
	}

	void VersionManager::updateVersion(const std::string& version, const std::string& minVersion,
		int grayPercent, const std::string& grayPlayerIds)
	{
		std::lock_guard<std::mutex> lck(_mtx);
		if (!version.empty())
			_currentVersion = version;
		if (!minVersion.empty())
			_minCompatibleVersion = minVersion;
		_grayPercent = grayPercent;

		// 解析灰度玩家ID列表（逗号分隔）
		_grayPlayerIds.clear();
		if (!grayPlayerIds.empty()) {
			std::istringstream ss(grayPlayerIds);
			std::string pid;
			while (std::getline(ss, pid, ',')) {
				// 去除首尾空白
				size_t start = pid.find_first_not_of(" \t\r\n");
				size_t end = pid.find_last_not_of(" \t\r\n");
				if (start != std::string::npos && end != std::string::npos) {
					pid = pid.substr(start, end - start + 1);
					if (!pid.empty())
						_grayPlayerIds[pid] = true;
				}
			}
		}

		// 灰度版本默认等于当前版本
		if (_grayVersion.empty())
			_grayVersion = _currentVersion;
	}

	void VersionManager::loadFromRedis() {
		std::string value;

		if (RedisPool::getSingleton().hget(REDIS_KEY_VERSION_CONFIG, REDIS_KEY_VERSION_FIELD, value) && !value.empty()) {
			_currentVersion = value;
		}
		if (RedisPool::getSingleton().hget(REDIS_KEY_VERSION_CONFIG, REDIS_KEY_MIN_VERSION_FIELD, value) && !value.empty()) {
			_minCompatibleVersion = value;
		}
		int64_t intVal = 0;
		if (RedisPool::getSingleton().hget(REDIS_KEY_VERSION_CONFIG, REDIS_KEY_GRAY_PERCENT_FIELD, intVal)) {
			_grayPercent = static_cast<int>(intVal);
		}
		if (RedisPool::getSingleton().hget(REDIS_KEY_VERSION_CONFIG, REDIS_KEY_GRAY_VERSION_FIELD, value) && !value.empty()) {
			_grayVersion = value;
		}
		if (RedisPool::getSingleton().hget(REDIS_KEY_VERSION_CONFIG, REDIS_KEY_GRAY_PLAYERS_FIELD, value) && !value.empty()) {
			// 解析逗号分隔的玩家ID
			std::istringstream ss(value);
			std::string pid;
			while (std::getline(ss, pid, ',')) {
				size_t start = pid.find_first_not_of(" \t\r\n");
				size_t end = pid.find_last_not_of(" \t\r\n");
				if (start != std::string::npos && end != std::string::npos) {
					pid = pid.substr(start, end - start + 1);
					if (!pid.empty())
						_grayPlayerIds[pid] = true;
				}
			}
		}
	}

	void VersionManager::saveToRedis() {
		std::lock_guard<std::mutex> lck(_mtx);
		RedisPool::getSingleton().hset(REDIS_KEY_VERSION_CONFIG, REDIS_KEY_VERSION_FIELD, _currentVersion);
		RedisPool::getSingleton().hset(REDIS_KEY_VERSION_CONFIG, REDIS_KEY_MIN_VERSION_FIELD, _minCompatibleVersion);
		RedisPool::getSingleton().hset(REDIS_KEY_VERSION_CONFIG, REDIS_KEY_GRAY_PERCENT_FIELD, static_cast<int64_t>(_grayPercent));

		if (!_grayVersion.empty())
			RedisPool::getSingleton().hset(REDIS_KEY_VERSION_CONFIG, REDIS_KEY_GRAY_VERSION_FIELD, _grayVersion);

		// 序列化灰度玩家列表
		if (!_grayPlayerIds.empty()) {
			std::string ids;
			for (const auto& kv : _grayPlayerIds) {
				if (!ids.empty()) ids += ",";
				ids += kv.first;
			}
			RedisPool::getSingleton().hset(REDIS_KEY_VERSION_CONFIG, REDIS_KEY_GRAY_PLAYERS_FIELD, ids);
		}
	}

	bool VersionManager::hitGrayPercent(const std::string& playerId) const {
		// 使用 std::hash 对玩家ID做 hash，然后取模
		if (_grayPercent <= 0)
			return false;
		if (_grayPercent >= 100)
			return true;
		std::size_t h = std::hash<std::string>{}(playerId);
		return (h % 100 < static_cast<std::size_t>(_grayPercent));
	}
}
