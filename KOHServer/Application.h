#pragma once
#include <windows.h>

#include "PacketTypeId.h"
#include "Client.h"
#include "fighter.h"
#include "Room.h"
#include "Server.h"
#include "MessageReceiver.h"
#include "Logger.h"

class CApplication
{
	CSimpleTimer m_receiveTimer;
	CSimpleTimer m_sendTimer;
	CSimpleTimer m_pingTimer;
	CSimpleTimer m_statisticsTimer;

	CServer m_server;
public:
	
	CApplication()
		: m_receiveTimer(30)
		, m_sendTimer(40)
		, m_pingTimer(CConfiguration::GetInstance().GetPingPeriod())
		, m_statisticsTimer(60 * 60 * 1000) //1h
	{
	}

	void Start(HANDLE hEvent)
	{
		m_server.Start(10000);
		MainLoop(hEvent);
	}

private:

	void MainLoop(HANDLE hEvent)
	{
		CRoomManager roomManager;
		CMessageReceiver receiver(roomManager, m_server);
		
		uint64_t time = RakNet::GetTime();
		int worstTime = 0;
		float totalTime = 0.f;
		float count = 0;

		while(1)
		{

			{
				uint64_t time1 = RakNet::GetTime();
				int delta = (int)(time1 - time);
				if (delta > worstTime)
					worstTime = delta;
				time = time1;
				totalTime += delta;
				count +=1;
			}

			if (NeedExecute(m_receiveTimer))
			{
				m_server.ListenIncomingPackets(receiver);
			}

			bool needSend = NeedExecute(m_sendTimer);

			roomManager.CalculateGameAndSendState(m_server, needSend);

			if (needSend)
			{
				roomManager.SendWaitingForPlayers(m_server);
			}

			if (NeedExecute(m_pingTimer))
			{
				roomManager.PingClients(m_server);
			}

			if (NeedExecute(m_statisticsTimer))
			{
				LT_DoLog3("Statistics wt=%i m=%f rooms=%i", worstTime, totalTime/count, roomManager.GetNumberOfRooms());
				worstTime = 0;
				totalTime = 0.;
				count =0.;
				roomManager.WriteRoomsStatistics();
			}
			if (NULL != hEvent)
			{
				if (WAIT_TIMEOUT != WaitForSingleObject(hEvent, 10))
				{
					return;
				}
			}
			else
			{
				Sleep(10);
			}
		}
	}

	bool NeedExecute(CSimpleTimer& timer)
	{
		bool isElapsed = timer.IsElapsed();
		if (isElapsed)
		{
			timer.Init();
		}
		return isElapsed;
	}
};
