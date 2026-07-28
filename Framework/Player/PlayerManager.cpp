// PlayerManager.cpp

#include "Base/BaseUtils.h"
#include "Base/Log.h"
#include "Constant/RedisKeys.h"
#include "Redis/RedisPool.h"
#include "Timer/TimerManager.h"
#include "MySql/MysqlPool.h"
#include "Rabbitmq/RabbitmqConsumer.h"
#include "Rabbitmq/RabbitmqMessageJsonHandler.h"
#include "Venue/VenueManager.h"
#include "Venue/VenueInnerHandler.h"
#include "PlayerManager.h"
#include "LoadPlayerTask.h"
#include "PlayerMessages.h"

#include <mysql/jdbc.h>
#include <boost/locale.hpp>
#include "jsoncpp/include/json/json.h"

#include <chrono>
#include <sstream>

namespace NiuMa {
	template<> PlayerManager* Singleton<PlayerManager>::_inst = nullptr;

	namespace {
		std::string getJsonString(const Json::Value& root, const char* key) {
			const Json::Value& val = root[key];
			if (val.isString())
				return val.asString();
			if (val.isInt64() || val.isInt())
				return std::to_string(val.asInt64());
			if (val.isUInt64() || val.isUInt())
				return std::to_string(val.asUInt64());
			return std::string();
		}

		int64_t getJsonInt64(const Json::Value& root, const char* key, int64_t defaultValue = 0) {
			const Json::Value& val = root[key];
			if (val.isInt64() || val.isUInt64())
				return val.asInt64();
			if (val.isInt())
				return static_cast<int64_t>(val.asInt());
			if (val.isString()) {
				try {
					return std::stoll(val.asString());
				} catch (std::exception&) {
					return defaultValue;
				}
			}
			return defaultValue;
		}
	}

	PlayerManager::PlayerManager() {
		_inexactTime = BaseUtils::getCurrentSecond();
	}

	PlayerManager::~PlayerManager() {}

	void PlayerManager::init(const std::string& directConsumerTag) {
		// 添加定时任务
		_timer = std::make_shared<int>();
		std::weak_ptr<int> weak(_timer);
		TimerManager::getSingleton().addAsyncTimer(5000, [weak] {
			std::shared_ptr<int> strong = weak.lock();
			if (!strong)
				return true;
			return PlayerManager::getSingleton().onTimer();
			});

		if (!directConsumerTag.empty()) {
			class WalletSyncHandler : public RabbitmqMessageJsonHandler {
			public:
				WalletSyncHandler(const std::string& tag)
					: RabbitmqMessageJsonHandler(tag)
				{}

				virtual ~WalletSyncHandler() {}

			protected:
				virtual bool receive(const std::string& message) override {
					return (message.find("MsgPlayerWalletSync") != std::string::npos);
				}

				virtual void handleImpl(const std::string& msgType, const std::string& json) override {
					if (msgType == MsgPlayerWalletSync::TYPE)
						PlayerManager::getSingleton().handleWalletSync(json);
				}
			};
			RabbitmqMessageHandler::Ptr handler(new WalletSyncHandler(directConsumerTag));
			RabbitmqConsumer::getSingleton().addHandler(handler);
		}
	}

	void PlayerManager::handleWalletSync(const std::string& json) {
		if (json.empty())
			return;
		Json::Value root;
		std::stringstream ss(json);
		ss >> root;

		std::string playerId = getJsonString(root, "playerId");
		if (playerId.empty())
			return;
		std::string walletType = getJsonString(root, "walletType");
		int64_t changeAmount = getJsonInt64(root, "changeAmount");
		int64_t balanceAfter = getJsonInt64(root, "balanceAfter");
		int64_t gold = getJsonInt64(root, "gold", -1);
		int64_t deposit = getJsonInt64(root, "deposit", -1);
		int64_t diamond = getJsonInt64(root, "diamond", -1);
		std::string bizType = getJsonString(root, "bizType");
		std::string bizId = getJsonString(root, "bizId");
		int64_t walletLedgerId = getJsonInt64(root, "walletLedgerId");

		std::string venueId;
		Player::Ptr player = getPlayer(playerId);
		if (player)
			player->getVenueId(venueId);
		if (venueId.empty()) {
			std::string redisKey = RedisKeys::PLAYER_CURRENT_VENUE + playerId;
			RedisPool::getSingleton().get(redisKey, venueId);
		}
		if (venueId.empty())
			return;

		Venue::Ptr venue = VenueManager::getSingleton().getVenue(venueId);
		if (!venue)
			return;
		std::shared_ptr<VenueInnerHandler> handler = venue->getHandler();
		if (!handler)
			return;
		ThreadWorker::Ptr worker = handler->getWorker();
		if (!worker)
			return;

		std::weak_ptr<Venue> weakVenue = venue;
		worker->dispatch([weakVenue, playerId, walletType, changeAmount, balanceAfter,
			gold, deposit, diamond, bizType, bizId, walletLedgerId]() {
				Venue::Ptr strong = weakVenue.lock();
				if (!strong || strong->isObsolete())
					return;
				strong->onWalletSync(playerId, walletType, changeAmount, balanceAfter,
					gold, deposit, diamond, bizType, bizId, walletLedgerId);
			});
	}

