#pragma once

#include "./Raknet/RakNetTypes.h"

#include <map>
#include <list>
#include <vector>

#include "Logger.h"
#include "GameConstants.h"
#include "Configuration.h"

class CRoomManager
{
	typedef std::vector<CRoom*> CRoomPtrVector;
//	static const int NumRoomTypes = 6;
	CRoomPtrVector m_openRooms;
	std::vector<int> m_roomsStatistics;

	typedef std::map<int, CRoom*> CRoomMap;
	CRoomMap m_rooms;
	
	typedef std::map<SystemAddress, int> CAddrToRoomMap;
	CAddrToRoomMap m_addrToRoom;

	typedef std::list<int> CRoomsForRemoving;
	CRoomsForRemoving m_roomsForRemoving;

	std::list<SystemAddress> m_addressesForRemoving;

	int m_roomNextId;
	RakNetTimeUS m_prevCalcTime;

	typedef std::map<RakNet::RakString, SystemAddress> CDeviceIdToAddressMap;
	CDeviceIdToAddressMap m_deviceIdToAddress;
public:
	CRoomManager()
	{
		CConfiguration cfg = CConfiguration::GetInstance();
		for (int i = 0; i != cfg.GetNumberOfRooms(); ++i)
		{
			m_openRooms.push_back(new CRoom(i, cfg.GetNumberOfPlayersInRoom(i), cfg.GetRoomTime(i)));
			m_roomsStatistics.push_back(0);
		}

		m_roomNextId = m_openRooms.size();

		m_prevCalcTime = RakNet::GetTimeNS();
	}

	~CRoomManager()
	{
		for(CRoomMap::iterator it = m_rooms.begin(); it != m_rooms.end(); ++it)
		{
			delete it->second;
		}
		
		for (CRoomPtrVector::iterator oi = m_openRooms.begin(); oi != m_openRooms.end(); ++oi)
		{
			delete *oi;
		}
	}

	int GetNumberOfRooms()
	{
		return m_rooms.size();
	}

	void PackOpenRoomsInfo(RakNet::BitStream& outStream, const RakNet::RakString& deviceId)
	{
		LT_DoLog1("PackOpenRoomsInfo: deviceId=%s", deviceId.C_String());
		outStream.Write(PacketTypeId::RoomsInfo);
		outStream.Write(m_openRooms.size());

		for (CRoomPtrVector::iterator oi = m_openRooms.begin(); oi != m_openRooms.end(); ++oi)
		{
			(*oi)->PackRoomInfo(outStream);
		}

		CRoom* pOldRoom = GetOldRoomByDeviceId(deviceId);
		bool hasOldRoom = NULL != pOldRoom;

		outStream.Write(CHelper::BoolToInt(hasOldRoom));
	}

	void ProcessConnectToRoom(const SystemAddress& address, int roomId, const RakNet::RakString& deviceId, RakNet::RakString playerName, CServer& server)
	{
		LT_DoLog1("ProcessConnectToRoom: deviceId=%s", deviceId.C_String());
		if (-2 == roomId)
		{
			if (!TryConnectToOldRoom(address, deviceId, playerName, server))
			{
				SendConnectToRoomPacket(address, server, NULL, -1);
			}
			return;
		}
		if (-1 == roomId)
		{
			roomId = GetQuickStartRoom();
			//LT_DoLog1("connected to room: QuickStart -  %i \n", roomId);
		}
		int index = -1;
		for (int i = 0; i != (int)m_openRooms.size(); ++i)
		{
			if (m_openRooms[i]->GetId() == roomId)
			{
				index = i;
			}
		}

		if (-1 == index)
		{
			SendConnectToRoomPacket(address, server, NULL, -1);
			return;
		}

		int clientIndex = m_openRooms[index]->TryCreateClient(address, deviceId, playerName);

		SendConnectToRoomPacket(address, server, m_openRooms[index], clientIndex);

		m_addrToRoom[address] = m_openRooms[index]->GetId();

		if (m_openRooms[index]->IsFull())
		{
			CRoom* oldRoom = m_openRooms[index];
			m_openRooms[index] = new CRoom(m_roomNextId, oldRoom->GetNumClients(), oldRoom->GetTime());
			m_roomsStatistics[index]++;
			m_roomNextId++;
			CRoomMap::const_iterator ri = m_rooms.find(oldRoom->GetId());
			if (m_rooms.end() != ri)
			{
				LT_DoErr("room with the same id already exists");
				delete(ri->second);
			}
			m_rooms[oldRoom->GetId()] = oldRoom;
			oldRoom->PrepareToGame();
			oldRoom->SendWaitingForPlayersPacket(server, true);
			//
			std::list<SystemAddress> addresses;
			oldRoom->GetClientsAddresses(addresses);
			for(std::list<SystemAddress>::iterator ai = addresses.begin(); ai != addresses.end(); ++ai)
			{
				CClient* pClient = oldRoom->GetClient(*ai);
				m_deviceIdToAddress[pClient->GetDeviceId()] = *ai;
			}
			//
		}

		LT_DoLog1("connected to room %i", roomId);
	}

