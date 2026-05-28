// YuanJiangQianFenMessages.cpp

#include "YuanJiangQianFenMessages.h"
#include "Message/MessageCreator.h"

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
		MessageCreator::getSingleton().registCreator(MsgQianFenSync::TYPE, []() -> Message::Ptr { return std::make_shared<MsgQianFenSync>(); });
		MessageCreator::getSingleton().registCreator(MsgQianFenReady::TYPE, []() -> Message::Ptr { return std::make_shared<MsgQianFenReady>(); });
		MessageCreator::getSingleton().registCreator(MsgQianFenCallScore::TYPE, []() -> Message::Ptr { return std::make_shared<MsgQianFenCallScore>(); });
		MessageCreator::getSingleton().registCreator(MsgQianFenPlay::TYPE, []() -> Message::Ptr { return std::make_shared<MsgQianFenPlay>(); });
	}
}