	Player::Ptr PlayerManager::getPlayer(const std::string& playerId) const {
		Player::Ptr ret;

		std::lock_guard<std::mutex> lck(_mtx);

		std::unordered_map<std::string, Player::Ptr>::const_iterator it = _players.find(playerId);
		if (it != _players.end()) {
			ret = it->second;
			// 更新引用时间
			ret->touch(_inexactTime);
		}
		return ret;
	}

	Player::Ptr PlayerManager::loadPlayer(const std::string& playerId) {
		Player::Ptr player = getPlayer(playerId);
		if (player)
			return player;
		player = std::make_shared<Player>(playerId);
		if (loadPlayerImpl(player)) {
			if (!addPlayer(player))
				player = getPlayer(playerId);
			return player;
		}
		return Player::Ptr();
	}

	bool PlayerManager::loadPlayerImpl(const Player::Ptr& player) const {
		if (!player)
			return false;
		std::shared_ptr<LoadPlayerTask> task = std::make_shared<LoadPlayerTask>(player->getId());
		MysqlPool::getSingleton().syncQuery(task);
		if (!task->getSucceed())
			return false;
		player->setName(task->_name);
		player->setNickname(task->_nickname);
		player->setPhone(task->_phone);
		player->setSex(task->_sex);
		player->setAvatar(task->_avatar);
		return true;
	}

	bool PlayerManager::loadRobot(int robotId, std::string& playerId) {
		class GetRobotPlayerIdTask : public MysqlQueryTask {
		public:
			GetRobotPlayerIdTask(int robotId)
				: _robotId(robotId)
			{}

			virtual ~GetRobotPlayerIdTask() {}

		public:
			virtual QueryType buildQuery(std::string& sql) override {
				sql = "select `player_id` from `robot` where `id` = " + std::to_string(_robotId);
				return QueryType::Select;
			}

			virtual int fetchResult(sql::ResultSet* res) override {
				int rows = 0;
				while (res->next()) {
					_playerId = res->getString("player_id");
					rows++;
				}
				return rows;
			}

		public:
			// 机器人id
			const int _robotId;

			// 玩家id
			std::string _playerId;
		};
		std::shared_ptr<GetRobotPlayerIdTask> task = std::make_shared<GetRobotPlayerIdTask>(robotId);
		MysqlPool::getSingleton().syncQuery(task);
		if (!task->getSucceed())
			return false;
		Player::Ptr player = loadPlayer(task->_playerId);
		if (player) {
			playerId = player->getId();
			return true;
		}
		return false;
	}

	int PlayerManager::getRobotCount() {
		std::string sql("select count(*) from `robot`");
		std::shared_ptr <MysqlCountTask> task = std::make_shared<MysqlCountTask>(sql);
		MysqlPool::getSingleton().syncQuery(task);
		if (task->getSucceed())
			return task->getCount();
		return 0;
	}

	bool PlayerManager::addPlayer(const Player::Ptr& player) {
		if (!player)
			return false;

		std::lock_guard<std::mutex> lck(_mtx);

		std::unordered_map<std::string, Player::Ptr>::iterator it = _players.find(player->getId());
		if (it != _players.end())
			return false;
		_players.insert(std::make_pair(player->getId(), player));
		return true;
	}

