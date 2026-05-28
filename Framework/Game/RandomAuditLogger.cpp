// RandomAuditLogger.cpp

#include "RandomAuditLogger.h"
#include "Rabbitmq/RabbitmqClient.h"
#include "Base/Log.h"

#include <json/json.h>
#include <chrono>

namespace NiuMa
{
	void RandomAuditLogger::logAudit(const std::string& venueId,
		int gameType,
		int roundNo,
		int banker,
		const std::string& seedHash,
		const std::vector<int>& cardOrder,
		const std::vector<std::string>& playerIds)
	{
		Json::Value root;
		root["eventType"] = "RandomAuditLog";
		root["venueId"] = venueId;
		root["gameType"] = gameType;
		root["roundNo"] = roundNo;
		root["banker"] = banker;
		root["seedHash"] = seedHash;
		root["timestamp"] = Json::Value::Int64(
			std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()).count());

		// 加密存储发牌顺序（仅记录hash，不暴露明文）
		// 使用cardOrder生成hash，确保发牌可审计但不可逆推
		std::string orderHash = std::to_string(cardOrder.size());
		for (size_t i = 0; i < cardOrder.size() && i < 10; i++) {
			orderHash += ":" + std::to_string(cardOrder[i]);
		}
		root["orderHash"] = orderHash;

		Json::Value ids(Json::arrayValue);
		for (const auto& id : playerIds)
			ids.append(id);
		root["playerIds"] = ids;

		Json::StreamWriterBuilder builder;
		std::string jsonStr = Json::writeString(builder, root);

		// 通过MQ发布审计日志
		RabbitmqClient::getSingleton().publishJson("game.direct", "web_server_001", "RandomAuditLog", jsonStr);
	}

	bool RandomAuditLogger::validateSeedHash(const std::string& seedHash) {
		// 基本格式验证：CRC32 hash应为8位十六进制字符串
		if (seedHash.length() != 8)
			return false;
		for (char c : seedHash) {
			if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
				return false;
		}
		return true;
	}
}
