// PaoDeKuaiRule.cpp

#include "PaoDeKuaiRule.h"
#include "PokerCard.h"
#include "Base/Log.h"

#include <json/json.h>
#include <algorithm>
#include <unordered_map>

namespace NiuMa
{
	namespace
	{
		void setOfficerByPoint(PokerGenre& pcg, const CardArray& cards, int point)
		{
			for (CardArray::const_reverse_iterator it = cards.rbegin(); it != cards.rend(); ++it) {
				if (it->getPoint() == point) {
					pcg.setOfficer(*it);
					return;
				}
			}
		}

		std::unordered_map<int, int> countPoints(const CardArray& cards)
		{
			std::unordered_map<int, int> counts;
			for (const PokerCard& c : cards)
				counts[c.getPoint()]++;
			return counts;
		}
	}

	PaoDeKuaiRule::PaoDeKuaiRule()
		: _cardCount(15)
		, _playerCount(2)
		, _baseScore(1)
		, _roundCount(8)
		, _bombDouble(true)
		, _bombScore(10)
		, _allowPass(true)
		, _autoPlayTimeout(180000)
		, _maxRoundScore(0)
		, _mustIncludeSpade3(false)
		, _springDouble(true)
		, _forcePlayIfCanBeat(true)
	{
		_orderTable = new CardOrderTable();
	}

	PaoDeKuaiRule::~PaoDeKuaiRule() {}

	void PaoDeKuaiRule::initialise() {
		sortPointOrder();
		sortSuitOrder();
	}

	int PaoDeKuaiRule::getPackNums() const {
		return 1;
	}

	int PaoDeKuaiRule::getPointNums() const {
		return 14; // 3~10, J, Q, K, A, 2, Joker(牌池中已移除)
	}

