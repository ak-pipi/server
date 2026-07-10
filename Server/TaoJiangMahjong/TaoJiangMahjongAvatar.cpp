// TaoJiangMahjongAvatar.cpp

#include "TaoJiangMahjongAvatar.h"
#include "MahjongRule.h"
#include "MahjongDealer.h"

#include <algorithm>
#include <map>

namespace NiuMa
{
	namespace
	{
		bool isSequenceTile(const MahjongTile::Tile& tile, MahjongTile::Pattern pat, int num) {
			return tile.getPattern() == pat && static_cast<int>(tile.getNumber()) == num;
		}

		bool canAssignToSequence(const MahjongTileArray& tiles, const MahjongTile::Tile& laiZi,
			MahjongTile::Pattern pat, int start) {
			bool used[3] = { false, false, false };
			int wild = 0;
			for (const MahjongTile& mt : tiles) {
				if (laiZi.isValid() && mt.getTile() == laiZi) {
					wild++;
					continue;
				}
				bool matched = false;
				for (int i = 0; i < 3; i++) {
					if (!used[i] && isSequenceTile(mt.getTile(), pat, start + i)) {
						used[i] = true;
						matched = true;
						break;
					}
				}
				if (!matched)
					return false;
			}
			int missing = 0;
			for (int i = 0; i < 3; i++) {
				if (!used[i])
					missing++;
			}
			return wild >= missing;
		}

		bool canFormPengWithWildcards(const MahjongTileArray& tiles, const MahjongTile::Tile& laiZi) {
			for (int pi = 0; pi < 3; pi++) {
				MahjongTile::Pattern pat = static_cast<MahjongTile::Pattern>(
					static_cast<int>(MahjongTile::Pattern::Tong) + pi);
				for (int ni = 1; ni <= 9; ni++) {
					MahjongTile::Tile target(pat, static_cast<MahjongTile::Number>(ni));
					bool ok = true;
					for (const MahjongTile& mt : tiles) {
						if (laiZi.isValid() && mt.getTile() == laiZi)
							continue;
						if (mt.getTile() != target) {
							ok = false;
							break;
						}
					}
					if (ok)
						return true;
				}
			}
			return false;
		}
	}

	TaoJiangMahjongAvatar::TaoJiangMahjongAvatar(const std::string& playerId, int seat, bool bRobot)
		: MahjongAvatar(playerId, seat, bRobot)
		, _winGold(0.0)
		, _baoTinged(false)
		, _afterGang(false)
		, _yingZhuang(false)
		, _laiZi(MahjongTile::Tile())
		, _mingZi(MahjongTile::Tile())
	{
		for (int i = 0; i < 4; i++)
			_loseScores[i] = 0;
	}

	TaoJiangMahjongAvatar::~TaoJiangMahjongAvatar() {}

	void TaoJiangMahjongAvatar::clear() {
		MahjongAvatar::clear();

		for (int i = 0; i < 4; i++)
			_loseScores[i] = 0;
		_winGold = 0.0;
		_baoTinged = false;
		_baoTingTiles.clear();
		_afterGang = false;
		_yingZhuang = false;
	}

	void TaoJiangMahjongAvatar::setBaoTinged(bool b) {
		_baoTinged = b;
		if (b)
			_baoTingTiles = _tingTiles;
		else
			_baoTingTiles.clear();
	}

	bool TaoJiangMahjongAvatar::isBaoTinged() const {
		return _baoTinged;
	}

	const MahjongGenre::TingPaiArray& TaoJiangMahjongAvatar::getBaoTingTiles() const {
		return _baoTingTiles;
	}

	void TaoJiangMahjongAvatar::setAfterGang(bool v) {
		_afterGang = v;
	}

	bool TaoJiangMahjongAvatar::isAfterGang() const {
		return _afterGang;
	}

	void TaoJiangMahjongAvatar::setYingZhuang(bool b) {
		_yingZhuang = b;
	}

	bool TaoJiangMahjongAvatar::isYingZhuang() const {
		return _yingZhuang;
	}

	void TaoJiangMahjongAvatar::addHuStyle(MahjongGenre::HuStyle eStyle) {
		_huStyle |= static_cast<int>(eStyle);
	}

