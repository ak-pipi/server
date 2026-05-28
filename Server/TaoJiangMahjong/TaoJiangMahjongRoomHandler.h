// TaoJiangMahjongRoomHandler.h

#ifndef _NIU_MA_TAOJIANG_MAHJONG_ROOM_HANDLER_H_
#define _NIU_MA_TAOJIANG_MAHJONG_ROOM_HANDLER_H_

#include "MahjongRoomHandler.h"

namespace NiuMa
{
	class TaoJiangMahjongRoomHandler : public MahjongRoomHandler
	{
	public:
		TaoJiangMahjongRoomHandler(const MessageQueue::Ptr& queue = nullptr);
		virtual ~TaoJiangMahjongRoomHandler();
	};
}

#endif // !_NIU_MA_TAOJIANG_MAHJONG_ROOM_HANDLER_H_
