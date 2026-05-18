#include "pch.h"
#include "User.h"

void User::Init(const UINT32 index)
{
	mIndex = index;
	mPacketDataBuffer = make_shared<char[]>(PACKET_DATA_BUFFER_SIZE);
}

void User::Clear()
{
	mRoomIndex = { -1 };
	mUserID = { "" };
	mIsConfirm = { false };
	mCurDomainState = DOMAIN_STATE::NONE;

	mPacketDataBufferWPos = { 0 };
	mPacketDataBufferRPos = { 0 };
}

int User::SetLogin(shared_ptr<char> userID_)
{
	mCurDomainState = DOMAIN_STATE::LOGIN;
	mUserID = userID_.get();

	return 0;
}

void User::EnterRoom(INT32 roomIndex_)
{
	mRoomIndex = roomIndex_;
	mCurDomainState = DOMAIN_STATE::ROOM;
}

void User::SetPacketData(const UINT32 dataSize_, shared_ptr<char> pData_)
{
}

PacketInfo User::GetPacket()
{
	return PacketInfo();
}
