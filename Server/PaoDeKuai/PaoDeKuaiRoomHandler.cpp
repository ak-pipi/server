// PaoDeKuaiRoomHandler.cpp

#include "PaoDeKuaiRoomHandler.h"
#include "PaoDeKuaiMessages.h"
#include "../GameDefines.h"

namespace NiuMa
{
	PaoDeKuaiRoomHandler::PaoDeKuaiRoomHandler(const MessageQueue::Ptr& queue)
		: GameRoomHandler(queue)
	{
		addMessage(MsgPaoDeKuaiSync::TYPE);
		addMessage(MsgPaoDeKuaiReady::TYPE);
		addMessage(MsgPaoDeKuaiPlay::TYPE);
		addGameType(static_cast<int>(GameType::PaoDeKuai));
	}

	PaoDeKuaiRoomHandler::~PaoDeKuaiRoomHandler() {}
}
