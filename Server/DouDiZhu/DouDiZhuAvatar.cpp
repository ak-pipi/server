// DouDiZhuAvatar.cpp

#include "DouDiZhuAvatar.h"
#include "PokerCombination.h"

namespace NiuMa
{
	DouDiZhuAvatar::DouDiZhuAvatar(const PokerRule::Ptr& rule, const std::string& playerId, int seat, bool robot)
		: PokerAvatar(rule, playerId, seat, robot)
		, _ready(false)
		, _landlord(false)
		, _roundScore(0)
		, _totalScore(0)
		, _winGold(0)
	{}

	DouDiZhuAvatar::~DouDiZhuAvatar() {}

	void DouDiZhuAvatar::clear() {
		PokerAvatar::clear();
		_ready = false;
		_landlord = false;
		_roundScore = 0;
		_winGold = 0;
	}

	void DouDiZhuAvatar::combineAllGenres() {
		for (int i = 0; i < _rule->getPointNums(); i++)
			combineSamePoint(i);
		if (!_jokerCards[0].empty() && !_jokerCards[1].empty()) {
			PokerCombination::Ptr comb = allocateCombination();
			comb->setGenre(static_cast<int>(DouDiZhuGenre::Rocket));
			comb->setOfficerPoint(static_cast<int>(PokerPoint::Joker));
			comb->setOfficerSuit(static_cast<int>(PokerSuit::Big));
			comb->addCard(_jokerCards[0][0]);
			comb->addCard(_jokerCards[1][0]);
			insertCombination(comb);
		}
	}

	void DouDiZhuAvatar::combineSamePoint(int order) {
		int nums = _pointOrderNums[order];
		if (nums <= 0)
			return;
		const std::vector<int>& idsIn = _pointOrderCards[order];
		PokerCombination::Ptr comb = allocateCombination();
		comb->setGenre(static_cast<int>(DouDiZhuGenre::Single));
		comb->setOfficerPoint(_rule->getPointByOrder(order));
		comb->addCard(idsIn[0]);
		insertCombination(comb);
		if (nums >= 2) {
			comb = allocateCombination();
			comb->setGenre(static_cast<int>(DouDiZhuGenre::Pair));
			comb->setOfficerPoint(_rule->getPointByOrder(order));
			comb->addCard(idsIn[0]);
			comb->addCard(idsIn[1]);
			insertCombination(comb);
		}
		if (nums >= 3) {
			comb = allocateCombination();
			comb->setGenre(static_cast<int>(DouDiZhuGenre::Triple));
			comb->setOfficerPoint(_rule->getPointByOrder(order));
			for (int i = 0; i < 3; i++)
				comb->addCard(idsIn[i]);
			insertCombination(comb);
		}
		if (nums >= 4) {
			comb = allocateCombination();
			comb->setGenre(static_cast<int>(DouDiZhuGenre::Bomb));
			comb->setOfficerPoint(_rule->getPointByOrder(order));
			for (int i = 0; i < 4; i++)
				comb->addCard(idsIn[i]);
			insertCombination(comb);
		}
	}

	void DouDiZhuAvatar::candidateCombinationsImpl(int situation) {
		(void)situation;
		for (auto& item : _combinations) {
			for (auto& comb : item.second)
				_candidates.push_back(comb);
		}
	}

	void DouDiZhuAvatar::candidateCombinationsImpl(const PokerGenre& pg, int situation) {
		(void)situation;
		for (auto& item : _combinations) {
			for (auto& comb : item.second) {
				if (comb->getGenre() == pg.getGenre() ||
					comb->getGenre() == static_cast<int>(DouDiZhuGenre::Bomb) ||
					comb->getGenre() == static_cast<int>(DouDiZhuGenre::Rocket))
					_candidates.push_back(comb);
			}
		}
	}
}
