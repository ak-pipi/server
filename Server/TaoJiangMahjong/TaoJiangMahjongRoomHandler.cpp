// TaoJiangMahjongRoomHandler.cpp

#include "TaoJiangMahjongRoomHandler.h"
#include "TaoJiangMahjongMessages.h"
#include "../GameDefines.h"

namespace NiuMa
{
	TaoJiangMahjongRoomHandler::TaoJiangMahjongRoomHandler(const MessageQueue::Ptr& queue)
		: MahjongRoomHandler(queue)
	{
		// 添加接收的消息类型
		addMessage(MsgTJSync::TYPE);

		// 添加游戏类型
		addGameType(static_cast<int>(GameType::TaoJiangMahjong));
	}

	TaoJiangMahjongRoomHandler::~TaoJiangMahjongRoomHandler() {}
}