	bool PlayerManager::verifySignature(
		const std::string& playerId,
		const std::string& timestamp,
		const std::string& nonce,
		const std::string& signature,
		bool& outdate) {
		if (playerId.empty()) {
			ErrorS << "Verify signature failed: empty playerId";
			return false; // 玩家id非法
		}
		time_t time1 = BaseUtils::getCurrentSecond();
		time_t time2 = atoll(timestamp.c_str());
		time_t delta = abs(time1 - time2);
		if (delta > 60L) {
			// 传入时间戳与当前时间戳相差大于60秒
			outdate = true;
			ErrorS << "Verify signature failed: player(id:" << playerId << ") timestamp outdate, delta: " << delta;
			return false;
		}
		std::string secret;
		Player::Ptr player = loadPlayer(playerId);
		if (!player) {
			ErrorS << "Verify signature failed: load player(id:" << playerId << ") failed";
			return false;
		}
		if (!player->testNonce(nonce, time2)) {
			ErrorS << "Verify signature failed: player(id:" << playerId << ") nonce conflict";
			return false;	// 随机串冲突
		}
		player->getSecret(secret);
		bool test = false;
		std::string redisKey;
		if (secret.empty()) {
			// 从redis中获取
			redisKey = RedisKeys::PLAYER_MESSAGE_SECRET + playerId;
			if (!RedisPool::getSingleton().get(redisKey, secret)) {
				ErrorS << "Verify signature failed: player(id:" << playerId << ") redis secret missing";
				return false;	// 从Redis获取密钥失败
			}
			if (!secret.empty())
				player->setSecret(secret);
			test = true;
		}
		if (secret.empty()) {
			ErrorS << "Verify signature failed: player(id:" << playerId << ") secret empty";
			return false; // 未分配密钥
		}
		std::string text = playerId + '&' + timestamp + '&' + nonce + '&' + secret;
		std::string md5;
		BaseUtils::encodeMD5(text, md5);
		if (md5 != signature) {
			if (test) {
				ErrorS << "Verify signature failed: player(id:" << playerId << ") md5 mismatch";
				return false;
			}
			// 尝试从Redis中获取密钥
			std::string temp;
			redisKey = RedisKeys::PLAYER_MESSAGE_SECRET + playerId;
			if (!RedisPool::getSingleton().get(redisKey, temp)) {
				ErrorS << "Verify signature failed: player(id:" << playerId << ") redis secret refresh failed";
				return false;	// 从Redis获取密钥失败
			}
			if (temp == secret) {
				ErrorS << "Verify signature failed: player(id:" << playerId << ") md5 mismatch after redis refresh";
				return false;	// 密钥未发生变化
			}
			secret = temp;
			player->setSecret(secret);
			if (secret.empty())
				return false;
			text = playerId + '&' + timestamp + '&' + nonce + '&' + secret;
			BaseUtils::encodeMD5(text, md5);
			if (md5 != signature) {
				ErrorS << "Verify signature failed: player(id:" << playerId << ") md5 mismatch with refreshed secret";
				return false;
			}
		}
		return true;
	}

	void PlayerManager::setSessionPlayerId(const std::string& sessionId, const std::string& playerId) {
		std::lock_guard<std::mutex> lck(_mtx);

		std::unordered_map<std::string, std::string>::iterator it = _sessionMap.find(sessionId);
		if (it == _sessionMap.end())
			_sessionMap.insert(std::make_pair(sessionId, playerId));
		else
			it->second = playerId;
	}

	void PlayerManager::removeSessionId(const std::string& sessionId) {
		std::lock_guard<std::mutex> lck(_mtx);

		std::unordered_map<std::string, std::string>::iterator it = _sessionMap.find(sessionId);
		if (it != _sessionMap.end())
			_sessionMap.erase(it);
	}

	bool PlayerManager::getPlayerId(const std::string& sessionId, std::string& playerId) {
		std::lock_guard<std::mutex> lck(_mtx);

		std::unordered_map<std::string, std::string>::const_iterator it = _sessionMap.find(sessionId);
		if (it != _sessionMap.end()) {
			playerId = it->second;
			return true;
		}
		return false;
	}

	Player::Ptr PlayerManager::getPlayerBySessionId(const std::string& sessionId) {
		Player::Ptr ret;

		std::lock_guard<std::mutex> lck(_mtx);

		std::unordered_map<std::string, std::string>::const_iterator it1 = _sessionMap.find(sessionId);
		if (it1 == _sessionMap.end())
			return ret;
		std::unordered_map<std::string, Player::Ptr>::const_iterator it2 = _players.find(it1->second);
		if (it2 != _players.end())
			ret = it2->second;
		return ret;
	}

	void PlayerManager::addOfflinePlayer(const std::string& playerId) {
		std::lock_guard<std::mutex> lck(_mtx);

		_offlineIds.push_back(playerId);
	}

	bool PlayerManager::onTimer() {
		freeDormantPlayers();
		return false;
	}

	void PlayerManager::freeDormantPlayers() {
		std::lock_guard<std::mutex> lck(_mtx);

		if (_offlineIds.empty())
			return;
		time_t delta = 0LL;
		time_t nowTime = BaseUtils::getCurrentSecond();
		time_t offlineTime = 0LL;
		time_t referenceTime = 0LL;
		_inexactTime = nowTime;
		std::string venueId;
		std::list<std::string>::const_iterator it1 = _offlineIds.begin();
		std::unordered_map<std::string, Player::Ptr>::const_iterator it2;
		while (it1 != _offlineIds.end()) {
			it2 = _players.find(*it1);
			if (it2 == _players.end()) {
				it1 = _offlineIds.erase(it1);
				break;
			}
			const Player::Ptr& player = it2->second;
			if (!player->getOffline()) {
				it1 = _offlineIds.erase(it1);
				break;
			}
			player->getVenueId(venueId);
			if (!venueId.empty()) {
				// 玩家还在场地中，暂不释放
				it1++;
				continue;
			}
			player->getTime(offlineTime, referenceTime);
			delta = nowTime - offlineTime;
			if (delta < 30LL) {
				// 离线时间少于30秒，退出循环
				break;
			}
			delta = nowTime - referenceTime;
			if (delta < 20LL) {
				// 最新引用时间少于20秒，暂不释放
				it1++;
				continue;
			}
			// 释放玩家
			it1 = _offlineIds.erase(it1);
			_players.erase(it2);
		}
	}
}
