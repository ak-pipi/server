// TaoJiangMahjongMessages.cpp

#include "TaoJiangMahjongMessages.h"
#include "Message/MessageManager.h"

namespace NiuMa
{
	const std::string MsgTJSync::TYPE("MsgTJSync");

	const std::string MsgTJSyncResp::TYPE("MsgTJSyncResp");

	MsgTJSyncResp::MsgTJSyncResp()
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
		, leftTiles(0)
	{
		for (int i = 0; i < 4; i++)
			handTileNums[i] = 0;
	}

	MsgTJSyncResp::~MsgTJSyncResp() {}

	const std::string MsgTJStartRound::TYPE("MsgTJStartRound");

	MsgTJStartRound::MsgTJStartRound()
		: banker(0)
	{}

	MsgTJStartRound::~MsgTJStartRound() {}

	const std::string MsgTJSettlement::TYPE("MsgTJSettlement");

	MsgTJSettlement::MsgTJSettlement()
		: kick(false)
	{
		for (int i = 0; i < 4; i++) {
			golds[i] = 0LL;
			winGolds[i] = 0;
		}
	}

	MsgTJSettlement::~MsgTJSettlement() {}

	const std::string MsgTJDisbandVote::TYPE("MsgTJDisbandVote");

	MsgTJDisbandVote::MsgTJDisbandVote()
		: disbander(0)
		, elapsed(0)
	{
		for (int i = 0; i < 4; i++)
			choices[i] = 0;
	}

	MsgTJDisbandVote::~MsgTJDisbandVote() {}

	void TaoJiangMahjongMessages::registMessages() {
		IMsgCreator::Ptr creator = IMsgCreator::Ptr(new MsgCreator<MsgTJSync>());
		MessageManager::getSingleton().registCreator(MsgTJSync::TYPE, creator);
	}
}
