// YiYangWaiHuZiAvatar.cpp

#include "YiYangWaiHuZiAvatar.h"
#include <algorithm>

namespace NiuMa
{
	YiYangWaiHuZiAvatar::YiYangWaiHuZiAvatar(const std::string& playerId, int seat, bool robot)
		: GameAvatar(playerId, robot)
		, _ready(false)
		, _hu(false)
		, _discarded(false)
		, _roundScore(0)
		, _winGold(0.0)
		, _totalHuXi(0)
	{}

	YiYangWaiHuZiAvatar::~YiYangWaiHuZiAvatar() {}

	void YiYangWaiHuZiAvatar::clear() {
		_handCards.clear();
		_exposedCombs.clear();
		_ready = false;
		_hu = false;
		_discarded = false;
		_roundScore = 0;
		_winGold = 0.0;
		_totalHuXi = 0;
	}

	void YiYangWaiHuZiAvatar::setHandCards(const WaiHuZiCardArray& cards) {
		_handCards = cards;
	}

	void YiYangWaiHuZiAvatar::addCard(const WaiHuZiCard& card) {
		_handCards.push_back(card);
	}

	void YiYangWaiHuZiAvatar::removeCardById(int id) {
		for (auto it = _handCards.begin(); it != _handCards.end(); ++it) {
			if (it->getId() == id) {
				_handCards.erase(it);
				return;
			}
		}
	}

	void YiYangWaiHuZiAvatar::removeCardsByIds(const std::vector<int>& ids) {
		for (int id : ids)
			removeCardById(id);
	}

	bool YiYangWaiHuZiAvatar::hasCard(int id) const {
		for (auto& c : _handCards) {
			if (c.getId() == id)
				return true;
		}
		return false;
	}

	void YiYangWaiHuZiAvatar::addExposedComb(const WaiHuZiCombination& comb) {
		_exposedCombs.push_back(comb);
	}

	int YiYangWaiHuZiAvatar::calcHuXi() const {
		int total = 0;
		// 已亮出的组合胡息
		for (auto& comb : _exposedCombs)
			total += comb.huXi;
		// 手牌中的暗组合胡息（坎牌等）由Room中详细计算
		return total;
	}
}