	bool TryConnectToOldRoom(const SystemAddress& address, const RakNet::RakString& deviceId,
		RakNet::RakString playerName, CServer& server)
	{
		CRoom* pOldRoom = GetOldRoomByDeviceId(deviceId);
		if (NULL == pOldRoom)
		{
			LT_DoErr("TryConnectToOldRoom: GetOldRoomByDeviceId returns NULL");
			return false;
		}

		SystemAddress oldAddress = m_deviceIdToAddress[deviceId];
		m_deviceIdToAddress[deviceId] = address;
		m_addrToRoom.erase(oldAddress);
		m_addrToRoom[address] = pOldRoom->GetId();

		int clientIndex = pOldRoom->TryRecreateClient(oldAddress, address, deviceId, playerName);
		if (-1 == clientIndex)
		{
			LT_DoErr("TryConnectToOldRoom: -1 == clientIndex");
			return false;
		}

		SendConnectToRoomPacket(address, server, pOldRoom, clientIndex);
		pOldRoom->SendWaitingForPlayersPacket(server, true);

		return true;
	}

	void SendConnectToRoomPacket(const SystemAddress& address, CServer& server, CRoom* pRoom, int clientIndex)
	{
		bool fResult = (NULL != pRoom);

		if (!fResult)
			LT_DoLog("connected to room: room not found \n");

		RakNet::BitStream outStream;
		outStream.Write(PacketTypeId::ConnectToRoom);
		outStream.Write(CHelper::BoolToInt(fResult));
		outStream.Write(fResult ? pRoom->GetNumClients() : -1);
		outStream.Write(fResult ? pRoom->GetTime() : -1);//sec
		outStream.Write(fResult ? clientIndex : -1);
		server.Send(outStream, address, true);
	}

	int GetQuickStartRoom()
	{
		int index = 0;
		int roomSize = m_openRooms[0]->GetNumClients();
		int waitedPlayers = roomSize - m_openRooms[0]->GetConnectedClientsCount();
		for (int i = 1; i < (int)m_openRooms.size(); ++i)
		{
			int iRoomSize = m_openRooms[i]->GetNumClients();
			int iWaitedPlayers = iRoomSize - m_openRooms[i]->GetConnectedClientsCount();
			if ((iWaitedPlayers < waitedPlayers) 
				|| ((iWaitedPlayers == waitedPlayers) && (iRoomSize > roomSize)))
			{
				index = i;
				roomSize = iRoomSize;
				waitedPlayers = iWaitedPlayers;
			}
		}
		return m_openRooms[index]->GetId();
	} 

	void Disconnect(CServer& server, const SystemAddress& address)
	{
		CAddrToRoomMap::iterator atri = m_addrToRoom.find(address);
		if (m_addrToRoom.end() == atri)
			return;

		int roomId = atri->second;
		CRoom* pOpenRoom = GetOpenRoom(roomId);
		if (NULL != pOpenRoom)
		{
			pOpenRoom->Disconnect(server, address, true);
			m_addrToRoom.erase(address);
			return;
		}

		CRoom* pRoom = GetRoomByAddress(address);
		if (NULL != pRoom)
		{
			pRoom->Disconnect(server, address, false);
		}
		else
		{
			//internal error
			m_addrToRoom.erase(address);
		}
	}
	
