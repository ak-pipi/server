// PaoDeKuaiLoader.h
// 跑得快游戏加载器

#ifndef _NIU_MA_PAODEKUAI_LOADER_H_
#define _NIU_MA_PAODEKUAI_LOADER_H_

#include "Venue/VenueLoader.h"

namespace NiuMa
{
	class PaoDeKuaiLoader : public VenueLoader
	{
	public:
		PaoDeKuaiLoader();
		virtual ~PaoDeKuaiLoader();

	public:
		virtual Venue::Ptr load(const std::string& id) override;
	};
}

#endif // _NIU_MA_PAODEKUAI_LOADER_H_
