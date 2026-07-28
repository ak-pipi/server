// PlayerMessages.cpp

#include "PlayerMessages.h"
#include "Message/MessageManager.h"

namespace NiuMa
{
	const std::string MsgPlayerConnect::TYPE("MsgPlayerConnect");
	const std::string MsgPlayerConnectResp::TYPE("MsgPlayerConnectResp");
	const std::string MsgPlayerSignatureError::TYPE("MsgPlayerSignatureError");
	const std::string MsgPlayerWalletSync::TYPE("MsgPlayerWalletSync");

	void PlayerMessages::registMessages() {
		IMsgCreator::Ptr creator = IMsgCreator::Ptr(new MsgCreator<MsgPlayerConnect>());
		MessageManager::getSingleton().registCreator(MsgPlayerConnect::TYPE, creator);
	}
}
