#pragma once

#include "./Raknet/RakString.h"
#include "./Raknet/RakNetTypes.h"
#include "./Raknet/BitStream.h"

#include "clouds.h"
#include "simpleTimer.h"
#include "GameTimer.h"

#include "logger.h"
#include "jumper.h"
#include "fighter.h"

#include "configuration.h"

class CClient
{
	enum EClientState { Empty = 0, Disconnected = 0, JumpMode, FightMode };
	EClientState m_state;
	SystemAddress m_address;
	CGameTimer m_platformTimer;
	int m_pingTime;
	uint64_t m_lastPingTime;
	RakNet::RakString m_playerName;
	RakNet::RakString m_deviceId;

	bool m_isPingPaused;
	bool m_isPaused;

	CJumper m_jumper;
	CFighter m_fighter;

	CSimpleTimer m_pauseTimer;
	int m_index;
	int m_punchCount;
public:

	CClient()
		: m_pauseTimer(CConfiguration::GetInstance().GetPauseTimeout())
	{
		RemoveClient();
	}

	int GetPlayerIndex()
	{
		return m_index;
	}

	bool IsFighting()
	{
		return FightMode == m_state;
	}

	bool IsJumping()
	{
		return JumpMode == m_state;
	}

	int GetPunchCount()
	{
		return m_punchCount;
	}
	//bool IsDisconnected()
	//{
	//	return Disconnected == m_state;
	//}

	bool IsActive()
	{
		return IsJumping() || IsFighting();
	}

	bool IsEmpty()
	{
		return Empty == m_state;
	}

	bool CanCollision()
	{
		return (IsJumping()
				&& !IsGamePaused()
				&& m_jumper.IsInNormalState());
	}

	const RakNet::RakString& GetPlayerName()
	{
		return m_playerName;
	}

	const char* GetPlayerNameAsCString()
	{
		return m_playerName.C_String();
	}

	const RakNet::RakString& GetDeviceId()
	{
		return m_deviceId;
	}

	const SystemAddress& GetAddress()
	{
		return m_address;
	}
	
	bool CanFight()
	{
		return IsFighting() && m_fighter.CanFight();
	}

	bool IsReadyToShot()
	{
		return m_fighter.IsReadyToShot();
	}

	bool IsPaused()
	{
		return m_isPaused;
	}

	void ShotComplete()
	{
		m_fighter.ShotComplete();
	}

	void MakeFight(CClient& candidate)
	{
		if(m_fighter.MakeFight(candidate.m_fighter))
		{
			m_punchCount++;
			LT_DoLog2("Player %s crashed player %s", 
				GetPlayerNameAsCString(), candidate.GetPlayerNameAsCString());
		}
	}

	void Init(SystemAddress address, RakNet::RakString deviceId, RakNet::RakString playerName, int index)
	{
		m_index = index;
		m_playerName= playerName;
		m_address=address;
		m_state = JumpMode;
		m_deviceId = deviceId;

		m_jumper.Init(index);

		m_pingTime = 1;
		m_lastPingTime = RakNet::GetTime();
		m_isPingPaused = false;
		m_isPaused = false;

	}

	
	void SetPaused(bool isPaused)
	{
		bool isGamePaused = IsGamePaused();
		m_isPaused = isPaused;
		ProcessPauseSwitching(isGamePaused);
	}

	void Update(float accelX)
	{
		if (IsGamePaused())
			return;

		m_jumper.SetAccelerometerData(accelX);
	}

	
	void UpdateFighter(int moveLeft, int moveRight, int isShot)
	{
		if (IsGamePaused())
			return;

		m_fighter.Update(moveLeft, moveRight, isShot);
	}
	
	void ProcessFight()
	{
		if(IsFighting())
		{
			m_fighter.ProcessFight();
		}
	}

	void CheckShot()
	{
		if(IsFighting())
		{
			m_fighter.CheckShot();
		}
	}

	void Disconnect()
	{
		m_state = Disconnected;
		m_isPaused = false;
		m_platformTimer.Stop();
		m_jumper.Clear();
	}

	void RemoveClient() // removeClient
	{
		m_state = Empty;
		m_deviceId="";
		m_isPaused = false;
		m_isPingPaused=false;
		m_pingTime = 0;
		m_platformTimer.Reset();
		m_jumper.Clear();
		m_address = SystemAddress();
		m_index = -1;
		m_punchCount = 0;
		m_lastPingTime = RakNet::GetTime();
	}

