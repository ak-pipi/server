// DouDiZhuRoomHandler.cpp

#include "DouDiZhuRoomHandler.h"
#include "../GameDefines.h"

namespace NiuMa
{
	DouDiZhuRoomHandler::DouDiZhuRoomHandler(const MessageQueue::Ptr& queue)
		: GameRoomHandler(queue)
	{
		addGameType(static_cast<int>(GameType::DouDiZhu));
	}

	DouDiZhuRoomHandler::~DouDiZhuRoomHandler() {}
}
