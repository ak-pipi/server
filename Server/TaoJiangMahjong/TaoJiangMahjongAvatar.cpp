// TaoJiangMahjongAvatar.cpp

#include "TaoJiangMahjongAvatar.h"
#include "MahjongRule.h"
#include "MahjongDealer.h"

#include <algorithm>
#include <map>

namespace NiuMa
{
	TaoJiangMahjongAvatar::TaoJiangMahjongAvatar(const std::string& playerId, int seat, bool bRobot)
		: MahjongAvatar(playerId, seat, bRobot)
		, _winGold(0.0)
		, _baoTinged(false)
		, _yingZhuang(false)
		, _laiZi(MahjongTile::Tile())
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
		_yingZhuang = false;
	}

	void TaoJiangMahjongAvatar::setBaoTinged(bool b) {
		_baoTinged = b;
	}

	bool TaoJiangMahjongAvatar::isBaoTinged() const {
		return _baoTinged;
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
		MahjongGenre::TingPaiArray::const_iterator it = _tingTiles.begin();
		while (it != _tingTiles.end()) {
			if (mt.isSame(it->tile)) {
				_huStyle = it->style;
				bFound = true;
				break;
			}
			++it;
		}
		if (!bFound)
			return false;

		// 构建完整牌面列表（手牌 + 碰杠牌 + 点炮牌）
		MahjongTileArray lstAll;
		lstAll.reserve(_handTiles.size() + _chapters.size() * 4 + 1);
		lstAll = _handTiles;
		if (!bZiMo)
			lstAll.push_back(mt);
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
			if (pengPengHu && needWild <= laiziCount) {
				_huStyle |= static_cast<int>(MahjongGenre::HuStyle::PengPengHu);
			}
			} // if (!hasChi)
		}

		// === 检测将将胡 ===
		// 所有非赖子牌的数字必须是2、5、8
		{
			bool jiangJiangHu = true;
			for (const auto& t : lstNormal) {
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
			if (normalRemain == 0 && totalPairs >= 7) {
				_huStyle |= static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui);
				// 豪七对：含4张相同牌（不考虑赖子替代，仅检查普通牌）
				for (const auto& kv : tileCounts) {
					if (kv.second >= 4) {
						_huStyle |= static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui3);
						break;
					}
				}
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

		// 检测七小对（包括豪七对）
		bool isQiXiaoDui = false;
		bool isHaoQiDui = false; // 豪七对（含4张相同牌）
		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui)) {
			isQiXiaoDui = true;
			// 豪七对：含4张相同牌的七小对
			if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui1)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui1) ||
				(_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui2)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui2) ||
				(_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui3)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui3)) {
				isHaoQiDui = true;
			}
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
		if (isQiXiaoDui && !isHaoQiDui) {
			// 七小对 = 1个大胡
			daHuCount += 1;
		}
		if (isHaoQiDui) {
			// 豪七对 = 2个大胡
			daHuCount += 2;
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

		// 计算得分：底分8 × 大胡倍率(3^daHuCount)
		// 公式：8底分 × 3^daHuCount
		int score = 8;
		for (int i = 0; i < daHuCount; i++)
			score *= 3;

		// 硬庄额外 x2
		if (_yingZhuang)
			score *= 2;

		// 地胡：按大胡计分（已通过上面的逻辑处理）
		// 地胡本身算一个大胡
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::DiHu)) == static_cast<int>(MahjongGenre::HuWay::DiHu)) {
			// 地胡已在基类检测中处理
			// 地胡=拥有三张赖子本身牌，摸完牌即可胡
		}

		return score;
	}

	int TaoJiangMahjongAvatar::getDaHuCount() const {
		int daHuCount = 0;

		bool isQiXiaoDui = false;
		bool isHaoQiDui = false;
		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui)) {
			isQiXiaoDui = true;
			if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui1)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui1) ||
				(_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui2)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui2) ||
				(_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui3)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui3)) {
				isHaoQiDui = true;
			}
		}

		if (isQiXiaoDui && !isHaoQiDui) daHuCount += 1;
		if (isHaoQiDui) daHuCount += 2;
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

		if (_baoTinged)
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

		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui3)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui3) ||
			(_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui2)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui2) ||
			(_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui1)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui1))
			name += "豪七对+";
		else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui))
			name += "七小队+";

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
}
