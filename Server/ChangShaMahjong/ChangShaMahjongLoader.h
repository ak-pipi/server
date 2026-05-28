// ChangShaMahjongLoader.h
// 长沙麻将游戏加载器

#ifndef _NIU_MA_CHANGSHA_MAHJONG_LOADER_H_
#define _NIU_MA_CHANGSHA_MAHJONG_LOADER_H_

#include "Venue/VenueLoader.h"

namespace NiuMa
{
	class ChangShaMahjongLoader : public VenueLoader
	{
	public:
		ChangShaMahjongLoader();
		virtual ~ChangShaMahjongLoader();

	public:
		virtual Venue::Ptr load(const std::string& id) override;
	};
}

#endif // _NIU_MA_CHANGSHA_MAHJONG_LOADER_H_