	void UpdateClient(const SystemAddress& address, float accelX)
	{
		CRoom* pRoom = GetRoomByAddress(address);
		if (NULL == pRoom)
			return;

		pRoom->UpdateClient(address, accelX);
	}

	void UpdateFighter(const SystemAddress& address, int moveLeft, int moveRight, int isShot)
	{
		CRoom* pRoom = GetRoomByAddress(address);
		if (NULL == pRoom)
			return;
		pRoom->UpdateFighter(address, moveLeft, moveRight, isShot);
	}

	void ProcessPauseSignal(const SystemAddress& address, bool isPaused)
	{
		CRoom* pRoom = GetRoomByAddress(address);
		if (NULL == pRoom)
			return;

		pRoom->ProcessPauseSignal(address, isPaused);
	}

	void ValidatePingData(const SystemAddress& address, int packetLoss, int pingTime)
	{
		CRoom* pRoom = GetRoomByAddress(address, true, false);
		if (NULL == pRoom)
			return;

		pRoom->ValidatePingData(address, packetLoss, pingTime);
	}

	void PingClients(CServer& server)
	{
		for (CRoomPtrVector::iterator oi = m_openRooms.begin(); oi != m_openRooms.end(); ++oi)
		{
			(*oi)->PingClients(server);
		}
		for (CRoomMap::iterator it = m_rooms.begin(); it != m_rooms.end(); it++)
		{
			it->second->PingClients(server);
		}
	}

	void SendWaitingForPlayers(CServer& server)
	{
		for (CRoomPtrVector::iterator oi = m_openRooms.begin(); oi != m_openRooms.end(); ++oi)
		{
			(*oi)->SendWaitingForPlayersPacket(server, false);
		}
	}

	void CalculateGameAndSendState(CServer& server, bool needSend)
	{
		RakNetTimeUS currentTime = RakNet::GetTimeNS();
		uint64_t timeDif = currentTime - m_prevCalcTime;
		GameConstants::SetDeltaTime((float)timeDif);
		m_prevCalcTime = currentTime;

		for (CRoomMap::iterator it = m_rooms.begin(); it != m_rooms.end(); it++)
		{
			CRoom* pRoom = it->second;
			bool hasKeyPoints = false;
			if (pRoom->IsGameStarted())
			{
				pRoom->ProcessPausedClients(server);
				pRoom->PreprocessStates();
				if (pRoom->ProcessCollisions())
					hasKeyPoints = true;
				pRoom->CheckShots();
				pRoom->MakeFight();
				pRoom->ProcessFight();
				pRoom->ProccessJumps();
			}

			if (needSend || hasKeyPoints)
			{
				SendRoomState(server, pRoom);
			}
		}

		RemoveUnusedRooms();
	}
	
