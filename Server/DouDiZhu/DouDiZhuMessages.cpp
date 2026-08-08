// DouDiZhuMessages.cpp

#include "DouDiZhuMessages.h"
#include "Message/MessageManager.h"
#include "Message/MsgCreator.h"

namespace NiuMa
{
	const std::string MsgDouDiZhuSync::TYPE = "DouDiZhu.Sync";
	const std::string MsgDouDiZhuSyncResp::TYPE = "DouDiZhu.SyncResp";
	const std::string MsgDouDiZhuReady::TYPE = "DouDiZhu.Ready";
	const std::string MsgDouDiZhuDeal::TYPE = "DouDiZhu.Deal";
	const std::string MsgDouDiZhuCall::TYPE = "DouDiZhu.Call";
	const std::string MsgDouDiZhuCallNotify::TYPE = "DouDiZhu.CallNotify";
	const std::string MsgDouDiZhuCallFailed::TYPE = "DouDiZhu.CallFailed";
	const std::string MsgDouDiZhuLandlord::TYPE = "DouDiZhu.Landlord";
	const std::string MsgDouDiZhuPlay::TYPE = "DouDiZhu.Play";
	const std::string MsgDouDiZhuPlayNotify::TYPE = "DouDiZhu.PlayNotify";
	const std::string MsgDouDiZhuPlayFailed::TYPE = "DouDiZhu.PlayFailed";
	const std::string MsgDouDiZhuSettlement::TYPE = "DouDiZhu.Settlement";
	const std::string MsgDouDiZhuDisbandVote::TYPE = "DouDiZhu.DisbandVote";

	MsgDouDiZhuSyncResp::MsgDouDiZhuSyncResp()
		: gameState(0)
		, currentPlayer(-1)
		, mySeat(-1)
		, landlordSeat(-1)
		, banker(-1)
		, callStarter(-1)
		, highestBidSeat(-1)
		, highestBid(0)
		, callTurn(-1)
		, callCount(0)
		, multiplier(1)
		, lastPlaySeat(-1)
		, lastPlayGenre(0)
		, isFirstPlay(true)
		, roundNo(0)
		, playerCount(2)
		, level(0)
		, baseScore(1)
		, roundCount(8)
	{
		handCounts[0] = 0;
		handCounts[1] = 0;
	}

	MsgDouDiZhuDeal::MsgDouDiZhuDeal()
		: banker(-1)
		, callStarter(-1)
		, roundNo(0)
		, roundCount(8)
		, baseScore(1)
	{
		handCounts[0] = 0;
		handCounts[1] = 0;
	}

	MsgDouDiZhuCallNotify::MsgDouDiZhuCallNotify()
		: seat(-1)
		, score(0)
		, highestBid(0)
		, highestBidSeat(-1)
		, nextSeat(-1)
		, callCount(0)
	{}

	MsgDouDiZhuLandlord::MsgDouDiZhuLandlord()
		: landlordSeat(-1)
		, multiplier(1)
		, currentPlayer(-1)
	{
		handCounts[0] = 0;
		handCounts[1] = 0;
	}

	MsgDouDiZhuPlayNotify::MsgDouDiZhuPlayNotify()
		: seat(-1)
		, genre(0)
		, nextPlayer(-1)
		, multiplier(1)
	{
		handCounts[0] = 0;
		handCounts[1] = 0;
	}

	MsgDouDiZhuSettlement::MsgDouDiZhuSettlement()
		: winnerSeat(-1)
		, landlordSeat(-1)
		, multiplier(1)
		, spring(false)
		, roomFeeTotal(0)
		, shuffleFeeTotal(0)
	{
		for (int i = 0; i < 2; i++) {
			scores[i] = 0;
			winGolds[i] = 0;
		}
	}

	MsgDouDiZhuDisbandVote::MsgDouDiZhuDisbandVote()
		: disbander(-1)
		, remainTime(300)
	{
		choices[0] = 0;
		choices[1] = 0;
	}

	void DouDiZhuMessages::registMessages() {
		IMsgCreator::Ptr creator = IMsgCreator::Ptr(new MsgCreator<MsgDouDiZhuSync>());
		MessageManager::getSingleton().registCreator(MsgDouDiZhuSync::TYPE, creator);
		creator = IMsgCreator::Ptr(new MsgCreator<MsgDouDiZhuReady>());
		MessageManager::getSingleton().registCreator(MsgDouDiZhuReady::TYPE, creator);
		creator = IMsgCreator::Ptr(new MsgCreator<MsgDouDiZhuCall>());
		MessageManager::getSingleton().registCreator(MsgDouDiZhuCall::TYPE, creator);
		creator = IMsgCreator::Ptr(new MsgCreator<MsgDouDiZhuPlay>());
		MessageManager::getSingleton().registCreator(MsgDouDiZhuPlay::TYPE, creator);
	}
}
