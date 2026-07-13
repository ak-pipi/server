// DouDiZhuGameRule.h

#ifndef _NIU_MA_DOU_DI_ZHU_GAME_RULE_H_
#define _NIU_MA_DOU_DI_ZHU_GAME_RULE_H_

#include "Poker/DouDiZhuRule.h"

namespace NiuMa
{
	enum class DouDiZhuGenre : int
	{
		Invalid = 0,
		Single = 1,
		Pair = 2,
		Triple = 3,
		TripleOne = 4,
		TriplePair = 5,
		Straight = 6,
		StraightPair = 7,
		Plane = 8,
		PlaneOne = 9,
		PlanePair = 10,
		FourTwoSingle = 11,
		FourTwoPair = 12,
		Bomb = 13,
		Rocket = 14
	};

	class DouDiZhuGameRule : public DouDiZhuRule
	{
	public:
		DouDiZhuGameRule();
		virtual ~DouDiZhuGameRule();

	public:
		virtual void initialise() override;
		virtual int getPackNums() const override;
		virtual int getPointNums() const override;
		virtual void sortPointOrder() override;
		virtual void sortSuitOrder() override;
		virtual bool isDisapprovedCard(const PokerCard& c) const override;
		virtual int predicateCardGenre(PokerGenre& pcg) const override;
		virtual int getGenreCardNums(int genre) const override;
		virtual bool isValidGenre(int genre) const override;
		virtual bool straightExcluded(const PokerCard& c) const override;
		virtual bool straightPairExcluded(const PokerCard& c) const override;
		virtual bool straightTripleExcluded(const PokerCard& c) const override;
		virtual bool butterflyExcluded(const PokerCard& c) const override;
		virtual int getBombOrder(int genre) const override;
		virtual int getBombByOrder(int order) const override;

	public:
		int getPlayerCount() const { return _playerCount; }
		int getHandCardCount() const { return _handCardCount; }
		int getBottomCardCount() const { return _bottomCardCount; }
		int getBaseScore() const { return _baseScore; }
		int getRoundCount() const { return _roundCount; }
		int getCallTimeout() const { return _callTimeout; }
		int getAutoPlayTimeout() const { return _autoPlayTimeout; }
		int getMaxRoundScore() const { return _maxRoundScore; }
		void loadConfig(const std::string& ruleConfig);

	private:
		bool findPlane(const CardArray& cards, int wingCardNums, PokerCard& officer) const;
		bool samePointGroups(const CardArray& cards, int groupSize, int minGroups, PokerCard& officer) const;
		bool isRocket(const CardArray& cards, PokerCard& officer) const;
		bool isFourWithTwo(const CardArray& cards, bool pairs, PokerCard& officer) const;

	private:
		int _playerCount;
		int _handCardCount;
		int _bottomCardCount;
		int _baseScore;
		int _roundCount;
		int _callTimeout;
		int _autoPlayTimeout;
		int _maxRoundScore;
	};
}

#endif
