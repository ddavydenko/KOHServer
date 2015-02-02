#include "Room.h"
#include "Helper.h"
// send
void CRoom::SendPlayerDisconnectedPacket(CServer& server, int playerIndex)
{
	RakNet::BitStream outStream;
	outStream.Write(PacketTypeId::PlayerDisconnected);
	outStream.Write(playerIndex);
	SendToAllClients(server, outStream, true);
}

void CRoom::SendWaitingForPlayersPacket(CServer& server, bool reliable)
{
	if (!HasClients())
		return;

	RakNet::BitStream outStream;

	outStream.Write(PacketTypeId::WaitingForPlayers);
	outStream.Write(GetNumClients());
	outStream.Write(GetConnectedClientsCount());

	for (int i = 0; i != m_numClients; ++i)
	{
		if (m_clients[i].IsActive())
		{
			m_clients[i].PackSimpleStatePacket(outStream);
		}
	}

	SendToAllClients(server, outStream, reliable);
}

void CRoom::SendCountdownToGamePacket(CServer& server)
{
	RakNet::BitStream outStream;

	outStream.Write(PacketTypeId::CountdownToGame);
	outStream.Write(CHelper::ToClientTime(m_prepareToGameTimer.GetRestTime(), true));

	SendToAllClients(server, outStream, false);
}

void CRoom::SendGameOverPacket(CServer& server)
{
	int winnerIndex = -1;
	uint64_t bestTime = 0;
	for (int i = 0; i != m_numClients; ++i)
	{
		if (m_clients[i].IsActive())
		{
			uint64_t iTime = m_clients[i].GetPlatformTime();
			if (iTime > bestTime)
			{
				winnerIndex = i;
				bestTime = iTime;
			}
		}
	}
	LT_DoLog2("GameOver index = %i bestTime = %i", winnerIndex, (int)bestTime);

	RakNet::BitStream outStream;

	outStream.Write(PacketTypeId::GameOver);
	outStream.Write(GetConnectedClientsCount());
	outStream.Write(GetTime()); //sec

	for (int i = 0; i != m_numClients; ++i)
	{
		if (m_clients[i].IsActive())
		{
			if (i != m_clients[i].GetPlayerIndex())
				LT_DoErr("Internal error: i != m_clients[i].GetPlayerIndex()");

			outStream.Write(i);//m_clients[i].GetPlayerId());
			outStream.Write(m_clients[i].GetPlayerName());
			outStream.Write(CHelper::ToClientTime(m_clients[i].GetPlatformTime(), false));
			outStream.Write(m_clients[i].GetPunchCount());
			int fWinner = (i == winnerIndex) ? 1 : 0;
			outStream.Write(fWinner);
			LT_DoLog3("GameOver index = %i fWinner =%i time = %i", 
				i, fWinner, CHelper::ToClientTime(m_clients[i].GetPlatformTime(), false));
		}
	}

	SendToAllClients(server, outStream, true);
}

int CRoom::GetRestGameTime()
{
	int restTime = m_timer.GetRestTime();
	if (m_isAloneGame)
	{
		restTime = min(restTime, m_aloneGameTimer.GetRestTime());
	}
	return restTime;
}

void CRoom::SendStatePacket(CServer& server)
{
	if (HasJumpingClients())
	{
		RakNet::BitStream outStream;

		outStream.Write(PacketTypeId::GAME_STATE_ID);
		outStream.Write(GetJumpingClientsCount());
		outStream.Write(CHelper::ToClientTime(GetRestGameTime(), true));

		for(int i=0; i<m_numClients; i++)
		{
			if (m_clients[i].IsJumping())
			{
				m_clients[i].PackStatePacket(outStream);
			}
		}

		SendToAllClients(server, outStream, false);
	}
}

void CRoom::SendFightersState(CServer& server)
{
	if(HasFightingClients())
	{
		RakNet::BitStream outStream;
		outStream.Write(PacketTypeId::FIGHT_ID);
		outStream.Write(GetFightingClientsCount());
		outStream.Write(CHelper::ToClientTime(GetRestGameTime(), true));
		
		for(int i=0; i != m_numClients; ++i)
		{
			if(m_clients[i].IsFighting())
			{
				m_clients[i].PackStatePacket(outStream);
			}
		}
		SendToAllClients(server, outStream, true);
	}
}

void CRoom::SendToAllJumpers(CServer& server, RakNet::BitStream& outStream, bool reliable)
{
	for (int i = 0; i != m_numClients; ++i)
	{
		if (m_clients[i].IsJumping())
		{
			server.Send(outStream, m_clients[i].GetAddress(), reliable);
		}
	}
}
void CRoom::SendToAllFighters(CServer& server, RakNet::BitStream& outStream, bool reliable)
{
	for (int i = 0; i != m_numClients; ++i)
	{
		if (m_clients[i].IsFighting())
		{
			server.Send(outStream, m_clients[i].GetAddress(), reliable);
		}
	}
}
void CRoom::SendToAllClients(CServer& server, RakNet::BitStream& outStream, bool reliable)
{
	for (int i = 0; i != m_numClients; ++i)
	{
		if (m_clients[i].IsActive())
		{
			server.Send(outStream, m_clients[i].GetAddress(), reliable);
		}
	}
}

// count/has

bool CRoom::HasClients()
{
	for(int i=0; i<m_numClients; i++)
	{
		if(m_clients[i].IsActive())
		{
			return true;
		}
	}
	return false;
}

bool CRoom::HasFightingClients()
{
	for(int i=0; i<m_numClients; i++)
	{
		if(m_clients[i].IsFighting())
		{
			return true;
		}
	}
	return false;
}

bool CRoom::HasJumpingClients()
{
	for(int i=0; i<m_numClients; i++)
	{
		if(m_clients[i].IsJumping())
		{
			return true;
		}
	}

	return false;
}

int CRoom::GetConnectedClientsCount()
{
	int count=0;
	for(int i=0; i < m_numClients; i++)
	{
		if(m_clients[i].IsActive())
		{
			count++;
		}
	}
	return count;
}

int CRoom::GetJumpingClientsCount()
{
	int count=0;
	for(int i=0; i < m_numClients; i++)
	{
		if(m_clients[i].IsJumping())
		{
			count++;
		}
	}
	return count;
}

int CRoom::GetFightingClientsCount()
{	
	int count=0;
	for(int i=0; i<m_numClients; i++)
	{
		if(m_clients[i].IsFighting())
		{
			count++;
		}
	}
	return count;
}
//
void CRoom::MakeFight()
{
	for(int i=0; i<m_numClients; i++)
	{
		if(m_clients[i].CanFight() && m_clients[i].IsReadyToShot())
		{
			LT_DoLog1("Player %s fight", m_clients[i].GetPlayerNameAsCString());

			for(int j=0; j<m_numClients; j++)
			{
				if(i != j && 
					m_clients[j].CanFight())
				{
					m_clients[i].MakeFight(m_clients[j]);
				}
			}
			m_clients[i].ShotComplete();
		}
	}
} 
