// HongZhongMahjongLoader.h

#ifndef _NIU_MA_HONGZHONG_MAHJONG_LOADER_H_
#define _NIU_MA_HONGZHONG_MAHJONG_LOADER_H_

#include "Venue/VenueLoader.h"

namespace NiuMa
{
	/**
	 * 红中麻将游戏加载器
	 */
	class HongZhongMahjongLoader : public VenueLoader
	{
	public:
		HongZhongMahjongLoader();
		virtual ~HongZhongMahjongLoader();

	public:
		virtual Venue::Ptr load(const std::string& id) override;
	};
}

#endif // !_NIU_MA_HONGZHONG_MAHJONG_LOADER_H_
