// DouDiZhuGameRule.cpp

#include "DouDiZhuGameRule.h"
#include "Poker/PokerUtilities.h"

#include <json/json.h>
#include <algorithm>
#include <map>
#include <vector>

namespace NiuMa
{
	DouDiZhuGameRule::DouDiZhuGameRule()
		: _playerCount(2)
		, _handCardCount(17)
		, _bottomCardCount(3)
		, _baseScore(1)
		, _callTimeout(15000)
		, _autoPlayTimeout(20000)
		, _maxRoundScore(0)
		, _removeThreeAndFour(true)
	{
		_orderTable = new CardOrderTable();
	}

	DouDiZhuGameRule::~DouDiZhuGameRule() {}

	void DouDiZhuGameRule::initialise() {
		sortPointOrder();
		sortSuitOrder();
	}

	int DouDiZhuGameRule::getPackNums() const {
		return 1;
	}

	int DouDiZhuGameRule::getPointNums() const {
		return 14;
	}

	void DouDiZhuGameRule::sortPointOrder() {
		_orderTable->setPointOrder(static_cast<int>(PokerPoint::Three), 0);
		_orderTable->setPointOrder(static_cast<int>(PokerPoint::Four), 1);
		_orderTable->setPointOrder(static_cast<int>(PokerPoint::Five), 2);
		_orderTable->setPointOrder(static_cast<int>(PokerPoint::Six), 3);
		_orderTable->setPointOrder(static_cast<int>(PokerPoint::Seven), 4);
		_orderTable->setPointOrder(static_cast<int>(PokerPoint::Eight), 5);
		_orderTable->setPointOrder(static_cast<int>(PokerPoint::Nine), 6);
		_orderTable->setPointOrder(static_cast<int>(PokerPoint::Ten), 7);
		_orderTable->setPointOrder(static_cast<int>(PokerPoint::Jack), 8);
		_orderTable->setPointOrder(static_cast<int>(PokerPoint::Queen), 9);
		_orderTable->setPointOrder(static_cast<int>(PokerPoint::King), 10);
		_orderTable->setPointOrder(static_cast<int>(PokerPoint::Ace), 11);
		_orderTable->setPointOrder(static_cast<int>(PokerPoint::Two), 12);
		_orderTable->setPointOrder(static_cast<int>(PokerPoint::Joker), 13);
	}

	void DouDiZhuGameRule::sortSuitOrder() {
		_orderTable->setSuitOrder(static_cast<int>(PokerSuit::Diamond), 0);
		_orderTable->setSuitOrder(static_cast<int>(PokerSuit::Club), 1);
		_orderTable->setSuitOrder(static_cast<int>(PokerSuit::Heart), 2);
		_orderTable->setSuitOrder(static_cast<int>(PokerSuit::Spade), 3);
		_orderTable->setSuitOrder(static_cast<int>(PokerSuit::Little), 4);
		_orderTable->setSuitOrder(static_cast<int>(PokerSuit::Big), 5);
	}

	bool DouDiZhuGameRule::isDisapprovedCard(const PokerCard& c) const {
		if (!_removeThreeAndFour)
			return false;
		int point = c.getPoint();
		return point == static_cast<int>(PokerPoint::Three) ||
			point == static_cast<int>(PokerPoint::Four);
	}

	bool DouDiZhuGameRule::isRocket(const CardArray& cards, PokerCard& officer) const {
		if (cards.size() != 2)
			return false;
		bool little = false;
		bool big = false;
		for (const PokerCard& c : cards) {
			if (c.getPoint() != static_cast<int>(PokerPoint::Joker))
				return false;
			if (c.getSuit() == static_cast<int>(PokerSuit::Little))
				little = true;
			else if (c.getSuit() == static_cast<int>(PokerSuit::Big)) {
				big = true;
				officer = c;
			}
		}
		return little && big;
	}

