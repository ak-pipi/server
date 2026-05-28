// RiskControlCollector.h
// 风控数据采集器

#ifndef _NIU_MA_RISK_CONTROL_COLLECTOR_H_
#define _NIU_MA_RISK_CONTROL_COLLECTOR_H_

#include <string>
#include <vector>
#include <unordered_map>
#include <ctime>

#include "msgpack/msgpack.hpp"

namespace NiuMa
{
	/**
	 * 单个玩家风控数据
	 */
	struct PlayerRiskData
	{
		// 玩家ID
		std::string playerId;

		// IP地址
		std::string ipAddress;

		// 设备ID
		std::string deviceId;

		// 座位号
		int seat;

		// 操作耗时列表（毫秒）
		std::vector<int> operationTimes;

		// 当局得分
		int score;

		// 当局赢的金币
		int64_t winGold;

		// 是否逃跑
		bool escaped;

		MSGPACK_DEFINE_MAP(playerId, ipAddress, deviceId, seat, operationTimes, score, winGold, escaped);
	};

	/**
	 * 一局风控数据
	 */
	class RoundRiskData
	{
	public:
		RoundRiskData();
		virtual ~RoundRiskData();

	public:
		// 场地ID
		std::string venueId;

		// 游戏类型ID
		int gameType;

		// 局号
		int roundNo;

		// 局开始时间
		int64_t startTime;

		// 局结束时间
		int64_t endTime;

		// 玩家数量
		int playerCount;

		// 各玩家风控数据
		std::vector<PlayerRiskData> players;

		// 随机种子hash
		std::string randomSeedHash;

		MSGPACK_DEFINE_MAP(venueId, gameType, roundNo, startTime, endTime, playerCount, players, randomSeedHash);
	};

	/**
	 * 风控数据采集器
	 * 每局采集同桌玩家组合、IP记录、设备ID、操作耗时
	 * 通过MQ将风控数据推送给web_server
	 */
	class RiskControlCollector
	{
	public:
		RiskControlCollector();
		virtual ~RiskControlCollector();

	public:
		// 开始一局采集
		void startRound(const std::string& venueId, int gameType, int roundNo);

		// 记录玩家信息
		void recordPlayer(const std::string& playerId, int seat,
			const std::string& ipAddress = "",
			const std::string& deviceId = "");

		// 记录操作耗时
		void recordOperation(const std::string& playerId, int elapsedMs);

		// 记录玩家得分
		void recordScore(const std::string& playerId, int score, int64_t winGold);

		// 记录玩家逃跑
		void recordEscape(const std::string& playerId);

		// 设置随机种子hash
		void setRandomSeedHash(const std::string& hash);

		// 结束一局采集并推送到MQ
		void finishRound();

		// 获取当前局的风控数据
		const RoundRiskData& getCurrentData() const;

	private:
		// 查找玩家数据索引
		int findPlayerIndex(const std::string& playerId) const;

		// 将风控数据推送到MQ
		void publishToMQ();

	private:
		// 当前局的风控数据
		RoundRiskData _currentData;
	};
}

#endif // _NIU_MA_RISK_CONTROL_COLLECTOR_H_
