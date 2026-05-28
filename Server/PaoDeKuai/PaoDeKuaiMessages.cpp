// PaoDeKuaiMessages.cpp

#include "PaoDeKuaiMessages.h"
#include "Message/MessageCreator.h"

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
	{}

	MsgPaoDeKuaiDeal::MsgPaoDeKuaiDeal()
		: firstPlayer(0)
		, roundNo(0)
		, banker(0)
	{}

	MsgPaoDeKuaiPlayNotify::MsgPaoDeKuaiPlayNotify()
		: seat(-1)
		, genre(0)
		, nextPlayer(-1)
	{}

	MsgPaoDeKuaiSettlement::MsgPaoDeKuaiSettlement()
		: winnerSeat(-1)
	{
		for (int i = 0; i < 2; i++) {
			scores[i] = 0;
			winGolds[i] = 0;
		}
	}

	void PaoDeKuaiMessages::registMessages() {
		MessageCreator::getSingleton().registCreator(
			MsgPaoDeKuaiSync::TYPE,
			[]() -> Message::Ptr { return std::make_shared<MsgPaoDeKuaiSync>(); });
		MessageCreator::getSingleton().registCreator(
			MsgPaoDeKuaiReady::TYPE,
			[]() -> Message::Ptr { return std::make_shared<MsgPaoDeKuaiReady>(); });
		MessageCreator::getSingleton().registCreator(
			MsgPaoDeKuaiPlay::TYPE,
			[]() -> Message::Ptr { return std::make_shared<MsgPaoDeKuaiPlay>(); });
	}
}
