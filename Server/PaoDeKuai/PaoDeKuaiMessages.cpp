// PaoDeKuaiMessages.cpp

#include "PaoDeKuaiMessages.h"
#include "Message/MessageManager.h"
#include "Message/MsgCreator.h"

namespace NiuMa
{
	const std::string MsgPaoDeKuaiSync::TYPE = "PaoDeKuai.Sync";
	const std::string MsgPaoDeKuaiSyncResp::TYPE = "PaoDeKuai.SyncResp";
	const std::string MsgPaoDeKuaiReady::TYPE = "PaoDeKuai.Ready";
	const std::string MsgPaoDeKuaiDeal::TYPE = "PaoDeKuai.Deal";
	const std::string MsgPaoDeKuaiPlay::TYPE = "PaoDeKuai.Play";
	const std::string MsgPaoDeKuaiPlayNotify::TYPE = "PaoDeKuai.PlayNotify";
	const std::string MsgPaoDeKuaiPlayFailed::TYPE = "PaoDeKuai.PlayFailed";
	const std::string MsgPaoDeKuaiSettlement::TYPE = "PaoDeKuai.Settlement";
	const std::string MsgPaoDeKuaiDisbandVote::TYPE = "PaoDeKuai.DisbandVote";

	MsgPaoDeKuaiSyncResp::MsgPaoDeKuaiSyncResp()
		: gameState(0)
		, currentPlayer(-1)
		, mySeat(-1)
		, lastPlaySeat(-1)
		, lastPlayGenre(0)
		, isFirstPlay(true)
		, roundNo(0)
		, banker(-1)
		, playerCount(2)
		, level(0)
		, baseScore(1)
		, scoreScale(1)
		, roundCount(8)
		, bombCount(0)
		, multiplier(1)
	{
		for (int i = 0; i < 2; i++)
			remainCounts[i] = 0;
	}

	MsgPaoDeKuaiDeal::MsgPaoDeKuaiDeal()
		: firstPlayer(0)
		, roundNo(0)
		, banker(0)
		, roundCount(8)
		, baseScore(1)
		, scoreScale(1)
	{}

	MsgPaoDeKuaiPlayNotify::MsgPaoDeKuaiPlayNotify()
		: seat(-1)
		, genre(0)
		, nextPlayer(-1)
		, remainCount(0)
		, multiplier(1)
	{}

	MsgPaoDeKuaiSettlement::MsgPaoDeKuaiSettlement()
		: winnerSeat(-1)
		, roundNo(0)
		, roundCount(8)
		, baseScore(1)
		, scoreScale(1)
		, bombCount(0)
		, multiplier(1)
		, spring(false)
		, zhaNiao(false)
		, birdHit(false)
		, birdSeat(-1)
		, birdMultiplier(1)
		, roomFeeTotal(0)
		, shuffleFeeTotal(0)
	{
		for (int i = 0; i < 2; i++) {
			scores[i] = 0;
			winGolds[i] = 0;
			bombScores[i] = 0;
			bombWinCounts[i] = 0;
		}
	}

	MsgPaoDeKuaiDisbandVote::MsgPaoDeKuaiDisbandVote()
		: disbander(-1)
		, remainTime(300)
	{
		choices[0] = 0;
		choices[1] = 0;
	}

	void PaoDeKuaiMessages::registMessages() {
		IMsgCreator::Ptr creator1 = IMsgCreator::Ptr(new MsgCreator<MsgPaoDeKuaiSync>());
		MessageManager::getSingleton().registCreator(MsgPaoDeKuaiSync::TYPE, creator1);
		IMsgCreator::Ptr creator2 = IMsgCreator::Ptr(new MsgCreator<MsgPaoDeKuaiReady>());
		MessageManager::getSingleton().registCreator(MsgPaoDeKuaiReady::TYPE, creator2);
		IMsgCreator::Ptr creator3 = IMsgCreator::Ptr(new MsgCreator<MsgPaoDeKuaiPlay>());
		MessageManager::getSingleton().registCreator(MsgPaoDeKuaiPlay::TYPE, creator3);
	}
}
