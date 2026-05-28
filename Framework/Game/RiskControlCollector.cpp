// RiskControlCollector.cpp

#include "RiskControlCollector.h"
#include "Rabbitmq/RabbitmqClient.h"
#include "Base/Log.h"

#include <chrono>
#include <json/json.h>

namespace NiuMa
{
	RoundRiskData::RoundRiskData()
		: gameType(0)
		, roundNo(0)
		, startTime(0)
		, endTime(0)
		, playerCount(0)
	{}

	RoundRiskData::~RoundRiskData() {}

	RiskControlCollector::RiskControlCollector() {}

	RiskControlCollector::~RiskControlCollector() {}

	void RiskControlCollector::startRound(const std::string& venueId, int gameType, int roundNo) {
		_currentData = RoundRiskData();
		_currentData.venueId = venueId;
		_currentData.gameType = gameType;
		_currentData.roundNo = roundNo;
		_currentData.startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
	}

	void RiskControlCollector::recordPlayer(const std::string& playerId, int seat,
		const std::string& ipAddress, const std::string& deviceId) {
		PlayerRiskData data;
		data.playerId = playerId;
		data.seat = seat;
		data.ipAddress = ipAddress;
		data.deviceId = deviceId;
		data.score = 0;
		data.winGold = 0;
		data.escaped = false;
		_currentData.players.push_back(data);
		_currentData.playerCount = static_cast<int>(_currentData.players.size());
	}

	void RiskControlCollector::recordOperation(const std::string& playerId, int elapsedMs) {
		int idx = findPlayerIndex(playerId);
		if (idx >= 0)
			_currentData.players[idx].operationTimes.push_back(elapsedMs);
	}

	void RiskControlCollector::recordScore(const std::string& playerId, int score, int64_t winGold) {
		int idx = findPlayerIndex(playerId);
		if (idx >= 0) {
			_currentData.players[idx].score = score;
			_currentData.players[idx].winGold = winGold;
		}
	}

	void RiskControlCollector::recordEscape(const std::string& playerId) {
		int idx = findPlayerIndex(playerId);
		if (idx >= 0)
			_currentData.players[idx].escaped = true;
	}

	void RiskControlCollector::setRandomSeedHash(const std::string& hash) {
		_currentData.randomSeedHash = hash;
	}

	void RiskControlCollector::finishRound() {
		_currentData.endTime = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
		publishToMQ();
	}

	const RoundRiskData& RiskControlCollector::getCurrentData() const {
		return _currentData;
	}

	int RiskControlCollector::findPlayerIndex(const std::string& playerId) const {
		for (size_t i = 0; i < _currentData.players.size(); i++) {
			if (_currentData.players[i].playerId == playerId)
				return static_cast<int>(i);
		}
		return -1;
	}

	void RiskControlCollector::publishToMQ() {
		// 构建JSON格式的风控数据
		Json::Value root;
		root["eventType"] = "RiskControlData";
		root["venueId"] = _currentData.venueId;
		root["gameType"] = _currentData.gameType;
		root["roundNo"] = _currentData.roundNo;
		root["startTime"] = Json::Value::Int64(_currentData.startTime);
		root["endTime"] = Json::Value::Int64(_currentData.endTime);
		root["playerCount"] = _currentData.playerCount;
		root["randomSeedHash"] = _currentData.randomSeedHash;

		Json::Value players(Json::arrayValue);
		for (const auto& p : _currentData.players) {
			Json::Value player;
			player["playerId"] = p.playerId;
			player["ipAddress"] = p.ipAddress;
			player["deviceId"] = p.deviceId;
			player["seat"] = p.seat;
			player["score"] = p.score;
			player["winGold"] = Json::Value::Int64(p.winGold);
			player["escaped"] = p.escaped;

			Json::Value opTimes(Json::arrayValue);
			for (int t : p.operationTimes)
				opTimes.append(t);
			player["operationTimes"] = opTimes;

			players.append(player);
		}
		root["players"] = players;

		Json::StreamWriterBuilder builder;
		std::string jsonStr = Json::writeString(builder, root);

		// 通过MQ发布
		RabbitmqClient::getSingleton().publishJson("game.direct", "web_server_001", "RiskControlData", jsonStr);
	}
}
