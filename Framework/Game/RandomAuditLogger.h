// RandomAuditLogger.h
// 合规随机算法审计日志器

#ifndef _NIU_MA_RANDOM_AUDIT_LOGGER_H_
#define _NIU_MA_RANDOM_AUDIT_LOGGER_H_

#include <string>
#include <vector>
#include <ctime>

namespace NiuMa
{
	/**
	 * 随机算法审计日志器
	 * 记录每局发牌的随机种子、发牌顺序等信息
	 * 通过MQ将审计数据推送给web_server，用于合规审计
	 */
	class RandomAuditLogger
	{
	public:
		/**
		 * 记录一局随机发牌审计数据
		 * @param venueId   场地ID
		 * @param gameType  游戏类型
		 * @param roundNo   局号
		 * @param banker    庄家座位号
		 * @param seedHash  随机种子hash
		 * @param cardOrder 发牌顺序（牌ID列表，加密存储）
		 * @param playerIds 玩家ID列表
		 */
		static void logAudit(const std::string& venueId,
			int gameType,
			int roundNo,
			int banker,
			const std::string& seedHash,
			const std::vector<int>& cardOrder,
			const std::vector<std::string>& playerIds);

		/**
		 * 验证随机性合规
		 * 检查种子hash是否符合预期格式
		 * @param seedHash 种子hash
		 * @return true合规
		 */
		static bool validateSeedHash(const std::string& seedHash);
	};
}

#endif // _NIU_MA_RANDOM_AUDIT_LOGGER_H_
