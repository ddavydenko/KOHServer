#pragma once

#include <list>

#include "Client.h"
#include "fighter.h"
#include "server.h"
#include "PacketTypeId.h"
#include "Logger.h"

class CRoom
{
	static const int MaxNumClients = 4;
	static const int BadClientIndex = -1;
	CClient m_clients[MaxNumClients];
	int m_numClients;

	int m_id;
	int m_time; //sec

	CSimpleTimer m_prepareToGameTimer;
	CSimpleTimer m_timer;
	bool m_isGameStarted;

	bool m_isAloneGame;
	CSimpleTimer m_aloneGameTimer;
public:
	CRoom(int id, int numClients, int time)
		: m_timer(time * 1000)
		, m_prepareToGameTimer(5 * 1000)
		, m_aloneGameTimer(30 * 1000)
	{
		m_id = id;
		m_numClients = numClients;
		m_time = time;
		m_isGameStarted = false;
		m_isAloneGame = false;
	}

	int GetId()
	{
		return m_id;
	}

	int GetNumClients()
	{
		return m_numClients;
	}
	
	int GetTime()
	{
		return m_time;
	}

	bool IsFull()
	{
		return m_numClients == GetConnectedClientsCount();
	}

	bool IsGameStarted()
	{
		if (!m_isGameStarted)
		{
			m_isGameStarted = m_prepareToGameTimer.IsElapsed();
			if (m_isGameStarted)
			{
				m_timer.Init();
				for(int i=0; i < m_numClients; i++)
				{
					if (m_clients[i].IsActive())
					{
						m_clients[i].StartGame();
					}
				}
			}
		}
		return m_isGameStarted;
	}

	void PrepareToGame()
	{
		LT_DoLog("Prepare to game");
		m_prepareToGameTimer.Init();
	}

	bool IsGameOver()
	{
		return m_timer.IsElapsed()
			|| !HasClients()
			|| (m_isAloneGame && m_aloneGameTimer.IsElapsed());
	}
	
	void GetClientsAddresses(std::list<SystemAddress>& addresses)
	{
		for(int i=0; i < m_numClients; i++)
		{
			if (!m_clients[i].IsEmpty())
			{
				addresses.push_back(m_clients[i].GetAddress());
			}
		}
	}

	void PackRoomInfo(RakNet::BitStream& outStream)
	{
		outStream.Write(m_id);
		outStream.Write(m_numClients);
		outStream.Write(m_time);
		outStream.Write(GetConnectedClientsCount());
	}

	void UpdateClient(const SystemAddress& address, float accelX)
	{
		CClient* client = GetClient(address);
		if (NULL != client)
		{
			client->Update(accelX);
		}
	}

	int TryCreateClient(const SystemAddress& address, const RakNet::RakString& deviceId, const RakNet::RakString& playerName)
	{
		int index = TryGetClientIndex(address);
		if (BadClientIndex == index)
		{
			for(int i=0; i<m_numClients; i++)
			{
				if(!m_clients[i].IsJumping())
				{
					m_clients[i].Init(address, deviceId, playerName, i);
					return i;
				}
			}
			LT_DoErr("Internal error: TryCreateClient");
			return -1;
		}

		m_clients[index].StartJumping();
		return index;
	}
	
	int TryRecreateClient(const SystemAddress& oldAddress, const SystemAddress& newAddress,
		const RakNet::RakString& deviceId, const RakNet::RakString& playerName)
	{
		for(int i=0; i<m_numClients; i++)
		{
			if(m_clients[i].GetAddress() == oldAddress && m_clients[i].GetDeviceId() == deviceId)
			{
				m_clients[i].Init(newAddress, deviceId, playerName, i);
				return i;
			}
		}
		return -1;
	}

	void Disconnect(CServer& server, const SystemAddress& address, bool clearClientInfo)
	{
		CClient* client = GetClient(address);
		if (NULL != client)
		{
			SendPlayerDisconnectedPacket(server, client->GetPlayerIndex());
			if (clearClientInfo)
			{
				client->RemoveClient();
			}
			else
			{
				client->Disconnect();
			}
			LT_DoLog1("Player with address %s was disconected", address.ToString());
		}
	}
	
	void ValidatePingData(const SystemAddress& address, int packetLoss, int pingTime)
	{
		CClient* client = GetClient(address);
		if (NULL != client)
		{
			client->ValidatePingData(packetLoss, pingTime);
		}
	}

