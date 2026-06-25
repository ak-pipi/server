// TaoJiangMahjongAvatar.cpp

#include "TaoJiangMahjongAvatar.h"
#include "MahjongRule.h"
#include "MahjongDealer.h"

#include <algorithm>

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
		// 调用基类检测基本胡牌样式（平胡、碰碰胡、七小对、清一色等）
		if (!MahjongAvatar::detectHuStyle(bZiMo, mt))
			return false;

		// 桃江麻将扩展：检测将将胡（所有牌都是2、5、8的牌）
		MahjongTileArray lstTemp;
		lstTemp.reserve(_handTiles.size() + _chapters.size() * 4);
		lstTemp = _handTiles;
		if (!bZiMo)
			lstTemp.push_back(mt);
		MahjongChapterArray::const_iterator it1 = _chapters.begin();
		while (it1 != _chapters.end()) {
			const MahjongTileArray& lstTiles = it1->getAllTiles();
			lstTemp.insert(lstTemp.end(), lstTiles.begin(), lstTiles.end());
			++it1;
		}
		std::sort(lstTemp.begin(), lstTemp.end());

		// 检测将将胡：所有牌的数字必须是2、5、8
		// 赖子也算2/5/8（赖子做万能牌时可当任意2/5/8使用）
		bool jiangJiangHu = true;
		MahjongTileArray::const_iterator it = lstTemp.begin();
		while (it != lstTemp.end()) {
			// 赖子牌视为2/5/8
			if (_laiZi.isValid() && it->getTile() == _laiZi) {
				++it;
				continue;
			}
			MahjongTile::Number num = it->getNumber();
			if (num != MahjongTile::Number::Er &&
				num != MahjongTile::Number::Wu &&
				num != MahjongTile::Number::Ba) {
				jiangJiangHu = false;
				break;
			}
			++it;
		}
		if (jiangJiangHu)
			_huStyle |= static_cast<int>(MahjongGenre::HuStyle::JiangJiangHu);

		// 注意：黑天胡检测在calcHuScore中进行（需要赖子信息）
		// 豪七对通过基类的QiXiaoDui1/QiXiaoDui2/QiXiaoDui3已覆盖

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
