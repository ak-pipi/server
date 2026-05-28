// YiYangWaiHuZiCard.cpp

#include "YiYangWaiHuZiCard.h"

namespace NiuMa
{
	WaiHuZiCard::WaiHuZiCard(int point_, int id_)
		: point(point_)
		, id(id_)
	{}

	WaiHuZiCard::~WaiHuZiCard() {}

	WaiHuZiCard& WaiHuZiCard::operator=(const WaiHuZiCard& c) {
		point = c.point;
		id = c.id;
		return *this;
	}

	bool WaiHuZiCard::operator==(const WaiHuZiCard& c) const {
		return point == c.point && id == c.id;
	}
}