	bool DouDiZhuGameRule::samePointGroups(const CardArray& cards, int groupSize, int minGroups, PokerCard& officer) const {
		if (cards.empty() || static_cast<int>(cards.size()) % groupSize != 0)
			return false;
		int groups = static_cast<int>(cards.size()) / groupSize;
		if (groups < minGroups)
			return false;
		for (int i = 0; i < groups; i++) {
			const PokerCard& first = cards.at(i * groupSize);
			if (straightTripleExcluded(first))
				return false;
			for (int j = 1; j < groupSize; j++) {
				if (cards.at(i * groupSize + j).getPoint() != first.getPoint())
					return false;
			}
			if (i > 0) {
				int prev = getPointOrder(cards.at((i - 1) * groupSize).getPoint());
				int now = getPointOrder(first.getPoint());
				if (now - prev != 1)
					return false;
			}
			officer = cards.at(i * groupSize + groupSize - 1);
		}
		return true;
	}

	bool DouDiZhuGameRule::findPlane(const CardArray& cards, int wingCardNums, PokerCard& officer) const {
		int nums = static_cast<int>(cards.size());
		int unit = 3 + wingCardNums;
		if (unit <= 3 || nums < unit * 2 || nums % unit != 0)
			return false;
		int planeLen = nums / unit;
		std::map<int, int> counts;
		std::map<int, PokerCard> highCards;
		for (const PokerCard& c : cards) {
			counts[c.getPoint()]++;
			highCards[c.getPoint()] = c;
		}
		std::vector<int> triplePoints;
		for (const auto& item : counts) {
			if (item.second >= 3 && getPointOrder(item.first) < 12)
				triplePoints.push_back(item.first);
		}
		std::sort(triplePoints.begin(), triplePoints.end(), [this](int a, int b) {
			return getPointOrder(a) < getPointOrder(b);
		});
		if (static_cast<int>(triplePoints.size()) < planeLen)
			return false;
		for (int i = 0; i <= static_cast<int>(triplePoints.size()) - planeLen; i++) {
			bool continuous = true;
			for (int j = 1; j < planeLen; j++) {
				if (getPointOrder(triplePoints[i + j]) - getPointOrder(triplePoints[i + j - 1]) != 1) {
					continuous = false;
					break;
				}
			}
			if (!continuous)
				continue;
			std::map<int, int> rest = counts;
			for (int j = 0; j < planeLen; j++)
				rest[triplePoints[i + j]] -= 3;
			int restCards = 0;
			bool ok = true;
			for (const auto& item : rest) {
				if (item.second < 0) {
					ok = false;
					break;
				}
				if (wingCardNums == 2 && item.second != 0 && item.second != 2) {
					ok = false;
					break;
				}
				restCards += item.second;
			}
			if (ok && restCards == planeLen * wingCardNums) {
				officer = highCards[triplePoints[i + planeLen - 1]];
				return true;
			}
		}
		return false;
	}

	bool DouDiZhuGameRule::isFourWithTwo(const CardArray& cards, bool pairs, PokerCard& officer) const {
		int nums = static_cast<int>(cards.size());
		if ((!pairs && nums != 6) || (pairs && nums != 8))
			return false;
		std::map<int, int> counts;
		std::map<int, PokerCard> highCards;
		for (const PokerCard& c : cards) {
			counts[c.getPoint()]++;
			highCards[c.getPoint()] = c;
		}
		int fourPoint = static_cast<int>(PokerPoint::Invalid);
		for (const auto& item : counts) {
			if (item.second == 4) {
				fourPoint = item.first;
				break;
			}
		}
		if (fourPoint == static_cast<int>(PokerPoint::Invalid))
			return false;
		if (pairs) {
			for (const auto& item : counts) {
				if (item.first == fourPoint)
					continue;
				if (item.second != 2)
					return false;
			}
		}
		officer = highCards[fourPoint];
		return true;
	}

