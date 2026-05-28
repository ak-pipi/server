// ReplayUtils.h

#ifndef _NIU_MA_REPLAY_UTILS_H_
#define _NIU_MA_REPLAY_UTILS_H_

#include <string>
#include <vector>

namespace NiuMa
{
	/**
	 * 回放系统增强工具
	 * 提供随机种子hash生成、回放数据压缩等功能
	 */
	class ReplayUtils
	{
	private:
		ReplayUtils() {}

	public:
		/**
		 * 生成随机种子hash
		 * 将牌池初始状态等信息组合后计算hash
		 * @param venueId  场地ID
		 * @param roundNo  局号
		 * @param banker   庄家座位
		 * @param tileSeed 牌池种子（可选）
		 * @return 种子hash字符串
		 */
		static std::string generateSeedHash(const std::string& venueId, int roundNo, int banker, int64_t tileSeed = 0);

		/**
		 * 压缩回放数据
		 * @param data   原始数据
		 * @param dataLen 原始数据长度
		 * @param output  输出的压缩数据(Base64编码)
		 * @return true成功，false失败
		 */
		static bool compressReplay(const char* data, int dataLen, std::string& output);
	};
}

#endif // !_NIU_MA_REPLAY_UTILS_H_