	void StartGame()
	{
		m_state = JumpMode;
		m_jumper.SetInitialPosition();
		if (IsGamePaused())
		{
			m_jumper.SetFrozenMode();
		}
		else
		{
			m_jumper.SetProtectedMode();
		}
	}

	void StartJumping()
	{
		m_state = JumpMode;
		m_jumper.SetNormalState();
	}

	uint64_t GetPlatformTime()
	{
		return m_platformTimer.GetElapsed();
	}

	void ValidatePingData(int packetLoss, int pingTime)
	{
		packetLoss;//to suppress warning
		bool isGamePaused = IsGamePaused();
		m_pingTime = CalcMediumPingTime(pingTime);
		m_lastPingTime = RakNet::GetTime();
		m_isPingPaused = (m_pingTime > CConfiguration::GetInstance().GetLowPingTime());

		ProcessPauseSwitching(isGamePaused);
		LT_DoLog3("PING for %s - %i m(%i)", GetPlayerNameAsCString(), pingTime, m_pingTime);
	}

	void PackSimpleStatePacket(RakNet::BitStream& outStream)
	{
		if (-1 == m_index)
			LT_DoErr("PackSimpleStatePacket: Uninitialized client");

		outStream.Write(m_index);
		outStream.Write(m_playerName);
		outStream.Write(m_jumper.GetXPos());
		outStream.Write(m_jumper.GetYPos());
		outStream.Write(CHelper::ToClientTime(GetPlatformTime(), false));
	}

	bool PackStatePacket(RakNet::BitStream& outStream)
	{
		if (-1 == m_index)
			LT_DoErr("PackStatePacket: Uninitialized client");

		outStream.Write(m_index);
		bool packetMustBeSend = false;
		if (IsJumping())
		{
			packetMustBeSend = m_jumper.PackStatePacket(outStream);
		}
		else if(IsFighting())
		{
			packetMustBeSend = m_fighter.PackState(outStream, GetPlatformTime());
		}

		outStream.Write(m_pingTime);
		outStream.Write(CHelper::BoolToInt(IsGamePaused()));

		return packetMustBeSend;
	}

	void ProccessJump()
	{
		if (IsFighting())
		{
			if (m_fighter.NeedSwitchToJumping())
			{
				m_state = JumpMode;
				m_platformTimer.Stop();
				m_jumper.OnStopFight(
					GameConstants::RecalcPosFightToJump(m_fighter.GetXPos()),
					m_fighter.Direction(), m_fighter.IsCrash());

				m_fighter.Clear();
			}
		}
		else if (IsJumping())
		{
			m_jumper.ProccessJump(IsGamePaused());
		}
	}

	bool PreprocessState()
	{
		bool hasKeyPoint = false;
		if (IsActive() && !IsGamePaused())
		{
			uint64_t deltaPingTime = RakNet::GetTime() - m_lastPingTime;
			int pingPeriod = CConfiguration::GetInstance().GetPingPeriod();
			if ( (deltaPingTime > pingPeriod) &&
				( CalcMediumPingTime((int)(deltaPingTime - pingPeriod)) >
				CConfiguration::GetInstance().GetLowPingTime() ))
			{
				bool prevGamePause = IsGamePaused();
				m_isPingPaused = true;
				ProcessPauseSwitching(prevGamePause);
			}
		}

		if (IsJumping())
		{
			m_jumper.ProcessIntermediateState();

			if (m_jumper.NeedSwitchToFighting())
			{
				m_fighter.Init(
					GameConstants::RecalcPosJumpToFight(m_jumper.GetXPos()), m_jumper.Direction());
				m_state = FightMode;
				m_platformTimer.Start();

				//m_clients[i].StartFight();
				LT_DoLog1("New fighter %s", GetPlayerNameAsCString());
				hasKeyPoint = true;
			}
		}
		return hasKeyPoint;
	}


	bool CheckCollision(CClient& candidate)
	{
		return m_jumper.CheckCollision(candidate.m_jumper);
	}
	
	bool IsGamePauseElapsed()
	{
		return IsGamePaused() && m_pauseTimer.IsElapsed();
	}

	private:
	
	bool IsGamePaused()
	{
		return m_isPingPaused || m_isPaused;
	}
	
	void ProcessPauseSwitching(bool prevGamePause)
	{
		if (!prevGamePause && IsGamePaused())
		{
			m_pauseTimer.Init();
		}
	}

	int CalcMediumPingTime(int pingTime) const
	{
		return (m_pingTime * 5 + pingTime)/6;
	}
};

