// DouDiZhuLoader.h

#ifndef _NIU_MA_DOU_DI_ZHU_LOADER_H_
#define _NIU_MA_DOU_DI_ZHU_LOADER_H_

#include "Venue/VenueLoader.h"

namespace NiuMa
{
	class DouDiZhuLoader : public VenueLoader
	{
	public:
		DouDiZhuLoader();
		virtual ~DouDiZhuLoader();

	public:
		virtual Venue::Ptr load(const std::string& id) override;

	};
}

#endif