	void ProccessJumps()
	{
		for(int i=0; i < m_numClients; i++)
		{
			m_clients[i].ProccessJump();
		}
	}

	void PingClients(CServer& server)
	{
		for(int i=0; i < m_numClients; i++)
		{
			if (m_clients[i].IsActive() && !m_clients[i].IsPaused())
			{
				server.Ping(m_clients[i].GetAddress());
			}
		}
	}
	
	void ProcessPausedClients(CServer& server)
	{
		for(int i=0; i < m_numClients; i++)
		{
			if (m_clients[i].IsActive() && m_clients[i].IsGamePauseElapsed())
			{
				LT_DoLog("Force disconnect");
				SendPlayerDisconnectedPacket(server, i);
				m_clients[i].Disconnect();
			}
		}
	}

	bool PreprocessStates()
	{
		bool hasKeyPoints = false;
		for(int i=0; i < m_numClients; i++)
		{
			if (m_clients[i].PreprocessState())
			{
				hasKeyPoints = true;
			}
		}
		ProcessAloneGame();
		return hasKeyPoints;
	}

	bool ProcessCollisions()
	{
		bool hasCheckPoint = false;
		for(int i=0; i<m_numClients; i++)
		{
			if(m_clients[i].CanCollision())
			{
				for(int j=0; j<m_numClients; j++)
				{
					if( (i != j) && m_clients[j].CanCollision())
					{
						if (m_clients[i].CheckCollision(m_clients[j]))
						{
							hasCheckPoint = true;
						}
					}
				}
			}
		}
		return hasCheckPoint;
	}


	int GetConnectedClientsCount();

	CClient* GetClient(const SystemAddress& address)
	{
		int index = GetClientIndex(address);
		if (BadClientIndex == index)
			return NULL;

		return &(m_clients[index]);
	}

	int GetClientIndex(const SystemAddress& address)
	{
		int index = TryGetClientIndex(address);
		if (BadClientIndex == index)
		{
			LT_DoErr("Client not found");
		}
		return index;
	}

	int TryGetClientIndex(const SystemAddress& address)
	{
		for(int i=0; i < m_numClients; i++)
		{
			if(m_clients[i].IsActive() && m_clients[i].GetAddress() == address)
			{
				return i;
			}
		}
		return BadClientIndex;
	}

	//fighter

	void UpdateFighter(const SystemAddress& address, int moveLeft, int moveRight, int isShot)
	{
		CClient* client = GetClient(address);
		if (NULL != client)
		{
			client->UpdateFighter(moveLeft, moveRight, isShot);
		}
	}

	void ProcessFight()
	{
		for(int i=0; i<m_numClients; i++)
		{
			m_clients[i].ProcessFight();
		}
	}

	void ProcessPauseSignal(const SystemAddress& address, bool isPaused)
	{
		CClient* client = GetClient(address);
		if (NULL != client)
		{
			client->SetPaused(isPaused);
		}
	}

	void CheckShots()
	{
		for(int i=0; i<m_numClients; i++)
		{
			m_clients[i].CheckShot();
		}
	}

	void MakeFight();

	void SendStatePacket(CServer& server);
	void SendFightersState(CServer& server);
	void SendWaitingForPlayersPacket(CServer& server, bool reliable);
	void SendGameOverPacket(CServer& server);
	void SendCountdownToGamePacket(CServer& server);
private:
	void SendPlayerDisconnectedPacket(CServer& server, int playerIndex);

	void ProcessAloneGame()
	{
		int activeClientCount = GetConnectedClientsCount();
		if (1 != m_numClients)
		{
			if (!m_isAloneGame && 1 == activeClientCount)
			{
				m_isAloneGame = true;
				m_aloneGameTimer.Init();
			}
			
			if (m_isAloneGame && 1 != activeClientCount)
			{
				m_isAloneGame = false;
			}
		}
	}

	int GetJumpingClientsCount();
	int GetFightingClientsCount();

	void SendToAllJumpers(CServer& server, RakNet::BitStream& outStream, bool reliable);
	void SendToAllFighters(CServer& server, RakNet::BitStream& outStream, bool reliable);

	void SendToAllClients(CServer& server, RakNet::BitStream& outStream, bool reliable);

	bool HasClients();
	bool HasFightingClients();
	bool HasJumpingClients();

	int GetRestGameTime();
};
