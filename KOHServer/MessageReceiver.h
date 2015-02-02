#pragma once
#include "./Raknet/RakString.h"

#include "logger.h"
#include "RoomManager.h"
#include "Server.h"


class CMessageReceiver : public IMessageReceiver
{
	CRoomManager& m_roomManager;
	CServer& m_server;
public:

	CMessageReceiver(CRoomManager& roomManager, CServer& server) 
		: m_roomManager(roomManager)
		, m_server(server)
	{
	}

	virtual void OnConnectionLost(const SystemAddress& address)
	{
		m_roomManager.Disconnect(m_server, address);
	}

	virtual void OnPong(const SystemAddress& address, int packetLoss, int pingTime)
	{
		m_roomManager.ValidatePingData(address, packetLoss, pingTime);
	}

	virtual void OnCustomMessage(const SystemAddress& address, unsigned char pkgId, RakNet::BitStream& inputStream)
	{
		switch (pkgId)
		{
			case PacketTypeId::ConnectToRoom:
				LT_DoLog("ConnectToRoom");
				ProcessConnectToRoom(address, inputStream);
				break;
			case PacketTypeId::GAME_CONTROLS:
				{
					float accelX;
					inputStream.Read(accelX);
					m_roomManager.UpdateClient(address, accelX);
				}
				break;
			case PacketTypeId::FIGHT_ID:
				{
					int moveLeft, moveRight;
					int isShot;
					LT_DoLog("FIGHT_ID");
					inputStream.Read(moveLeft);
					inputStream.Read(moveRight);
					inputStream.Read(isShot);
					m_roomManager.UpdateFighter(address, moveLeft, moveRight, isShot);
				}
				break;
			case PacketTypeId::PauseSignal:
				{
					int iIsPaused;
					inputStream.Read(iIsPaused);
					bool isPaused = 1 == iIsPaused;
					LT_DoLog1("PauseSignal: %s", isPaused ? "pause" : "resume");
					m_roomManager.ProcessPauseSignal(address, isPaused);
				}
				break;
			case PacketTypeId::RoomsInfo:
				LT_DoLog("RoomsInfo");
				ProcessRoomInfo(address, inputStream);
				break;
		}
	}
private:
	void ProcessConnectToRoom(const SystemAddress& address, RakNet::BitStream& inputStream)
	{
		RakNet::RakString deviceId;
		int roomId = -1;
		RakNet::RakString playerName;

		inputStream.Read(deviceId);
		inputStream.Read(roomId);
		inputStream.Read(playerName);

		m_roomManager.ProcessConnectToRoom(address, roomId, deviceId, playerName, m_server);
	}

	void ProcessRoomInfo(const SystemAddress& address, RakNet::BitStream& inputStream)
	{
		RakNet::RakString deviceId;
		inputStream.Read(deviceId);

		RakNet::BitStream outStream;
		m_roomManager.PackOpenRoomsInfo(outStream, deviceId);
		m_server.Send(outStream, address, true);
	}
};

