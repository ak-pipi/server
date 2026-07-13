// DouDiZhuRoomHandler.cpp

#include "DouDiZhuRoomHandler.h"
#include "DouDiZhuMessages.h"
#include "../GameDefines.h"

namespace NiuMa
{
	DouDiZhuRoomHandler::DouDiZhuRoomHandler(const MessageQueue::Ptr& queue)
		: GameRoomHandler(queue)
	{
		addMessage(MsgDouDiZhuSync::TYPE);
		addMessage(MsgDouDiZhuReady::TYPE);
		addMessage(MsgDouDiZhuCall::TYPE);
		addMessage(MsgDouDiZhuPlay::TYPE);
		addGameType(static_cast<int>(GameType::DouDiZhu));
	}

	DouDiZhuRoomHandler::~DouDiZhuRoomHandler() {}
}
