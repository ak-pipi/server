// PaoDeKuaiAvatar.cpp

#include "PaoDeKuaiAvatar.h"
#include "PokerCombination.h"

namespace NiuMa
{
	PaoDeKuaiAvatar::PaoDeKuaiAvatar(const PokerRule::Ptr& rule, const std::string& playerId, int seat, bool robot)
		: PokerAvatar(rule, playerId, seat, robot)
		, _roundScore(0)
		, _totalScore(0)
		, _winGold(0)
		, _ready(false)
		, _trusteeship(false)
	{
	}

	PaoDeKuaiAvatar::~PaoDeKuaiAvatar() {}

	void PaoDeKuaiAvatar::clear() {
		PokerAvatar::clear();
		_roundScore = 0;
		_winGold = 0;
		_ready = false;
		_trusteeship = false;
	}

	void PaoDeKuaiAvatar::combineAllGenres() {
		// 遍历所有牌值，生成单张、对子、三条、炸弹组合
		int pointNums = _rule->getPointNums();
		for (int i = 0; i < pointNums; i++) {
			combineSinglePair(i);
		}

		// 生成顺子组合
		combineStraight();

		// 生成连对组合
		combineStraightPair();

		// 生成三带一、三带二组合
		combineTriple();

		// 生成飞机组合
		combinePlane();

		// 王炸组合
		if (!_jokerCards[0].empty() && !_jokerCards[1].empty()) {
			PokerCombination::Ptr comb = allocateCombination();
			comb->genre = static_cast<int>(PaoDeKuaiGenre::Rocket);
			std::vector<int> ids;
			ids.push_back(_jokerCards[0][0]);
			ids.push_back(_jokerCards[1][0]);
			comb->cardIds = ids;
			insertCombination(comb);
		}
	}

	void PaoDeKuaiAvatar::combineSinglePair(int order) {
		int nums = _pointOrderNums[order];
		if (nums <= 0)
			return;

		const std::vector<int>& cardIds = _pointOrderCards[order];

		// 单张
		for (int i = 0; i < nums; i++) {
			PokerCombination::Ptr comb = allocateCombination();
			comb->genre = static_cast<int>(PaoDeKuaiGenre::Single);
			comb->officerOrder = order;
			std::vector<int> ids;
			ids.push_back(cardIds[i]);
			comb->cardIds = ids;
			insertCombination(comb);
		}

		// 对子
		if (nums >= 2) {
			PokerCombination::Ptr comb = allocateCombination();
			comb->genre = static_cast<int>(PaoDeKuaiGenre::Pair);
			comb->officerOrder = order;
			std::vector<int> ids;
			ids.push_back(cardIds[nums - 2]);
			ids.push_back(cardIds[nums - 1]);
			comb->cardIds = ids;
			insertCombination(comb);
		}

		// 三条
		if (nums >= 3) {
			PokerCombination::Ptr comb = allocateCombination();
			comb->genre = static_cast<int>(PaoDeKuaiGenre::Triple);
			comb->officerOrder = order;
			std::vector<int> ids;
			for (int i = nums - 3; i < nums; i++)
				ids.push_back(cardIds[i]);
			comb->cardIds = ids;
			insertCombination(comb);
		}

		// 炸弹
		if (nums >= 4) {
			PokerCombination::Ptr comb = allocateCombination();
			comb->genre = static_cast<int>(PaoDeKuaiGenre::Bomb);
			comb->officerOrder = order;
			std::vector<int> ids;
			for (int i = nums - 4; i < nums; i++)
				ids.push_back(cardIds[i]);
			comb->cardIds = ids;
			insertCombination(comb);
		}
	}

	void PaoDeKuaiAvatar::combineStraight() {
		int pointNums = _rule->getPointNums();
		// 2和王者不参与顺子，所以最大到A(order=11)
		int maxOrder = 11; // A
		for (int start = 0; start <= maxOrder; start++) {
			for (int len = 5; len <= maxOrder - start + 1; len++) {
				if (start + len - 1 > maxOrder)
					break;
				bool ok = true;
				std::vector<int> ids;
				for (int j = 0; j < len; j++) {
					int o = start + j;
					if (_pointOrderNums[o] <= 0) {
						ok = false;
						break;
					}
					ids.push_back(_pointOrderCards[o][0]);
				}
				if (ok) {
					PokerCombination::Ptr comb = allocateCombination();
					comb->genre = static_cast<int>(PaoDeKuaiGenre::Straight);
					comb->officerOrder = start + len - 1;
					comb->cardIds = ids;
					insertCombination(comb);
				}
			}
		}
	}

	void PaoDeKuaiAvatar::combineStraightPair() {
		int maxOrder = 11;
		for (int start = 0; start <= maxOrder; start++) {
			for (int len = 3; len <= maxOrder - start + 1; len++) {
				if (start + len - 1 > maxOrder)
					break;
				bool ok = true;
				std::vector<int> ids;
				for (int j = 0; j < len; j++) {
					int o = start + j;
					if (_pointOrderNums[o] < 2) {
						ok = false;
						break;
					}
					ids.push_back(_pointOrderCards[o][_pointOrderNums[o] - 2]);
					ids.push_back(_pointOrderCards[o][_pointOrderNums[o] - 1]);
				}
				if (ok) {
					PokerCombination::Ptr comb = allocateCombination();
					comb->genre = static_cast<int>(PaoDeKuaiGenre::StraightPair);
					comb->officerOrder = start + len - 1;
					comb->cardIds = ids;
					insertCombination(comb);
				}
			}
		}
	}