	int DouDiZhuGameRule::predicateCardGenre(PokerGenre& pcg) const {
		const CardArray& cards = pcg.getCards();
		int nums = static_cast<int>(cards.size());
		if (nums <= 0)
			return static_cast<int>(DouDiZhuGenre::Invalid);

		PokerCard officer;
		if (isRocket(cards, officer)) {
			pcg.setGenre(static_cast<int>(DouDiZhuGenre::Rocket));
			pcg.setOfficer(officer);
			return static_cast<int>(DouDiZhuGenre::Rocket);
		}
		if (nums == 1) {
			pcg.setGenre(static_cast<int>(DouDiZhuGenre::Single));
			pcg.setOfficer(cards.at(0));
			return static_cast<int>(DouDiZhuGenre::Single);
		}
		if (nums == 2 && pcg.samePoint()) {
			pcg.setGenre(static_cast<int>(DouDiZhuGenre::Pair));
			pcg.setOfficer(cards.at(1));
			return static_cast<int>(DouDiZhuGenre::Pair);
		}
		if (nums == 3 && pcg.samePoint()) {
			pcg.setGenre(static_cast<int>(DouDiZhuGenre::Triple));
			pcg.setOfficer(cards.at(2));
			return static_cast<int>(DouDiZhuGenre::Triple);
		}
		if (nums == 4 && pcg.samePoint()) {
			pcg.setGenre(static_cast<int>(DouDiZhuGenre::Bomb));
			pcg.setOfficer(cards.at(3));
			return static_cast<int>(DouDiZhuGenre::Bomb);
		}
		if (nums == 4 && pcg.carryM_N(3, 1) && PokerUtilities::rfindSamePointN(cards, officer, 3)) {
			pcg.setGenre(static_cast<int>(DouDiZhuGenre::TripleOne));
			pcg.setOfficer(officer);
			return static_cast<int>(DouDiZhuGenre::TripleOne);
		}
		if (nums == 5 && pcg.carryM_N(3, 2) && PokerUtilities::rfindSamePointN(cards, officer, 3)) {
			pcg.setGenre(static_cast<int>(DouDiZhuGenre::TriplePair));
			pcg.setOfficer(officer);
			return static_cast<int>(DouDiZhuGenre::TriplePair);
		}
		if (nums >= 5 && straight(cards)) {
			pcg.setGenre(static_cast<int>(DouDiZhuGenre::Straight));
			pcg.setOfficer(cards.at(nums - 1));
			return static_cast<int>(DouDiZhuGenre::Straight);
		}
		if (nums >= 6 && straightPair(cards)) {
			pcg.setGenre(static_cast<int>(DouDiZhuGenre::StraightPair));
			pcg.setOfficer(cards.at(nums - 1));
			return static_cast<int>(DouDiZhuGenre::StraightPair);
		}
		if (nums >= 6 && samePointGroups(cards, 3, 2, officer)) {
			pcg.setGenre(static_cast<int>(DouDiZhuGenre::Plane));
			pcg.setOfficer(officer);
			return static_cast<int>(DouDiZhuGenre::Plane);
		}
		if (findPlane(cards, 1, officer)) {
			pcg.setGenre(static_cast<int>(DouDiZhuGenre::PlaneOne));
			pcg.setOfficer(officer);
			return static_cast<int>(DouDiZhuGenre::PlaneOne);
		}
		if (findPlane(cards, 2, officer)) {
			pcg.setGenre(static_cast<int>(DouDiZhuGenre::PlanePair));
			pcg.setOfficer(officer);
			return static_cast<int>(DouDiZhuGenre::PlanePair);
		}
		if (isFourWithTwo(cards, false, officer)) {
			pcg.setGenre(static_cast<int>(DouDiZhuGenre::FourTwoSingle));
			pcg.setOfficer(officer);
			return static_cast<int>(DouDiZhuGenre::FourTwoSingle);
		}
		if (isFourWithTwo(cards, true, officer)) {
			pcg.setGenre(static_cast<int>(DouDiZhuGenre::FourTwoPair));
			pcg.setOfficer(officer);
			return static_cast<int>(DouDiZhuGenre::FourTwoPair);
		}
		return static_cast<int>(DouDiZhuGenre::Invalid);
	}

