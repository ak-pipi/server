// DouDiZhuLoader.h

#ifndef _NIU_MA_DOU_DI_ZHU_LOADER_H_
#define _NIU_MA_DOU_DI_ZHU_LOADER_H_

#include "Venue/VenueLoader.h"
#include "DouDiZhuGameRule.h"

namespace NiuMa
{
	class DouDiZhuLoader : public VenueLoader
	{
	public:
		DouDiZhuLoader();
		virtual ~DouDiZhuLoader();

	public:
		virtual Venue::Ptr load(const std::string& id) override;

	private:
		std::shared_ptr<DouDiZhuGameRule> _rule;
	};
}

#endif
