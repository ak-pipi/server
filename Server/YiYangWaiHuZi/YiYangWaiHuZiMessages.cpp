// YiYangWaiHuZiMessages.cpp

#include "YiYangWaiHuZiMessages.h"
#include "Message/MessageManager.h"
#include "Message/MsgCreator.h"

namespace NiuMa
{
	const std::string MsgWaiHuZiSync::TYPE = "WaiHuZi.Sync";
	const std::string MsgWaiHuZiReady::TYPE = "WaiHuZi.Ready";
	const std::string MsgWaiHuZiDiscard::TYPE = "WaiHuZi.Discard";
	const std::string MsgWaiHuZiAction::TYPE = "WaiHuZi.Action";
	const std::string MsgWaiHuZiSyncResp::TYPE = "WaiHuZi.SyncResp";
	const std::string MsgWaiHuZiDeal::TYPE = "WaiHuZi.Deal";
	const std::string MsgWaiHuZiDrawNotify::TYPE = "WaiHuZi.DrawNotify";
	const std::string MsgWaiHuZiDiscardNotify::TYPE = "WaiHuZi.DiscardNotify";
	const std::string MsgWaiHuZiActionNotify::TYPE = "WaiHuZi.ActionNotify";
	const std::string MsgWaiHuZiSettlement::TYPE = "WaiHuZi.Settlement";

	MsgWaiHuZiSyncResp::MsgWaiHuZiSyncResp()
		: gameState(0), mySeat(-1), currentPlayer(-1), roundNo(0), banker(-1), playerCount(3)
		, lastDiscardId(-1), lastDiscardSeat(-1) {}

	MsgWaiHuZiDeal::MsgWaiHuZiDeal() : firstPlayer(0), roundNo(0), banker(0) {}

	MsgWaiHuZiDrawNotify::MsgWaiHuZiDrawNotify() : seat(-1), cardId(-1), remainCount(0) {}

	MsgWaiHuZiDiscardNotify::MsgWaiHuZiDiscardNotify() : seat(-1), cardId(-1), nextPlayer(-1) {}

	MsgWaiHuZiActionNotify::MsgWaiHuZiActionNotify() : seat(-1), action(0), nextPlayer(-1) {}

	MsgWaiHuZiSettlement::MsgWaiHuZiSettlement() : huSeat(-1), huXi(0), roomFeeTotal(0), shuffleFeeTotal(0) {
		for (int i = 0; i < 3; i++) { scores[i] = 0; winGolds[i] = 0.0; }
	}

	void YiYangWaiHuZiMessages::registMessages() {
		IMsgCreator::Ptr creator1 = IMsgCreator::Ptr(new MsgCreator<MsgWaiHuZiSync>());
		MessageManager::getSingleton().registCreator(MsgWaiHuZiSync::TYPE, creator1);
		IMsgCreator::Ptr creator2 = IMsgCreator::Ptr(new MsgCreator<MsgWaiHuZiReady>());
		MessageManager::getSingleton().registCreator(MsgWaiHuZiReady::TYPE, creator2);
		IMsgCreator::Ptr creator3 = IMsgCreator::Ptr(new MsgCreator<MsgWaiHuZiDiscard>());
		MessageManager::getSingleton().registCreator(MsgWaiHuZiDiscard::TYPE, creator3);
		IMsgCreator::Ptr creator4 = IMsgCreator::Ptr(new MsgCreator<MsgWaiHuZiAction>());
		MessageManager::getSingleton().registCreator(MsgWaiHuZiAction::TYPE, creator4);
	}
}