	bool TaoJiangMahjongAvatar::detectHuStyle(bool bZiMo, const MahjongTile& mt) {
		// 先从_tingTiles中获取基础胡牌样式（平胡）
		_huStyle = 0;
		bool bFound = false;
		const MahjongGenre::TingPaiArray& huTiles = _baoTinged ? _baoTingTiles : _tingTiles;
		MahjongGenre::TingPaiArray::const_iterator it = huTiles.begin();
		while (it != huTiles.end()) {
			if (mt.isSame(it->tile)) {
				_huStyle = it->style;
				bFound = true;
				break;
			}
			++it;
		}
		if (!bFound) {
			if (bZiMo && _mingZi.isValid()) {
				int mingZiCount = 0;
				for (const MahjongTile& t : _handTiles) {
					if (t.getTile() == _mingZi)
						mingZiCount++;
				}
				if (mingZiCount >= 3)
					bFound = true;
			}
		}
		if (!bFound)
			return false;

		// 构建完整牌面列表（手牌 + 碰杠牌 + 点炮牌）
		MahjongTileArray lstAll;
		lstAll.reserve(_handTiles.size() + _chapters.size() * 4 + 1);
		lstAll = _handTiles;
		if (!bZiMo) {
			lstAll.push_back(mt);
		} else if (mt.isValid()) {
			bool inHand = false;
			for (const MahjongTile& handTile : _handTiles) {
				if (handTile.getId() == mt.getId()) {
					inHand = true;
					break;
				}
			}
			if (!inHand)
				lstAll.push_back(mt);
		}
		MahjongChapterArray::const_iterator it1 = _chapters.begin();
		while (it1 != _chapters.end()) {
			const MahjongTileArray& lstTiles = it1->getAllTiles();
			lstAll.insert(lstAll.end(), lstTiles.begin(), lstTiles.end());
			++it1;
		}
		std::sort(lstAll.begin(), lstAll.end());

		// 统计赖子数量
		int laiziCount = 0;
		if (_laiZi.isValid()) {
			for (const auto& t : lstAll) {
				if (t.getTile() == _laiZi)
					laiziCount++;
			}
		}

		// 构建不含赖子的牌面列表（赖子做万能牌，可当任意牌）
		MahjongTileArray lstNormal;
		for (const auto& t : lstAll) {
			if (!(_laiZi.isValid() && t.getTile() == _laiZi))
				lstNormal.push_back(t);
		}
		std::sort(lstNormal.begin(), lstNormal.end());

		// === 检测清一色（赖子可当任意花色） ===
		// 不含赖子的普通牌必须全部是同一花色
		if (lstNormal.size() > 0) {
			MahjongTile::Pattern firstPat = lstNormal[0].getPattern();
			bool qingYiSe = true;
			for (const auto& t : lstNormal) {
				if (t.getPattern() != firstPat) {
					qingYiSe = false;
					break;
				}
			}
			if (qingYiSe)
				_huStyle |= static_cast<int>(MahjongGenre::HuStyle::QingYiSe);

		} else if (laiziCount == lstAll.size()) {
			// 全部是赖子牌，也算清一色
			_huStyle |= static_cast<int>(MahjongGenre::HuStyle::QingYiSe);
		}

		// === 检测碰碰胡（赖子可补刻子） ===
		// 碰碰胡要求：所有牌面都是刻子(3张) + 最多1个对子(雀头)
		// 赖子可以补任意刻子或雀头
		// 注意：如果有吃的顺子（Chi章），则不可能是碰碰胡
		{
			bool hasChi = false;
			for (const auto& ch : _chapters) {
				if (ch.getType() == MahjongChapter::Type::Chi) {
					hasChi = true;
					break;
				}
			}
			if (!hasChi) {
			// 统计不含赖子的每种牌数量
			std::map<MahjongTile::Tile, int> tileCounts;
			for (const auto& t : lstNormal) {
				tileCounts[t.getTile()]++;
			}
			// 计算需要的万能牌数量来补成碰碰胡
			int needWild = 0;
			bool hasJiang = false;
			bool pengPengHu = true;
			for (const auto& kv : tileCounts) {
				int cnt = kv.second;
				if (cnt == 3) {
					// 刻子，OK
				} else if (cnt == 2) {
					// 可以是雀头（最多一组）
					if (hasJiang) {
						pengPengHu = false;
						break;
					}
					hasJiang = true;
				} else if (cnt == 1) {
					// 需要万能牌补成刻子(需2张)或雀头(需1张)
					// 但如果作为雀头，需要和另一张万能牌配对(需1张万能做该牌)
					// 最优：如果还没雀头，可以1张+1万能=对子
					if (!hasJiang && laiziCount >= 1) {
						// 1张普通牌 + 1张万能牌 = 对子(雀头)
						needWild += 1;
						hasJiang = true;
					} else {
						// 需要补成刻子：3-1=2张万能
						needWild += 2;
					}
				} else if (cnt == 4) {
					// 4张牌：可以拆成1个刻子+1个单牌，或用于七小对(不在碰碰胡范围)
					// 碰碰胡中4张不能直接处理（需要拆成刻子+剩余1张）
					// 但标准碰碰胡不含4张相同牌（那是豪七对或杠）
					// 4张牌：3张做刻子，剩余1张需要补
					if (!hasJiang && laiziCount >= 1) {
						needWild += 1;
						hasJiang = true;
					} else {
						needWild += 2;
					}
				}
			}
			// 如果没有雀头，需要万能牌做雀头(2张万能做一对)
			if (pengPengHu && !hasJiang && laiziCount >= 2) {
				needWild += 2;
			}
			bool pengPengHuResult = (pengPengHu && needWild <= laiziCount);
			if (pengPengHuResult) {
				_huStyle |= static_cast<int>(MahjongGenre::HuStyle::PengPengHu);
			}

			} // if (!hasChi)
		}

		// === 检测将将胡 ===
		// 所有非赖子牌的数字必须是2、5、8
		{
			bool jiangJiangHu = true;
			for (const auto& ch : _chapters) {
				if (ch.getType() == MahjongChapter::Type::Chi) {
					jiangJiangHu = false;
					break;
				}
			}
			for (const auto& t : lstNormal) {
				if (!jiangJiangHu)
					break;
				MahjongTile::Number num = t.getNumber();
				if (num != MahjongTile::Number::Er &&
					num != MahjongTile::Number::Wu &&
					num != MahjongTile::Number::Ba) {
					jiangJiangHu = false;
					break;
				}
			}
			if (jiangJiangHu)
				_huStyle |= static_cast<int>(MahjongGenre::HuStyle::JiangJiangHu);

		}

		// === 检测七小对（含赖子万能牌） ===
		if (_chapters.empty()) {
			// 门清无碰杠时才能是七小对
			std::map<MahjongTile::Tile, int> tileCounts;
			for (const auto& t : lstNormal) {
				tileCounts[t.getTile()]++;
			}
			int pairCount = 0;
			int singleCount = 0;
			for (const auto& kv : tileCounts) {
				pairCount += kv.second / 2;
				if (kv.second % 2 == 1)
					singleCount++;
			}
			int laiziPairs = laiziCount / 2;
			int laiziSingles = laiziCount % 2;
			int pairedWithWild = std::min(laiziSingles, singleCount);
			int wildRemain = laiziSingles - pairedWithWild;
			int normalRemain = singleCount - pairedWithWild;
			int totalPairs = pairCount + laiziPairs + pairedWithWild + wildRemain / 2;
			bool qiXiaoDuiResult = (normalRemain == 0 && totalPairs >= 7);
			if (qiXiaoDuiResult) {
				_huStyle |= static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui);
				// 豪七对/双豪七对：含4张相同牌（不考虑赖子替代，仅检查普通牌）
				int quadCount = 0;
				for (const auto& kv : tileCounts) {
					if (kv.second >= 4)
						quadCount++;
				}
				if (quadCount == 1)
					_huStyle |= static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui1);
				else if (quadCount == 2)
					_huStyle |= static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui2);
				else if (quadCount >= 3)
					_huStyle |= static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui3);
			}

		}

		return true;
	}

	int TaoJiangMahjongAvatar::calcHuScore() const {
		// 桃江麻将计分体系：
		// 底分为8分，大胡类型叠加
		// 每个大胡类型加 x3 倍
		// 基础：平胡自摸8分
		// 硬庄：无赖子参与，额外 x2
		// 报听：算一个大胡（加 x3）
		// 杠上花：算一个大胡（加 x3）
		// 天胡：算一个大胡（加 x3）
		// 天天胡：算两个大胡（加 x6）
		// 豪七对：算两个大胡（加 x6）

		int daHuCount = 0;
		bool isPingHu = false;

		// 基础平胡（4组顺子/刻子 + 1对雀头）
		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::PingHu)) == static_cast<int>(MahjongGenre::HuStyle::PingHu))
			isPingHu = true;

		// 检测七小对（包括豪七对/双豪七对）
		bool isQiXiaoDui = false;
		int haoQiDuiLevel = 0; // 豪七对等级: 0=无, 1=豪七对(1组4张), 2=双豪七对(2组4张), 3=三豪七对(3组4张)
		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui)) {
			isQiXiaoDui = true;
			if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui1)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui1))
				haoQiDuiLevel = 1;  // 1组4张 = 2大胡
			else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui2)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui2))
				haoQiDuiLevel = 2;  // 2组4张 = 3大胡
			else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui3)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui3))
				haoQiDuiLevel = 3;  // 3组4张 = 4大胡
		}

		// 检测碰碰胡
		bool isPengPengHu = false;
		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::PengPengHu)) == static_cast<int>(MahjongGenre::HuStyle::PengPengHu))
			isPengPengHu = true;

		// 检测将将胡
		bool isJiangJiangHu = false;
		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::JiangJiangHu)) == static_cast<int>(MahjongGenre::HuStyle::JiangJiangHu))
			isJiangJiangHu = true;

		// 检测黑天胡
		// 黑天胡由Room在doHu()中通过addHuStyle()设置标记
		// 条件：第一轮摸牌、无刻子、无顺子、将牌非2/5/8、无赖子做万能牌
		bool isHeiTianHu = false;
		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::HeiTianHu)) == static_cast<int>(MahjongGenre::HuStyle::HeiTianHu))
			isHeiTianHu = true;

		// 检测清一色
		bool isQingYiSe = false;
		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QingYiSe)) == static_cast<int>(MahjongGenre::HuStyle::QingYiSe))
			isQingYiSe = true;

		// 统计大胡数量
		if (isQiXiaoDui && haoQiDuiLevel == 0) {
			// 七小对 = 1个大胡
			daHuCount += 1;
		}
		if (haoQiDuiLevel == 1) {
			// 豪七对 = 2个大胡
			daHuCount += 2;
		}
		else if (haoQiDuiLevel == 2) {
			// 双豪七对 = 3个大胡
			daHuCount += 3;
		}
		else if (haoQiDuiLevel >= 3) {
			// 三豪七对 = 4个大胡
			daHuCount += 4;
		}
		if (isPengPengHu) {
			// 碰碰胡 = 1个大胡
			daHuCount += 1;
		}
		if (isJiangJiangHu) {
			// 将将胡 = 1个大胡
			daHuCount += 1;
		}
		if (isHeiTianHu) {
			// 黑天胡 = 1个大胡
			daHuCount += 1;
		}
		if (isQingYiSe) {
			// 清一色 = 1个大胡
			daHuCount += 1;
		}

		// 胡牌方式大胡
		bool isTianHu = false;
		bool isTianTianHu = false;
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::TianTianHu)) == static_cast<int>(MahjongGenre::HuWay::TianTianHu)) {
			// 天天胡 = 2个大胡
			isTianTianHu = true;
			daHuCount += 2;
		}
		else if ((_huWay & static_cast<int>(MahjongGenre::HuWay::TianHu)) == static_cast<int>(MahjongGenre::HuWay::TianHu)) {
			// 天胡 = 1个大胡
			isTianHu = true;
			daHuCount += 1;
		}

		// 杠上花 = 1个大胡
		bool isGangShangHua = false;
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::GangShangHua1)) == static_cast<int>(MahjongGenre::HuWay::GangShangHua1)) {
			isGangShangHua = true;
			daHuCount += 1;
		}

		// 报听 = 1个大胡
		if (_baoTinged)
			daHuCount += 1;

		// 硬庄作为一个大牌累计
		if (_yingZhuang)
			daHuCount += 1;

		// 地胡：拥有3张明子牌且能正常胡牌，算1个大胡
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::DiHu)) == static_cast<int>(MahjongGenre::HuWay::DiHu)) {
			daHuCount += 1;
			if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::PingHu)) == static_cast<int>(MahjongGenre::HuStyle::PingHu))
				daHuCount += 1;
		}

		int multiplier = (daHuCount > 0) ? (daHuCount * 3) : 1;
		int score = 8 * multiplier;

		return score;
	}

	int TaoJiangMahjongAvatar::getDaHuCount() const {
		int daHuCount = 0;

		bool isQiXiaoDui = false;
		int haoQiDuiLevel = 0;
		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui)) {
			isQiXiaoDui = true;
			if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui1)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui1))
				haoQiDuiLevel = 1;
			else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui2)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui2))
				haoQiDuiLevel = 2;
			else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui3)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui3))
				haoQiDuiLevel = 3;
		}

		if (isQiXiaoDui && haoQiDuiLevel == 0) daHuCount += 1;
		if (haoQiDuiLevel == 1) daHuCount += 2;
		else if (haoQiDuiLevel == 2) daHuCount += 3;
		else if (haoQiDuiLevel >= 3) daHuCount += 4;
		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::PengPengHu)) == static_cast<int>(MahjongGenre::HuStyle::PengPengHu)) daHuCount += 1;
		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::JiangJiangHu)) == static_cast<int>(MahjongGenre::HuStyle::JiangJiangHu)) daHuCount += 1;
		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::HeiTianHu)) == static_cast<int>(MahjongGenre::HuStyle::HeiTianHu)) daHuCount += 1;
		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QingYiSe)) == static_cast<int>(MahjongGenre::HuStyle::QingYiSe)) daHuCount += 1;

		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::TianTianHu)) == static_cast<int>(MahjongGenre::HuWay::TianTianHu))
			daHuCount += 2;
		else if ((_huWay & static_cast<int>(MahjongGenre::HuWay::TianHu)) == static_cast<int>(MahjongGenre::HuWay::TianHu))
			daHuCount += 1;

		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::GangShangHua1)) == static_cast<int>(MahjongGenre::HuWay::GangShangHua1))
			daHuCount += 1;

		// 地胡算1个大胡
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::DiHu)) == static_cast<int>(MahjongGenre::HuWay::DiHu)) {
			daHuCount += 1;
			if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::PingHu)) == static_cast<int>(MahjongGenre::HuStyle::PingHu))
				daHuCount += 1;
		}

		if (_baoTinged)
			daHuCount += 1;

		if (_yingZhuang)
			daHuCount += 1;

		return daHuCount;
	}

	std::string TaoJiangMahjongAvatar::getHuTypeName() const {
		std::string name;

		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::TianTianHu)) == static_cast<int>(MahjongGenre::HuWay::TianTianHu))
			name += "天天胡+";
		else if ((_huWay & static_cast<int>(MahjongGenre::HuWay::TianHu)) == static_cast<int>(MahjongGenre::HuWay::TianHu))
			name += "天胡+";

		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::DiHu)) == static_cast<int>(MahjongGenre::HuWay::DiHu))
			name += "地胡+";

		if (_baoTinged)
			name += "报听+";

		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::GangShangHua1)) == static_cast<int>(MahjongGenre::HuWay::GangShangHua1))
			name += "杠上花+";

		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QingYiSe)) == static_cast<int>(MahjongGenre::HuStyle::QingYiSe))
			name += "清一色+";

		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::JiangJiangHu)) == static_cast<int>(MahjongGenre::HuStyle::JiangJiangHu))
			name += "将将胡+";

		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::HeiTianHu)) == static_cast<int>(MahjongGenre::HuStyle::HeiTianHu))
			name += "黑天胡+";

		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui3)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui3))
			name += "三豪七对+";
		else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui2)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui2))
			name += "双豪七对+";
		else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui1)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui1))
			name += "豪七对+";
		else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui))
			name += "七小对+";

		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::PengPengHu)) == static_cast<int>(MahjongGenre::HuStyle::PengPengHu))
			name += "碰碰胡+";

		if (name.empty() || name == "") {
			if (isZiMo())
				name = "平胡(自摸)";
			else
				name = "平胡(放炮)";
		} else {
			// 去掉末尾的"+"
			if (!name.empty() && name[name.size() - 1] == '+')
				name.resize(name.size() - 1);
		}

		if (_yingZhuang)
			name += "(硬庄)";

		return name;
	}

	void TaoJiangMahjongAvatar::getHuCalcState(int& style, int& styleEx, int& way, bool& yingZhuang) const {
		style = _huStyle;
		styleEx = _huStyleEx;
		way = _huWay;
		yingZhuang = _yingZhuang;
	}

	void TaoJiangMahjongAvatar::restoreHuCalcState(int style, int styleEx, int way, bool yingZhuang) {
		_huStyle = style;
		_huStyleEx = styleEx;
		_huWay = way;
		_yingZhuang = yingZhuang;
	}

	void TaoJiangMahjongAvatar::addLoseScore(int seat, int s) {
		if (seat < 0 || seat > 3)
			return;
		_loseScores[seat] += s;
	}

	void TaoJiangMahjongAvatar::getLoseScores(int loseScores[4]) const {
		for (int i = 0; i < 4; i++)
			loseScores[i] = _loseScores[i];
	}

	void TaoJiangMahjongAvatar::setWinGold(double g) {
		_winGold = g;
	}

	double TaoJiangMahjongAvatar::getWinGold() const {
		return _winGold;
	}

	void TaoJiangMahjongAvatar::setLaiZi(const MahjongTile::Tile& tile) {
		_laiZi = tile;
	}

	const MahjongTile::Tile& TaoJiangMahjongAvatar::getLaiZi() const {
		return _laiZi;
	}

	void TaoJiangMahjongAvatar::setMingZi(const MahjongTile::Tile& tile) {
		_mingZi = tile;
	}

	const MahjongTile::Tile& TaoJiangMahjongAvatar::getMingZi() const {
		return _mingZi;
	}

	bool TaoJiangMahjongAvatar::canZhiGang(const MahjongTile& mt) const {
		// 桃江麻将规则：没有听牌时不能直杠
		if (_tingTiles.empty())
			return false;
		return MahjongAvatar::canZhiGang(mt);
	}

	bool TaoJiangMahjongAvatar::canPeng(const MahjongTile& mt, std::string& passed) const {
		// 桃江麻将规则：开杠后不能碰
		if (_afterGang || _baoTinged)
			return false;
		passed.clear();
		for (size_t i = 0; i < _handTiles.size(); i++) {
			for (size_t j = i + 1; j < _handTiles.size(); j++) {
				MahjongTileArray testTiles;
				testTiles.push_back(mt);
				testTiles.push_back(_handTiles[i]);
				testTiles.push_back(_handTiles[j]);
				if (canFormPengWithWildcards(testTiles, _laiZi)) {
					return true;
				}
			}
		}
		return MahjongAvatar::canPeng(mt, passed);
	}

	bool TaoJiangMahjongAvatar::canChi(const MahjongTile& mt, std::vector<std::pair<int, int> >& lstPairs) const {
		// 桃江麻将规则：开杠后不能吃
		if (_afterGang || _baoTinged)
			return false;
		lstPairs.clear();
		for (size_t i = 0; i < _handTiles.size(); i++) {
			for (size_t j = i + 1; j < _handTiles.size(); j++) {
				MahjongTileArray tiles;
				tiles.push_back(mt);
				tiles.push_back(_handTiles[i]);
				tiles.push_back(_handTiles[j]);
				bool ok = false;
				for (int pi = 0; pi < 3 && !ok; pi++) {
					MahjongTile::Pattern pat = static_cast<MahjongTile::Pattern>(
						static_cast<int>(MahjongTile::Pattern::Tong) + pi);
					for (int start = 1; start <= 7; start++) {
						if (canAssignToSequence(tiles, _laiZi, pat, start)) {
							ok = true;
							break;
						}
					}
				}
				if (ok)
					lstPairs.push_back(std::make_pair(_handTiles[i].getId(), _handTiles[j].getId()));
			}
		}
		return !lstPairs.empty();
	}

	bool TaoJiangMahjongAvatar::canHu(const MahjongTile& mt) const {
		const MahjongGenre::TingPaiArray& huTiles = _baoTinged ? _baoTingTiles : _tingTiles;
		for (const MahjongGenre::TingPai& tp : huTiles) {
			if (mt.isSame(tp.tile))
				return true;
		}
		if (!_baoTinged && _mingZi.isValid()) {
			int count = 0;
			for (const MahjongTile& t : _handTiles) {
				if (t.getTile() == _mingZi)
					count++;
			}
			if (mt.getTile() == _mingZi)
				count++;
			if (count >= 3)
				return true;
		}
		return false;
	}

	bool TaoJiangMahjongAvatar::playTile(int id) {
		if (_baoTinged && id != getFetchedTileId())
			return false;
		return MahjongAvatar::playTile(id);
	}

	bool TaoJiangMahjongAvatar::doChi(const MahjongTile& mt, int id1, int id2, int actionId, int player) {
		MahjongTile mt1;
		MahjongTile mt2;
		mt1.setId(id1);
		mt2.setId(id2);
		if (!getTile(mt1) || !getTile(mt2))
			return false;

		MahjongTileArray lstTemp;
		lstTemp.push_back(mt);
		lstTemp.push_back(mt1);
		lstTemp.push_back(mt2);
		bool ok = false;
		for (int pi = 0; pi < 3 && !ok; pi++) {
			MahjongTile::Pattern pat = static_cast<MahjongTile::Pattern>(
				static_cast<int>(MahjongTile::Pattern::Tong) + pi);
			for (int start = 1; start <= 7; start++) {
				if (canAssignToSequence(lstTemp, _laiZi, pat, start)) {
					ok = true;
					break;
				}
			}
		}
		if (!ok)
			return false;

		removeTile(id1);
		removeTile(id2);
		std::sort(lstTemp.begin(), lstTemp.end());

		MahjongChapter mc;
		mc.addType(MahjongChapter::Type::Chi, actionId);
		mc.setTargetTile(mt.getId());
		mc.setTargetPlayer(player);
		mc.setAllTiles(lstTemp);
		_chapters.push_back(mc);
		clearPassedHu();

		return true;
	}

	bool TaoJiangMahjongAvatar::doPeng(const MahjongTile& mt, int actionId, int player) {
		MahjongTileArray lstTemp;
		lstTemp.reserve(3);
		std::vector<int> removeIds;
		for (size_t i = 0; i < _handTiles.size() && removeIds.empty(); i++) {
			for (size_t j = i + 1; j < _handTiles.size(); j++) {
				MahjongTileArray testTiles;
				testTiles.push_back(mt);
				testTiles.push_back(_handTiles[i]);
				testTiles.push_back(_handTiles[j]);
				if (canFormPengWithWildcards(testTiles, _laiZi)) {
					lstTemp.push_back(_handTiles[i]);
					lstTemp.push_back(_handTiles[j]);
					removeIds.push_back(_handTiles[i].getId());
					removeIds.push_back(_handTiles[j].getId());
					break;
				}
			}
		}
		lstTemp.push_back(mt);
		if (removeIds.size() < 2 || !canFormPengWithWildcards(lstTemp, _laiZi))
			return false;

		for (int id : removeIds)
			removeTile(id);

		std::sort(lstTemp.begin(), lstTemp.end());
		MahjongChapter mc;
		mc.addType(MahjongChapter::Type::Peng, actionId);
		mc.setTargetTile(mt.getId());
		mc.setTargetPlayer(player);
		mc.setAllTiles(lstTemp);
		_chapters.push_back(mc);
		clearPassedHu();

		return true;
	}
}