	void WriteRoomsStatistics()
	{
		TCHAR sFileName[MAX_PATH+1] = {0};
		TCHAR thisModulePath[MAX_PATH] = {0};
		GetModuleFileName(NULL, thisModulePath, MAX_PATH); 
		LPTSTR filePart = NULL;
		GetFullPathName(thisModulePath, MAX_PATH + 1, sFileName, &filePart);
		*filePart = 0;
		wsprintf(filePart, _T("room_statistics.log"));

		SYSTEMTIME sysTime = {0};
		GetLocalTime(&sysTime);

		HANDLE hFile = CreateFile(sFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL,
								OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (INVALID_HANDLE_VALUE != hFile)
		{
			SetFilePointer( hFile, 0, NULL, FILE_END);
			TCHAR buffer[512] = {0};
			wsprintf(buffer, _T("%02hu.%02hu.%02hu %02hu:%02hu:%02hu\t"),
				sysTime.wDay, sysTime.wMonth, sysTime.wYear,
				sysTime.wHour, sysTime.wMinute, sysTime.wSecond);

			DWORD dwWritten;
			WriteFile(hFile, buffer, (DWORD)strlen(buffer), &dwWritten, NULL);

			for (size_t i = 0; i != m_roomsStatistics.size(); ++i)
			{
				wsprintf(buffer, _T("%i/%i %i \t"), 
					m_openRooms[i]->GetNumClients(), m_openRooms[i]->GetTime(), m_roomsStatistics[i]);
				WriteFile(hFile, buffer, (DWORD)strlen(buffer), &dwWritten, NULL);
				m_roomsStatistics[i] = 0;
			}
			WriteFile(hFile, _T("\n"), (DWORD)strlen(_T("\n")), &dwWritten, NULL);

			CloseHandle(hFile);
		}
	}
private:
	CRoom* GetOldRoomByDeviceId(const RakNet::RakString& deviceId)
	{
		LT_DoLog1("GetOldRoomByDeviceId: deviceId=%s", deviceId.C_String());
		CRoom* pOldRoom = NULL;
		CDeviceIdToAddressMap::iterator di = m_deviceIdToAddress.find(deviceId);
		if (m_deviceIdToAddress.end() != di)
		{
			pOldRoom = GetRoomByAddress(di->second);
			if (NULL == pOldRoom)
			{
				LT_DoErr1("GetOldRoomByDeviceId: NULL == pOldRoom addr=%s", di->second.ToString());
			}
		}
		else
		{
			LT_DoLog("GetOldRoomByDeviceId: m_deviceIdToAddress.end() == di");
		}
		return pOldRoom;
	}

	CRoom* GetRoomByAddress(const SystemAddress& address)
	{
		return GetRoomByAddress(address, false, true);
	}

	CRoom* GetRoomByAddress(const SystemAddress& address, bool includeOpenRooms, bool showError)
	{
		CAddrToRoomMap::iterator atri = m_addrToRoom.find(address);
		if (m_addrToRoom.end() == atri)
		{
			if (showError)
			{
				LT_DoErr("Cannot resolve room by address");
			}
			return NULL;
		}

		int roomId = atri->second;

		CRoomMap::iterator ri = m_rooms.find(roomId);
		if (m_rooms.end() != ri)
		{
			return ri->second;
		}

		if (includeOpenRooms)
		{
			CRoom* openRoom = GetOpenRoom(roomId);
			if (NULL != openRoom)
			{
				return openRoom;
			}
		}

		LT_DoErr("room was not found");

		return NULL;
	}
	
	CRoom* GetOpenRoom(int roomId)
	{
		for (CRoomPtrVector::iterator oi = m_openRooms.begin(); oi != m_openRooms.end(); ++oi)
		{
			if ((*oi)->GetId() == roomId)
			{
				return *oi;
			}
		}

		return NULL;
	}

	void SendRoomState(CServer& server, CRoom* pRoom)
	{
		if (pRoom->IsGameStarted())
		{
			if (pRoom->IsGameOver())
			{
				pRoom->SendGameOverPacket(server);
				m_roomsForRemoving.push_back(pRoom->GetId());
			}
			else
			{
				pRoom->SendStatePacket(server);
				pRoom->SendFightersState(server);
			}
		}
		else
		{
			pRoom->SendCountdownToGamePacket(server);
		}
	}

	void RemoveUnusedRooms()
	{
		if (!m_roomsForRemoving.empty())
		{
			for (CRoomsForRemoving::iterator ri = m_roomsForRemoving.begin(); ri != m_roomsForRemoving.end(); ++ri)
			{
				CRoom* pRoom = m_rooms[*ri];
				m_rooms.erase(*ri);

				pRoom->GetClientsAddresses(m_addressesForRemoving);
				for(std::list<SystemAddress>::iterator ai = m_addressesForRemoving.begin(); ai != m_addressesForRemoving.end(); ++ai)
				{
					m_addrToRoom.erase(*ai);
					m_deviceIdToAddress.erase(pRoom->GetClient(*ai)->GetDeviceId());
				}
				m_addressesForRemoving.clear();

				delete pRoom;
			}

			m_roomsForRemoving.clear();
		}
	}

};