	void PaoDeKuaiRule::sortPointOrder() {
		// 跑得快中牌值从小到大的顺序：
		// 3, 4, 5, 6, 7, 8, 9, 10, J, Q, K, A, 2, 小王, 大王
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

	void PaoDeKuaiRule::sortSuitOrder() {
		// 花色从小到大：方块、梅花、红桃、黑桃
		_orderTable->setSuitOrder(static_cast<int>(PokerSuit::Diamond), 0);
		_orderTable->setSuitOrder(static_cast<int>(PokerSuit::Club), 1);
		_orderTable->setSuitOrder(static_cast<int>(PokerSuit::Heart), 2);
		_orderTable->setSuitOrder(static_cast<int>(PokerSuit::Spade), 3);
		// 小王、大王花色顺序
		_orderTable->setSuitOrder(static_cast<int>(PokerSuit::Little), 0);
		_orderTable->setSuitOrder(static_cast<int>(PokerSuit::Big), 1);
	}

	bool PaoDeKuaiRule::isDisapprovedCard(const PokerCard& c) const {
		int pt = c.getPoint();
		int suit = c.getSuit();
		// 15张跑得快牌库：去掉大小王、三张2、三张A、一张K，共45张牌。
		if (pt == static_cast<int>(PokerPoint::Joker))
			return true;
		if (pt == static_cast<int>(PokerPoint::Two))
			return suit != static_cast<int>(PokerSuit::Spade);
		if (pt == static_cast<int>(PokerPoint::Ace))
			return suit != static_cast<int>(PokerSuit::Spade);
		if (pt == static_cast<int>(PokerPoint::King))
			return suit == static_cast<int>(PokerSuit::Diamond);
		return false;
	}

	int PaoDeKuaiRule::predicateCardGenre(PokerGenre& pcg) const {
		const CardArray& cards = pcg.getCards();
		int n = static_cast<int>(cards.size());
		if (n <= 0)
			return static_cast<int>(PaoDeKuaiGenre::Invalid);

		// 王炸（双王）
		if (n == 2) {
			bool hasLittle = false;
			bool hasBig = false;
			for (auto& c : cards) {
				if (c.getSuit() == static_cast<int>(PokerSuit::Little))
					hasLittle = true;
				else if (c.getSuit() == static_cast<int>(PokerSuit::Big))
					hasBig = true;
			}
			if (hasLittle && hasBig) {
				pcg.setGenre(static_cast<int>(PaoDeKuaiGenre::Rocket));
				return static_cast<int>(PaoDeKuaiGenre::Rocket);
			}
		}

		// 单张
		if (n == 1) {
			pcg.setGenre(static_cast<int>(PaoDeKuaiGenre::Single));
			pcg.setOfficer(cards[0]);
			return static_cast<int>(PaoDeKuaiGenre::Single);
		}

		// 对子
		if (n == 2 && pcg.samePoint()) {
			pcg.setGenre(static_cast<int>(PaoDeKuaiGenre::Pair));
			pcg.setOfficer(cards[1]);
			return static_cast<int>(PaoDeKuaiGenre::Pair);
		}

		// 三条
		if (n == 3 && pcg.samePoint()) {
			pcg.setGenre(static_cast<int>(PaoDeKuaiGenre::Triple));
			pcg.setOfficer(cards[2]);
			return static_cast<int>(PaoDeKuaiGenre::Triple);
		}

		// 炸弹（四张相同）
		if (n == 4 && pcg.samePoint()) {
			pcg.setGenre(static_cast<int>(PaoDeKuaiGenre::Bomb));
			pcg.setOfficer(cards[3]);
			return static_cast<int>(PaoDeKuaiGenre::Bomb);
		}

		// 顺子（5张及以上连续单张）
		if (n >= 5 && straight(cards)) {
			pcg.setGenre(static_cast<int>(PaoDeKuaiGenre::Straight));
			pcg.setOfficer(cards[n - 1]);
			return static_cast<int>(PaoDeKuaiGenre::Straight);
		}

		// 连对（2对及以上连续对子）
		if (n >= 4 && n % 2 == 0 && straightPair(cards)) {
			pcg.setGenre(static_cast<int>(PaoDeKuaiGenre::StraightPair));
			pcg.setOfficer(cards[n - 1]);
			return static_cast<int>(PaoDeKuaiGenre::StraightPair);
		}

		// 三带一
		if (n == 4 && pcg.carryM_N(3, 1)) {
			pcg.setGenre(static_cast<int>(PaoDeKuaiGenre::TripleOne));
			// 找出三张相同的主牌作为officer
			for (int i = 2; i < n; i++) {
				if (cards[i].getPoint() == cards[i - 2].getPoint()) {
					pcg.setOfficer(cards[i]);
					break;
				}
			}
			return static_cast<int>(PaoDeKuaiGenre::TripleOne);
		}

		// 三带二：跑得快三张可以任意带两张，不要求带对子。
		if (n == 5) {
			std::unordered_map<int, int> pointCounts = countPoints(cards);
			int triplePoint = -1;
			bool ok = true;
			for (const auto& kv : pointCounts) {
				if (kv.second == 3) {
					if (triplePoint > 0) {
						ok = false;
						break;
					}
					triplePoint = kv.first;
				}
				else if (kv.second > 3) {
					ok = false;
					break;
				}
			}
			if (!ok || triplePoint < 0)
				return static_cast<int>(PaoDeKuaiGenre::Invalid);
			pcg.setGenre(static_cast<int>(PaoDeKuaiGenre::TriplePair));
			setOfficerByPoint(pcg, cards, triplePoint);
			return static_cast<int>(PaoDeKuaiGenre::TriplePair);
		}

		// 飞机不带（2组及以上连续三条）
		if (n >= 6 && n % 3 == 0 && straightTriple(cards)) {
			pcg.setGenre(static_cast<int>(PaoDeKuaiGenre::Plane));
			pcg.setOfficer(cards[n - 1]);
			return static_cast<int>(PaoDeKuaiGenre::Plane);
		}

		// 飞机带单（连续三条+等量单张）
		// 飞机带对（连续三条+等量对子）
		if ((n % 4 == 0 && n >= 8) || (n % 5 == 0 && n >= 10)) {
			std::unordered_map<int, int> pointCounts = countPoints(cards);
			int planeLen = (n % 4 == 0) ? (n / 4) : (n / 5);
			for (int start = 0; start <= 11 - planeLen + 1; start++) {
				bool ok = true;
				for (int i = 0; i < planeLen; i++) {
					int point = getPointByOrder(start + i);
					if (pointCounts[point] < 3) {
						ok = false;
						break;
					}
				}
				if (!ok)
					continue;

				std::unordered_map<int, int> mates = pointCounts;
				for (int i = 0; i < planeLen; i++) {
					int point = getPointByOrder(start + i);
					mates[point] -= 3;
				}

				if (n % 4 == 0) {
					int singles = 0;
					for (const auto& kv : mates)
						singles += kv.second;
					if (singles == planeLen) {
						pcg.setGenre(static_cast<int>(PaoDeKuaiGenre::PlaneOne));
						setOfficerByPoint(pcg, cards, getPointByOrder(start + planeLen - 1));
						return static_cast<int>(PaoDeKuaiGenre::PlaneOne);
					}
				}
				else {
					int pairs = 0;
					bool pairOk = true;
					for (const auto& kv : mates) {
						if (kv.second == 0)
							continue;
						if (kv.second != 2) {
							pairOk = false;
							break;
						}
						pairs++;
					}
					if (pairOk && pairs == planeLen) {
						pcg.setGenre(static_cast<int>(PaoDeKuaiGenre::PlanePair));
						setOfficerByPoint(pcg, cards, getPointByOrder(start + planeLen - 1));
						return static_cast<int>(PaoDeKuaiGenre::PlanePair);
					}
				}
			}
		}

		return static_cast<int>(PaoDeKuaiGenre::Invalid);
	}

	int PaoDeKuaiRule::getGenreCardNums(int genre) const {
		switch (static_cast<PaoDeKuaiGenre>(genre)) {
		case PaoDeKuaiGenre::Single:
			return 1;
		case PaoDeKuaiGenre::Pair:
			return 2;
		case PaoDeKuaiGenre::Triple:
			return 3;
		case PaoDeKuaiGenre::TripleOne:
			return 4;
		case PaoDeKuaiGenre::TriplePair:
			return 5;
		case PaoDeKuaiGenre::Straight:
			return 5; // 最少5张
		case PaoDeKuaiGenre::StraightPair:
			return 4; // 最少2对
		case PaoDeKuaiGenre::Bomb:
			return 4;
		case PaoDeKuaiGenre::Rocket:
			return 2;
		default:
			return 0;
		}
	}

	bool PaoDeKuaiRule::isValidGenre(int genre) const {
		return genre >= static_cast<int>(PaoDeKuaiGenre::Single) &&
			genre <= static_cast<int>(PaoDeKuaiGenre::Rocket);
	}

	int PaoDeKuaiRule::compareGenre(const PokerGenre& pcg1, const PokerGenre& pcg2) const {
		int g1 = pcg1.getGenre();
		int g2 = pcg2.getGenre();

		// 王炸最大
		if (g1 == static_cast<int>(PaoDeKuaiGenre::Rocket)) {
			if (g2 == static_cast<int>(PaoDeKuaiGenre::Rocket))
				return 0;
			return 1;
		}
		if (g2 == static_cast<int>(PaoDeKuaiGenre::Rocket))
			return 2;

		// 炸弹大于非炸弹
		bool bomb1 = (g1 == static_cast<int>(PaoDeKuaiGenre::Bomb));
		bool bomb2 = (g2 == static_cast<int>(PaoDeKuaiGenre::Bomb));
		if (bomb1 && !bomb2)
			return 1;
		if (!bomb1 && bomb2)
			return 2;
		if (bomb1 && bomb2) {
			// 同为炸弹比较主牌
			return comparePoint(pcg1.getOfficer().getPoint(), pcg2.getOfficer().getPoint());
		}

		// 相同牌型比较主牌大小
		if (g1 != g2)
			return 0; // 不同牌型无法比较

		// 顺子/连对/飞机还需张数相同
		if (g1 == static_cast<int>(PaoDeKuaiGenre::Straight) ||
			g1 == static_cast<int>(PaoDeKuaiGenre::StraightPair) ||
			g1 == static_cast<int>(PaoDeKuaiGenre::Plane) ||
			g1 == static_cast<int>(PaoDeKuaiGenre::PlaneOne) ||
			g1 == static_cast<int>(PaoDeKuaiGenre::PlanePair)) {
			if (pcg1.getCardNums() != pcg2.getCardNums())
				return 0;
		}

		// 跑得快按点数压牌，不使用花色大小。
		return comparePoint(pcg1.getOfficer().getPoint(), pcg2.getOfficer().getPoint());
	}

	bool PaoDeKuaiRule::straightExcluded(const PokerCard& c) const {
		// 2和王者不能参与顺子
		int pt = c.getPoint();
		return pt == static_cast<int>(PokerPoint::Two) ||
			pt == static_cast<int>(PokerPoint::Joker);
	}

	bool PaoDeKuaiRule::straightPairExcluded(const PokerCard& c) const {
		return straightExcluded(c);
	}

	bool PaoDeKuaiRule::straightTripleExcluded(const PokerCard& c) const {
		return straightExcluded(c);
	}

	bool PaoDeKuaiRule::butterflyExcluded(const PokerCard& c) const {
		return straightExcluded(c);
	}

	bool PaoDeKuaiRule::straight(const CardArray& cards) const {
		int n = static_cast<int>(cards.size());
		if (n < 5)
			return false;
		// 2和王不能出现在顺子中
		for (auto& c : cards) {
			if (straightExcluded(c))
				return false;
		}
		return PokerRule::straight(cards);
	}

	bool PaoDeKuaiRule::straightPair(const CardArray& cards) const {
		int n = static_cast<int>(cards.size());
		if (n < 4 || n % 2 != 0)
			return false;
		for (auto& c : cards) {
			if (straightPairExcluded(c))
				return false;
		}
		return PokerRule::straightPair(cards);
	}

	void PaoDeKuaiRule::loadConfig(const std::string& ruleConfig) {
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

		if (root.isMember("card_count") && root["card_count"].isInt())
			_cardCount = root["card_count"].asInt();
		if (root.isMember("player_count") && root["player_count"].isInt())
			_playerCount = root["player_count"].asInt();
		if (root.isMember("base_score") && root["base_score"].isInt())
			_baseScore = root["base_score"].asInt();
		if (root.isMember("round_count") && root["round_count"].isInt())
			_roundCount = root["round_count"].asInt();
		if (root.isMember("bomb_double") && root["bomb_double"].isBool())
			_bombDouble = root["bomb_double"].asBool();
		if (root.isMember("bomb_score") && root["bomb_score"].isInt())
			_bombScore = root["bomb_score"].asInt();
		if (root.isMember("allow_pass") && root["allow_pass"].isBool())
			_allowPass = root["allow_pass"].asBool();
		if (root.isMember("auto_play_timeout") && root["auto_play_timeout"].isInt())
			_autoPlayTimeout = root["auto_play_timeout"].asInt();
		if (root.isMember("max_round_score") && root["max_round_score"].isInt())
			_maxRoundScore = root["max_round_score"].asInt();
		if (root.isMember("max_score") && root["max_score"].isInt())
			_maxRoundScore = root["max_score"].asInt();
		if (root.isMember("must_include_spade3") && root["must_include_spade3"].isBool())
			_mustIncludeSpade3 = root["must_include_spade3"].asBool();
		if (root.isMember("spring_double") && root["spring_double"].isBool())
			_springDouble = root["spring_double"].asBool();
		if (root.isMember("force_play_if_can_beat") && root["force_play_if_can_beat"].isBool())
			_forcePlayIfCanBeat = root["force_play_if_can_beat"].asBool();

		_playerCount = 2;
		_cardCount = 15;
		_mustIncludeSpade3 = false;
		if (_baseScore < 1)
			_baseScore = 1;
		if (_roundCount < 0)
			_roundCount = 0;
		if (_autoPlayTimeout < 5000)
			_autoPlayTimeout = 5000;
	}
}