	void PaoDeKuaiAvatar::combineTriple() {
		int pointNums = _rule->getPointNums();
		int maxOrder = 11;
		for (int o = 0; o < pointNums; o++) {
			if (_pointOrderNums[o] < 3)
				continue;
			std::vector<int> tripleIds;
			int n = _pointOrderNums[o];
			for (int i = n - 3; i < n; i++)
				tripleIds.push_back(_pointOrderCards[o][i]);

			// 三带一
			for (int o2 = 0; o2 < pointNums; o2++) {
				if (o2 == o || _pointOrderNums[o2] <= 0)
					continue;
				PokerCombination::Ptr comb = allocateCombination();
				comb->genre = static_cast<int>(PaoDeKuaiGenre::TripleOne);
				comb->officerOrder = o;
				std::vector<int> ids = tripleIds;
				ids.push_back(_pointOrderCards[o2][0]);
				comb->cardIds = ids;
				insertCombination(comb);
			}

			// 三带二
			for (int o2 = 0; o2 < pointNums; o2++) {
				if (o2 == o || _pointOrderNums[o2] < 2)
					continue;
				PokerCombination::Ptr comb = allocateCombination();
				comb->genre = static_cast<int>(PaoDeKuaiGenre::TriplePair);
				comb->officerOrder = o;
				std::vector<int> ids = tripleIds;
				int n2 = _pointOrderNums[o2];
				ids.push_back(_pointOrderCards[o2][n2 - 2]);
				ids.push_back(_pointOrderCards[o2][n2 - 1]);
				comb->cardIds = ids;
				insertCombination(comb);
			}
		}
	}

	void PaoDeKuaiAvatar::combinePlane() {
		int maxOrder = 11;
		// 找出所有三条
		std::vector<int> tripleOrders;
		int pointNums = _rule->getPointNums();
		for (int o = 0; o < pointNums; o++) {
			if (_pointOrderNums[o] >= 3 && o <= maxOrder)
				tripleOrders.push_back(o);
		}
		if (tripleOrders.size() < 2)
			return;

		// 连续三条序列
		for (size_t i = 0; i < tripleOrders.size(); i++) {
			for (size_t len = 2; i + len <= tripleOrders.size(); len++) {
				// 检查是否连续
				bool continuous = true;
				for (size_t j = 1; j < len; j++) {
					if (tripleOrders[i + j] != tripleOrders[i] + static_cast<int>(j)) {
						continuous = false;
						break;
					}
				}
				if (!continuous)
					break;

				int planeLen = static_cast<int>(len);
				std::vector<int> tripleIds;
				for (size_t j = 0; j < len; j++) {
					int o = tripleOrders[i + j];
					int n = _pointOrderNums[o];
					for (int k = n - 3; k < n; k++)
						tripleIds.push_back(_pointOrderCards[o][k]);
				}

				// 飞机不带
				{
					PokerCombination::Ptr comb = allocateCombination();
					comb->genre = static_cast<int>(PaoDeKuaiGenre::Plane);
					comb->officerOrder = tripleOrders[i + len - 1];
					comb->cardIds = tripleIds;
					insertCombination(comb);
				}

				// 飞机带单
				{
					PokerCombination::Ptr comb = allocateCombination();
					comb->genre = static_cast<int>(PaoDeKuaiGenre::PlaneOne);
					comb->officerOrder = tripleOrders[i + len - 1];
					comb->cardIds = tripleIds; // 简化：不带副牌的组合也让其能出
					insertCombination(comb);
				}

				// 飞机带对
				{
					PokerCombination::Ptr comb = allocateCombination();
					comb->genre = static_cast<int>(PaoDeKuaiGenre::PlanePair);
					comb->officerOrder = tripleOrders[i + len - 1];
					comb->cardIds = tripleIds;
					insertCombination(comb);
				}
			}
		}
	}

	void PaoDeKuaiAvatar::candidateCombinationsImpl(int situation) {
		// 首位出牌：出最小牌型的组合
		for (auto& kv : _combinations) {
			for (auto& comb : kv.second) {
				_candidates.push_back(comb);
			}
		}
	}

	void PaoDeKuaiAvatar::candidateCombinationsImpl(const PokerGenre& pg, int situation) {
		// 压牌：找出所有能大过当前牌型的组合
		int targetGenre = pg.getGenre();
		for (auto& kv : _combinations) {
			for (auto& comb : kv.second) {
				if (comb->genre == targetGenre ||
					comb->genre == static_cast<int>(PaoDeKuaiGenre::Bomb) ||
					comb->genre == static_cast<int>(PaoDeKuaiGenre::Rocket)) {
					_candidates.push_back(comb);
				}
			}
		}
	}
}
