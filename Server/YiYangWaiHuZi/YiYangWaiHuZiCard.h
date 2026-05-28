// YiYangWaiHuZiCard.h
// 益阳歪胡子牌面定义

#ifndef _NIU_MA_YIYANG_WAIHUZI_CARD_H_
#define _NIU_MA_YIYANG_WAIHUZI_CARD_H_

#include <vector>
#include <string>

#include "msgpack/msgpack.hpp"

namespace NiuMa
{
	/**
	 * 歪胡子字牌类型
	 * 小字牌：一二三四五六七八九十（各4张，共40张）
	 * 大字牌：壹贰叁肆伍陆柒捌玖拾（各4张，共40张）
	 * 总计80张牌
	 */
	enum class WaiHuZiPoint : int
	{
		Invalid = 0,
		Yi = 1, Er, San, Si, Wu, Liu, Qi, Ba, Jiu, Shi,	// 小字1-10
		DaYi = 11, DaEr, DaSan, DaSi, DaWu, DaLiu, DaQi, DaBa, DaJiu, DaShi // 大字11-20
	};

	/**
	 * 歪胡子单张牌
	 */
	class WaiHuZiCard
	{
	public:
		WaiHuZiCard(int point_ = 0, int id_ = -1);
		virtual ~WaiHuZiCard();

		int getPoint() const { return point; }
		void setPoint(int p) { point = p; }
		int getId() const { return id; }
		void setId(int i) { id = i; }
		bool isValid() const { return point > 0; }
		bool isSmall() const { return point >= 1 && point <= 10; }
		bool isBig() const { return point >= 11 && point <= 20; }
		int getFaceValue() const { return isBig() ? point - 10 : point; }
		bool sameFace(const WaiHuZiCard& c) const { return getFaceValue() == c.getFaceValue(); }

		WaiHuZiCard& operator=(const WaiHuZiCard& c);
		bool operator==(const WaiHuZiCard& c) const;

	private:
		int point;
		int id;

	public:
		MSGPACK_DEFINE_MAP(point, id);
	};

	typedef std::vector<WaiHuZiCard> WaiHuZiCardArray;

	/**
	 * 歪胡子操作类型
	 */
	enum class WaiHuZiAction : int
	{
		None = 0,
		Chi = 1,	// 吃牌
		Peng = 2,	// 碰牌
		Wei = 3,	// 偎牌（手中有两张，摸到第三张）
		Pao = 4,	// 跑牌（已有碰/偎后摸到第四张）
		Ti = 5,		// 提牌（手中有三张，摸到第四张）
		Hu = 6,		// 胡牌
		Pass = 7,	// 过
		Discard = 8	// 出牌
	};

	/**
	 * 胡息计算中的牌组合
	 */
	enum class WaiHuZiCombType : int
	{
		Invalid = 0,
		ChiShun = 1,	// 吃顺子（同大小连续，如一二三）
		Peng = 2,		// 碰（三张相同小字=1息，大字=3息）
		Wei = 3,		// 偎（三张相同小字=3息，大字=6息）
		Pao = 4,		// 跑（四张相同小字=6息，大字=9息）
		Ti = 5,			// 提（四张相同小字=9息，大字=12息）
		Kan = 6,		// 坎（手中三张相同小字=3息，大字=6息）
		Shun = 7,		// 顺子（同字大小混合连续，如一贰三）
		Dui = 8,		// 对子（胡牌时不能单独存在）
		ErQiShi = 9,	// 二七十（特殊组合，同大小的小字/大字2/7/10）
		YiErSan = 10	// 一二三（特殊组合，同大小的小字/大字1/2/3）
	};
}

#endif // _NIU_MA_YIYANG_WAIHUZI_CARD_H_
