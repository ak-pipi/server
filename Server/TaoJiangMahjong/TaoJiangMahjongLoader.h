// TaoJiangMahjongLoader.h

#ifndef _NIU_MA_TAOJIANG_MAHJONG_LOADER_H_
#define _NIU_MA_TAOJIANG_MAHJONG_LOADER_H_

#include "Venue/VenueLoader.h"

namespace NiuMa
{
	/**
	 * 桃江麻将游戏加载器
	 */
	class TaoJiangMahjongLoader : public VenueLoader
	{
	public:
		TaoJiangMahjongLoader();
		virtual ~TaoJiangMahjongLoader();

	public:
		virtual Venue::Ptr load(const std::string& id) override;
	};
}

#endif // !_NIU_MA_TAOJIANG_MAHJONG_LOADER_H_
