// WalletEventTask.cpp

#include "WalletEventTask.h"
#include "Base/Log.h"
#include "Base/BaseUtils.h"

#include <atomic>

namespace NiuMa
{
	static std::atomic<unsigned long long> walletEventSeq(0);

	void WalletEventTask::publish(const std::string& playerId,
		const std::string& eventType,
		int64_t amount,
		const std::string& bizType,
		const std::string& bizId,
		const std::string& remark,
		const std::string& exchange,
		const std::string& routingKey)
	{
		Json::Value json(Json::objectValue);
		unsigned long long seq = ++walletEventSeq;
		std::string refNo = "cpp:" + eventType + ":" + bizType + ":" + bizId + ":" + playerId + ":"
			+ std::to_string(BaseUtils::getCurrentMillisecond()) + ":" + std::to_string(seq);
		json["user_id"] = playerId;
		json["wallet_type"] = "gold";
		json["change_amount"] = static_cast<Json::Int64>(amount);
		json["event_type"] = eventType;
		json["biz_type"] = bizType;
		json["biz_id"] = bizId;
		json["ref_no"] = refNo;
		json["remark"] = remark;

		std::string body = json.toStyledString();
		bool ret = RabbitmqClient::getSingleton().publishJson(exchange, routingKey, "WalletChangeEvent", body);
		if (!ret)
			LOG_ERROR("发布积分变动事件失败，玩家: " + playerId + ", 事件: " + eventType);
	}

	void WalletEventTask::publishRoomFee(const std::string& playerId,
		int64_t amount,
		const std::string& venueId,
		const std::string& exchange,
		const std::string& routingKey)
	{
		publish(playerId, "ROOM_FEE", amount, "room_fee", venueId, "房费消耗", exchange, routingKey);
	}
}
