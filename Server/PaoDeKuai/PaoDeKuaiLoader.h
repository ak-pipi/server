// PaoDeKuaiLoader.h
// 跑得快游戏加载器

#ifndef _NIU_MA_PAODEKUAI_LOADER_H_
#define _NIU_MA_PAODEKUAI_LOADER_H_

#include "Venue/VenueLoader.h"
#include "PaoDeKuaiRule.h"

namespace NiuMa
{
	class PaoDeKuaiLoader : public VenueLoader
	{
	public:
		PaoDeKuaiLoader();
		virtual ~PaoDeKuaiLoader();

	public:
		virtual Venue::Ptr load(const std::string& id) override;

	private:
		std::shared_ptr<PaoDeKuaiRule> _rule;
	};
}

#endif // _NIU_MA_PAODEKUAI_LOADER_H_
