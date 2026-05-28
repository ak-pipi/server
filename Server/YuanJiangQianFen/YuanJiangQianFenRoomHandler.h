// YuanJiangQianFenRoomHandler.h
#ifndef _NIU_MA_YUANJIANG_QIANFEN_ROOM_HANDLER_H_
#define _NIU_MA_YUANJIANG_QIANFEN_ROOM_HANDLER_H_
#include "Game/GameRoomHandler.h"
namespace NiuMa {
	class YuanJiangQianFenRoomHandler : public GameRoomHandler {
	public:
		YuanJiangQianFenRoomHandler(const MessageQueue::Ptr& queue = nullptr);
		virtual ~YuanJiangQianFenRoomHandler();
	};
}
#endif