	int DouDiZhuGameRule::getGenreCardNums(int genre) const {
		switch (static_cast<DouDiZhuGenre>(genre)) {
		case DouDiZhuGenre::Single: return 1;
		case DouDiZhuGenre::Pair: return 2;
		case DouDiZhuGenre::Triple: return 3;
		case DouDiZhuGenre::TripleOne: return 4;
		case DouDiZhuGenre::TriplePair: return 5;
		case DouDiZhuGenre::Straight: return 5;
		case DouDiZhuGenre::StraightPair: return 6;
		case DouDiZhuGenre::Plane: return 6;
		case DouDiZhuGenre::PlaneOne: return 8;
		case DouDiZhuGenre::PlanePair: return 10;
		case DouDiZhuGenre::FourTwoSingle: return 6;
		case DouDiZhuGenre::FourTwoPair: return 8;
		case DouDiZhuGenre::Bomb: return 4;
		case DouDiZhuGenre::Rocket: return 2;
		default: return 0;
		}
	}

	bool DouDiZhuGameRule::isValidGenre(int genre) const {
		return genre >= static_cast<int>(DouDiZhuGenre::Single) &&
			genre <= static_cast<int>(DouDiZhuGenre::Rocket);
	}

	bool DouDiZhuGameRule::straightExcluded(const PokerCard& c) const {
		int point = c.getPoint();
		return point == static_cast<int>(PokerPoint::Two) ||
			point == static_cast<int>(PokerPoint::Joker);
	}

	bool DouDiZhuGameRule::straightPairExcluded(const PokerCard& c) const {
		return straightExcluded(c);
	}

	bool DouDiZhuGameRule::straightTripleExcluded(const PokerCard& c) const {
		return straightExcluded(c);
	}

	bool DouDiZhuGameRule::butterflyExcluded(const PokerCard& c) const {
		return straightExcluded(c);
	}

	int DouDiZhuGameRule::getBombOrder(int genre) const {
		if (genre == static_cast<int>(DouDiZhuGenre::Bomb))
			return 0;
		if (genre == static_cast<int>(DouDiZhuGenre::Rocket))
			return 1;
		return -1;
	}

	int DouDiZhuGameRule::getBombByOrder(int order) const {
		if (order == 0)
			return static_cast<int>(DouDiZhuGenre::Bomb);
		if (order == 1)
			return static_cast<int>(DouDiZhuGenre::Rocket);
		return static_cast<int>(DouDiZhuGenre::Invalid);
	}

	void DouDiZhuGameRule::loadConfig(const std::string& ruleConfig) {
		if (ruleConfig.empty())
			return;
		Json::Value root;
		Json::CharReaderBuilder builder;
		Json::CharReader* reader = builder.newCharReader();
		std::string errs;
		if (!reader->parse(ruleConfig.c_str(), ruleConfig.c_str() + ruleConfig.size(), &root, &errs)) {
			delete reader;
			return;
		}
		delete reader;

		if (root.isMember("base_score") && root["base_score"].isInt())
			_baseScore = std::max(1, root["base_score"].asInt());
		if (root.isMember("call_timeout") && root["call_timeout"].isInt())
			_callTimeout = std::max(5000, root["call_timeout"].asInt());
		if (root.isMember("auto_play_timeout") && root["auto_play_timeout"].isInt())
			_autoPlayTimeout = std::max(5000, root["auto_play_timeout"].asInt());
		if (root.isMember("max_score") && root["max_score"].isInt())
			_maxRoundScore = std::max(0, root["max_score"].asInt());
	}
}
