// YiYangWaiHuZiLoader.h
#ifndef _NIU_MA_YIYANG_WAIHUZI_LOADER_H_
#define _NIU_MA_YIYANG_WAIHUZI_LOADER_H_

#include "Venue/VenueLoader.h"

namespace NiuMa
{
	class YiYangWaiHuZiLoader : public VenueLoader
	{
	public:
		YiYangWaiHuZiLoader();
		virtual ~YiYangWaiHuZiLoader();
		virtual Venue::Ptr load(const std::string& id) override;
	};
}
#endif
