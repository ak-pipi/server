// PlayerSignatureHandler.cpp

#include "Base/Log.h"
#include "PlayerSignatureHandler.h"
#include "PlayerManager.h"
#include "PlayerMessages.h"
#include "Network/SecurityManager.h"

namespace NiuMa
{
	PlayerSignatureHandler::PlayerSignatureHandler(const MessageQueue::Ptr& queue)
		: MessageHandler(queue)
	{}

	PlayerSignatureHandler::~PlayerSignatureHandler() {}

	bool PlayerSignatureHandler::preproccess(const NetMessage::Ptr& netMsg) {
		const MsgBase::Ptr& msg = netMsg->getMessage();
		MsgPlayerSignature* sign = dynamic_cast<MsgPlayerSignature*>(msg.get());
		if (sign == nullptr)
			return true;
		Session::Ptr session = netMsg->getSession();
		if (!session)
			return false;
		// 验证玩家签名数据
		DebugS << "Signature check: playerId=" << sign->getPlayerId()
			<< ", timestamp=" << sign->getTimestamp()
			<< ", nonce=" << sign->getNonce()
			<< ", signature=" << sign->getSignature();
		bool outdate = false;
		if (!PlayerManager::getSingleton().verifySignature(sign->getPlayerId(),
			sign->getTimestamp(), sign->getNonce(), sign->getSignature(), outdate)) {
			if (!outdate) {
				// 签名验证失败，记录一次异常行为
				const std::string& remoteIp = session->getRemoteIp();
				SecurityManager::getSingleton().abnormalBehavior(remoteIp);
			}
			WarningS << "Send MsgPlayerSignatureError to player(id:" << sign->getPlayerId() << "), ip:" << session->getRemoteIp() << ", msgType:" << msg->getType();
			MsgPlayerSignatureError resp;
			resp.outdate = outdate;
			resp.send(session);
			return false;
		}
		return true;
	}
}