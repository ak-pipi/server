// YuanJiangQianFenLoader.h
#ifndef _NIU_MA_YUANJIANG_QIANFEN_LOADER_H_
#define _NIU_MA_YUANJIANG_QIANFEN_LOADER_H_
#include "Venue/VenueLoader.h"
namespace NiuMa {
	class YuanJiangQianFenLoader : public VenueLoader {
	public:
		YuanJiangQianFenLoader();
		virtual ~YuanJiangQianFenLoader();
		virtual Venue::Ptr load(const std::string& id) override;
	};
}
#endif
