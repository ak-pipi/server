// ReplayUtils.cpp

#include "ReplayUtils.h"
#include "Base/BaseUtils.h"
#include "Base/Log.h"

#include <sstream>
#include <zlib.h>

namespace NiuMa
{
	std::string ReplayUtils::generateSeedHash(const std::string& venueId, int roundNo, int banker, int64_t tileSeed) {
		std::ostringstream oss;
		oss << venueId << ":" << roundNo << ":" << banker << ":" << tileSeed;
		std::string raw = oss.str();

		// 简单hash: 使用zlib的crc32
		uLong crc = crc32(0L, Z_NULL, 0);
		crc = crc32(crc, reinterpret_cast<const Bytef*>(raw.data()), static_cast<uInt>(raw.size()));

		std::ostringstream hex;
		hex << std::hex << crc;
		return hex.str();
	}

	bool ReplayUtils::compressReplay(const char* data, int dataLen, std::string& output) {
		if (data == nullptr || dataLen <= 0)
			return false;

		uLongf dstLen = static_cast<uLongf>(dataLen) + 100;
		unsigned char* dstBuf = new unsigned char[dstLen];
		int ret = compress(dstBuf, &dstLen, reinterpret_cast<const unsigned char*>(data), static_cast<uLongf>(dataLen));
		if (ret != Z_OK) {
			delete[] dstBuf;
			LOG_ERROR("回放数据压缩失败");
			return false;
		}

		bool result = BaseUtils::encodeBase64(output, reinterpret_cast<const char*>(dstBuf), static_cast<int>(dstLen));
		delete[] dstBuf;

		if (!result)
			LOG_ERROR("回放数据Base64编码失败");
		return result;
	}
}
