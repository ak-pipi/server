// YuanJiangQianFenMessages.cpp

#include "YuanJiangQianFenMessages.h"
#include "Message/MessageManager.h"
#include "Message/MsgCreator.h"

namespace NiuMa
{
	const std::string MsgQianFenSync::TYPE = "QianFen.Sync";
	const std::string MsgQianFenReady::TYPE = "QianFen.Ready";
	const std::string MsgQianFenCallScore::TYPE = "QianFen.CallScore";
	const std::string MsgQianFenPlay::TYPE = "QianFen.Play";
	const std::string MsgQianFenSyncResp::TYPE = "QianFen.SyncResp";
	const std::string MsgQianFenDeal::TYPE = "QianFen.Deal";
	const std::string MsgQianFenCallScoreNotify::TYPE = "QianFen.CallScoreNotify";
	const std::string MsgQianFenPlayNotify::TYPE = "QianFen.PlayNotify";
	const std::string MsgQianFenRoundResult::TYPE = "QianFen.RoundResult";
	const std::string MsgQianFenFinalResult::TYPE = "QianFen.FinalResult";

	MsgQianFenSyncResp::MsgQianFenSyncResp() : gameState(0), mySeat(-1), currentPlayer(-1), roundNo(0), banker(-1), playerCount(4), bankerSeat(-1) {}
	MsgQianFenDeal::MsgQianFenDeal() : roundNo(0), banker(0) {}
	MsgQianFenCallScoreNotify::MsgQianFenCallScoreNotify() : seat(-1), score(0), nextSeat(-1), bankerSeat(-1) {}
	MsgQianFenPlayNotify::MsgQianFenPlayNotify() : seat(-1), nextPlayer(-1) {}
	MsgQianFenRoundResult::MsgQianFenRoundResult() { for (int i = 0; i < 4; i++) { scores[i] = 0; winGolds[i] = 0; roundScoreCards[i] = 0; } }
	MsgQianFenFinalResult::MsgQianFenFinalResult() { for (int i = 0; i < 4; i++) { totalScores[i] = 0; totalGolds[i] = 0; } }

	void YuanJiangQianFenMessages::registMessages() {
		IMsgCreator::Ptr creator1 = IMsgCreator::Ptr(new MsgCreator<MsgQianFenSync>());
		MessageManager::getSingleton().registCreator(MsgQianFenSync::TYPE, creator1);
		IMsgCreator::Ptr creator2 = IMsgCreator::Ptr(new MsgCreator<MsgQianFenReady>());
		MessageManager::getSingleton().registCreator(MsgQianFenReady::TYPE, creator2);
		IMsgCreator::Ptr creator3 = IMsgCreator::Ptr(new MsgCreator<MsgQianFenCallScore>());
		MessageManager::getSingleton().registCreator(MsgQianFenCallScore::TYPE, creator3);
		IMsgCreator::Ptr creator4 = IMsgCreator::Ptr(new MsgCreator<MsgQianFenPlay>());
		MessageManager::getSingleton().registCreator(MsgQianFenPlay::TYPE, creator4);
	}
}
