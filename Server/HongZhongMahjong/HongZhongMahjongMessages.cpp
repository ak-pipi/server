// HongZhongMahjongMessages.cpp

#include "HongZhongMahjongMessages.h"
#include "Message/MessageManager.h"

namespace NiuMa
{
	const std::string MsgHZSync::TYPE("MsgHZSync");

	const std::string MsgHZSyncResp::TYPE("MsgHZSyncResp");

	MsgHZSyncResp::MsgHZSyncResp()
		: gold(0)
		, diamond(0)
		, diZhu(0)
		, chi(false)
		, dianPao(false)
		, hasFetch(false)
		, seat(0)
		, roundState(0)
		, disbandState(0)
		, banker(0)
		, playerCount(2)
		, roundNo(0)
		, roundCount(0)
		, leftTiles(0)
	{
		for (int i = 0; i < 4; i++)
			handTileNums[i] = 0;
	}

	MsgHZSyncResp::~MsgHZSyncResp() {}

	const std::string MsgHZStartRound::TYPE("MsgHZStartRound");

	MsgHZStartRound::MsgHZStartRound()
		: banker(0)
		, playerCount(2)
		, roundNo(0)
		, roundCount(0)
	{}

	MsgHZStartRound::~MsgHZStartRound() {}

	const std::string MsgHZSettlement::TYPE("MsgHZSettlement");

	MsgHZSettlement::MsgHZSettlement()
		: kick(false)
		, birdMultiplier(1)
	{
		for (int i = 0; i < 4; i++) {
			golds[i] = 0LL;
			winGolds[i] = 0;
		}
	}

	MsgHZSettlement::~MsgHZSettlement() {}

	const std::string MsgHZDisbandVote::TYPE("MsgHZDisbandVote");

	MsgHZDisbandVote::MsgHZDisbandVote()
		: disbander(0)
		, elapsed(0)
	{
		for (int i = 0; i < 4; i++)
			choices[i] = 0;
	}

	MsgHZDisbandVote::~MsgHZDisbandVote() {}

	void HongZhongMahjongMessages::registMessages() {
		IMsgCreator::Ptr creator = IMsgCreator::Ptr(new MsgCreator<MsgHZSync>());
		MessageManager::getSingleton().registCreator(MsgHZSync::TYPE, creator);
	}
}
