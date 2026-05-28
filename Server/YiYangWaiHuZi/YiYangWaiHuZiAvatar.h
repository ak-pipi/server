// YiYangWaiHuZiAvatar.h
// 益阳歪胡子玩家替身

#ifndef _NIU_MA_YIYANG_WAIHUZI_AVATAR_H_
#define _NIU_MA_YIYANG_WAIHUZI_AVATAR_H_

#include "Game/GameAvatar.h"
#include "YiYangWaiHuZiCard.h"

#include <vector>

namespace NiuMa
{
	/**
	 * 歪胡子牌组合
	 */
	struct WaiHuZiCombination
	{
		int type;				// WaiHuZiCombType
		int huXi;				// 胡息数
		std::vector<int> cards;	// 牌ID列表

		WaiHuZiCombination() : type(0), huXi(0) {}
	};

	class YiYangWaiHuZiAvatar : public GameAvatar
	{
	public:
		YiYangWaiHuZiAvatar(const std::string& playerId, int seat, bool robot);
		virtual ~YiYangWaiHuZiAvatar();

		void clear();

	public:
		// 手牌操作
		void setHandCards(const WaiHuZiCardArray& cards);
		const WaiHuZiCardArray& getHandCards() const { return _handCards; }
		void addCard(const WaiHuZiCard& card);
		void removeCardById(int id);
		void removeCardsByIds(const std::vector<int>& ids);
		bool hasCard(int id) const;
		int getHandCardCount() const { return static_cast<int>(_handCards.size()); }

		// 吃/碰/偎/跑/提 的牌组
		void addExposedComb(const WaiHuZiCombination& comb);
		const std::vector<WaiHuZiCombination>& getExposedCombs() const { return _exposedCombs; }

		// 桌面上最后打出的牌
		void setLastDiscard(const WaiHuZiCard& c) { _lastDiscard = c; }
		const WaiHuZiCard& getLastDiscard() const { return _lastDiscard; }

		// 设置/获取是否准备
		void setReady(bool r) { _ready = r; }
		bool isReady() const { return _ready; }

		// 得分
		void setRoundScore(int s) { _roundScore = s; }
		int getRoundScore() const { return _roundScore; }
		void setWinGold(double g) { _winGold = g; }
		double getWinGold() const { return _winGold; }

		// 胡息
		void setTotalHuXi(int h) { _totalHuXi = h; }
		int getTotalHuXi() const { return _totalHuXi; }

		// 是否已胡
		void setHu(bool h) { _hu = h; }
		bool isHu() const { return _hu; }

		// 是否已出牌
		void setDiscarded(bool d) { _discarded = d; }
		bool hasDiscarded() const { return _discarded; }

		// 计算手牌胡息
		int calcHuXi() const;

	private:
		// 手牌
		WaiHuZiCardArray _handCards;

		// 吃/碰/偎/跑/提 组合
		std::vector<WaiHuZiCombination> _exposedCombs;

		// 最后打出的牌
		WaiHuZiCard _lastDiscard;

		// 状态
		bool _ready;
		bool _hu;
		bool _discarded;

		// 得分
		int _roundScore;
		double _winGold;

		// 胡息
		int _totalHuXi;
	};
}

#endif // _NIU_MA_YIYANG_WAIHUZI_AVATAR_H_
