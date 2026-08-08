// WalletEventTask.h

#ifndef _NIU_MA_WALLET_EVENT_TASK_H_
#define _NIU_MA_WALLET_EVENT_TASK_H_

#include <string>
#include <vector>
#include "Rabbitmq/RabbitmqClient.h"
#include "jsoncpp/include/json/json.h"

namespace NiuMa
{
	/**
	 * 积分钱包事件通知工具
	 * 游戏结算后通过 RabbitMQ 向 web_server 发送积分变动事件
	 *
	 * 事件类型:
	 *   GAME_WIN     - 赢牌获得积分
	 *   GAME_LOSE    - 输牌扣除积分
	 *   ROOM_FEE     - 房费消耗
	 *   SHUFFLE_FEE  - 下局开始前洗牌扣分
	 *
	 * 使用方式:
	 *   WalletEventTask::publish(playerId, "GAME_WIN", 100, "game_mahjong", venueId, "赢牌得分");
	 */
	class WalletEventTask
	{
	private:
		WalletEventTask() {}

	public:
		/**
		 * 发布积分变动事件到 MQ
		 * @param playerId  玩家ID
		 * @param eventType 事件类型: GAME_WIN / GAME_LOSE / ROOM_FEE / SHUFFLE_FEE
		 * @param amount    变动金额(正数)
		 * @param bizType   业务类型
		 * @param bizId     业务ID(如场地ID、牌局ID)
		 * @param remark    备注说明
		 * @param exchange  MQ交换机
		 * @param routingKey MQ路由键
		 */
		static void publish(const std::string& playerId,
			const std::string& eventType,
			int64_t amount,
			const std::string& bizType,
			const std::string& bizId,
			const std::string& remark,
			const std::string& exchange = "game.direct",
			const std::string& routingKey = "web_server_001");

		/**
		 * 发布积分变动事件，并指定返佣归属玩家拆分。
		 * 钱包扣减仍发生在 playerId，返佣按 commissionPlayerIds/commissionAmounts 分配。
		 */
		static void publishWithCommission(const std::string& playerId,
			const std::string& eventType,
			int64_t amount,
			const std::string& bizType,
			const std::string& bizId,
			const std::string& remark,
			const std::vector<std::string>& commissionPlayerIds,
			const std::vector<int64_t>& commissionAmounts,
			const std::string& exchange = "game.direct",
			const std::string& routingKey = "web_server_001");

		/**
		 * 发布房费扣除事件
		 */
		static void publishRoomFee(const std::string& playerId,
			int64_t amount,
			const std::string& venueId,
			const std::string& exchange,
			const std::string& routingKey);

		/**
		 * 发布洗牌扣分事件
		 */
		static void publishShuffleFee(const std::string& playerId,
			int64_t amount,
			const std::string& venueId,
			const std::string& exchange,
			const std::string& routingKey);
	};
}

#endif // !_NIU_MA_WALLET_EVENT_TASK_H_
