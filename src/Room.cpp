#include "pch.h"

#include "Room.h"
#include "User.h"

#include "CharacterSyncPacket.h"

void Room::Init(const INT32 roomNum_, const INT32 maxUserCount_)
{
	mRoomNum = roomNum_;
	mMaxUserCount = maxUserCount_;
}

INT16 Room::EnterUser(shared_ptr<User> user_)
{
	if (mCurrentUserCount >= mMaxUserCount)
		return ERROR_CODE::ENTER_ROOM_FULL_USER;

	mUserList.push_back(user_);
	++mCurrentUserCount;

	user_->EnterRoom(mRoomNum);

	return ERROR_CODE::NONE;
}

void Room::LeaveUser(shared_ptr<User> leaveUser_)
{
	mUserList.remove_if([leaveUserId = leaveUser_->GetUserId()](shared_ptr<User> pUser)
		{
			return leaveUserId == pUser->GetUserId();
		});
}

void Room::NotifyChat(INT32 clientIndex_, const char* userID_, const char* msg_)
{
	ROOM_CHAT_NOTIFY_PACKET roomChatNtfyPkt = {};
	roomChatNtfyPkt.PacketId = PACKET_ID::ROOM_CHAT_NOTIFY;
	roomChatNtfyPkt.PacketLength = sizeof(roomChatNtfyPkt);

	CopyMemory(roomChatNtfyPkt.Msg, msg_, sizeof(roomChatNtfyPkt.Msg));
	CopyMemory(roomChatNtfyPkt.UserID, userID_, sizeof(roomChatNtfyPkt.UserID));
	SendToAllUser(sizeof(roomChatNtfyPkt), MakePacketBuffer(roomChatNtfyPkt), clientIndex_, false);
}

void Room::NotifyNewGuest(INT32 clientIndex_, const char* userID_)
{
	ROOM_NEW_GUEST_PACKET roomNewGuestPkt = {};
	roomNewGuestPkt.PacketId = PACKET_ID::ROOM_NEW_GUEST_NOTIFY;
	roomNewGuestPkt.PacketLength = sizeof(ROOM_NEW_GUEST_PACKET);

	CopyMemory(roomNewGuestPkt.UserID, userID_, sizeof(roomNewGuestPkt.UserID));
	SendToAllUser(sizeof(roomNewGuestPkt), MakePacketBuffer(roomNewGuestPkt), clientIndex_, false);
}

void Room::CharacterSync(INT32 clientIndex_, shared_ptr<char[]> pData)
{
	SendToAllUser(sizeof(CHARACTER_SYNC_PACKET), pData, clientIndex_, false);
}

void Room::SendToAllUser(const UINT16 dataSize_, shared_ptr<char[]> data_, const INT32 passUserIndex_, bool exceptMe)
{
	for (auto& pUser : mUserList)
	{
		if (pUser == nullptr)
			continue;

		if (exceptMe && pUser->GetNetConnIdx() == passUserIndex_)
			continue;

		SendPacketFunc(pUser->GetNetConnIdx(), static_cast<UINT32>(dataSize_), data_);
	}
}
