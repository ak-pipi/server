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
		publishWithCommission(playerId, eventType, amount, bizType, bizId, remark,
			std::vector<std::string>(), std::vector<int64_t>(), exchange, routingKey);
	}

	void WalletEventTask::publishWithCommission(const std::string& playerId,
		const std::string& eventType,
		int64_t amount,
		const std::string& bizType,
		const std::string& bizId,
		const std::string& remark,
		const std::vector<std::string>& commissionPlayerIds,
		const std::vector<int64_t>& commissionAmounts,
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
		if (!commissionPlayerIds.empty() && commissionPlayerIds.size() == commissionAmounts.size()) {
			Json::Value ids(Json::arrayValue);
			Json::Value amounts(Json::arrayValue);
			for (size_t i = 0; i < commissionPlayerIds.size(); i++) {
				ids.append(commissionPlayerIds[i]);
				amounts.append(static_cast<Json::Int64>(commissionAmounts[i]));
			}
			json["commission_player_ids"] = ids;
			json["commission_amounts"] = amounts;
		}

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

	void WalletEventTask::publishShuffleFee(const std::string& playerId,
		int64_t amount,
		const std::string& venueId,
		const std::string& exchange,
		const std::string& routingKey)
	{
		publish(playerId, "SHUFFLE_FEE", amount, "SHUFFLE_FEE", venueId, "洗牌扣分", exchange, routingKey);
	}
}
