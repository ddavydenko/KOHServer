#pragma once
#include "./Raknet/RakString.h"
#include "./Raknet/BitStream.h"

#include "helper.h"
#include "Logger.h"
#include "GameConstants.h"
#include "client.h"

class CFighter 
{
	enum EMovingState { msStop = 0, msPressedFirstStepDoing, msUnpressedFirstStepDoing, msMoving, msSleepAfterFlight};
	enum EMove {mNone = 0, mLeft, mRight};
	enum EShotInfo {siNone = 0, siCrash = 1, siFightComplete = 2, siHit = 4};

	EDirection m_direction;
	EShotInfo m_shotInfo;
	EMove m_move;

	EDirection m_crashDirection;

	float m_posX;
	float m_posY;

	bool m_isReadyToShot;

	bool m_sleepAfterCrash;
	CSimpleTimer m_sleepAfterCrashTimer;

	bool m_isPrepareShot;
	CSimpleTimer m_prepareToShotTimer;

	bool m_isDownFromPlatform;


	EMovingState m_movingState;
	float m_movementDestinationPosition;
	CSimpleTimer m_sleepAfterFlightTimer;

public:

	CFighter()
		: m_prepareToShotTimer(200)
		, m_sleepAfterCrashTimer(500)
		, m_sleepAfterFlightTimer(2100)
	{
		Clear();
	}

	void Init(float posX, EDirection direction)
	{
		m_direction = direction;
		m_posX = posX;
		m_posY = GameConstants::GetPlatformYPosition();
		m_isDownFromPlatform = false;
		m_isReadyToShot=false;
		m_move = mNone;
		m_sleepAfterCrash = false;
		m_movingState = msStop;
		SetShotInfo(siNone);
		m_crashDirection = dLeft;
	}
	void Clear()
	{
		m_posX=0;
		m_posY=0;
		m_isPrepareShot=false;
		m_isReadyToShot=false;
		m_isDownFromPlatform = false;
		m_move = mNone;
		m_sleepAfterCrash = false;
		m_movingState = msStop;
		SetShotInfo(siNone);
		m_crashDirection = dLeft;
		
	}

	bool IsReadyToShot()
	{
		return m_isReadyToShot;
	}

	void ShotComplete()
	{
		SetShotInfo(siFightComplete);
		m_isReadyToShot = false;
		m_posX += (dLeft == m_direction) ? -10.f : +10.f;
		CheckPosX();
	}

	bool CanFight()
	{
		return !m_sleepAfterCrash && msSleepAfterFlight != m_movingState;
	}

	float GetXPos()
	{
		return m_posX;
	}
	EDirection Direction()
	{
		return m_direction;
	}

	bool NeedSwitchToJumping()
	{
		if (m_isDownFromPlatform)
		{
			return m_sleepAfterCrash ? m_sleepAfterCrashTimer.IsElapsed() : true;
		}
		return false;
	}

	bool IsCrash()
	{
		return m_sleepAfterCrash;
	}

	void PackFightFirstConnectionSignalPacket(RakNet::BitStream& outStream, int playerId, RakNet::RakString playerName)
	{
		outStream.Write(playerId);
		outStream.Write(m_posX);
		outStream.Write(m_posY);
		outStream.Write(playerName);
	}

	
	void Update(int _moveLeft, int _moveRight, int isShot)
	{
		if (m_sleepAfterCrash || msSleepAfterFlight == m_movingState)
			return;

		m_move = ((0 == _moveLeft) && (0 == _moveRight)) ? mNone :
			( (1 == _moveLeft) ? mLeft : mRight);

		switch(m_movingState)
		{
		case msStop:
			if(mNone != m_move)
			{
				SwitchToPressedFirstStepDoing();
			}
			break;
		case msPressedFirstStepDoing:
			if (IsMoveKeyUnpressed())
			{
				m_movingState = msUnpressedFirstStepDoing;
			}
			break;
		case msUnpressedFirstStepDoing:
			if (IsMoveKeyPressed())
			{
				SwitchToPressedFirstStepDoing();
			}
			break;
		case msMoving:
			if (IsMoveKeyUnpressed())
			{
				m_movingState = msStop;
			}
			break;
		}

		if ((1 == isShot) && !m_isPrepareShot)
		{
			m_movingState = msStop;
			m_isPrepareShot=true;
			m_prepareToShotTimer.Init();
		}
	}

	void ProcessFight()
	{
		if (m_sleepAfterCrash)
			return;

		switch(m_movingState)
		{
		case msStop:
			//do nothing
			break;
		case msSleepAfterFlight:
			if (m_sleepAfterFlightTimer.IsElapsed())
			{
				m_movingState = msStop;
			}
			break;
		case msPressedFirstStepDoing:
			if (MakeMovement(true))
			{
				m_movingState = msMoving;
			}
			break;
		case msUnpressedFirstStepDoing:
			if (MakeMovement(true))
			{
				m_movingState = msStop;
			}
			break;
		case msMoving:
			MakeMovement(false);
			break;
		}
	}

	void CheckShot()
	{
		if (m_isPrepareShot)
		{
			LT_DoLog("CheckShot - Preparing");
			if (m_prepareToShotTimer.IsElapsed())
			{
				m_isPrepareShot = false;
				m_isReadyToShot = true;
				LT_DoLog("CheckShot - End prepare");
			}
		}
	}

	bool PackState(RakNet::BitStream& outStream, uint64_t platformTime)
	{
		bool mustBeSend = siNone != m_shotInfo;
		outStream.Write(m_posX);
		outStream.Write(m_posY);
		outStream.Write(CHelper::BoolToInt(m_isPrepareShot));
		outStream.Write((int)m_shotInfo);
		outStream.Write((int)m_direction);
		outStream.Write((int)m_crashDirection);
		outStream.Write(CHelper::ToClientTime(platformTime, false));
		SetShotInfo(siNone);
		return mustBeSend;
	}

	bool MakeFight(CFighter& fCandidate)
	{
		//todo: m_fighters[j].isReadyToShot ?
		if(!IsFightComplete(fCandidate.m_posX))
			return false;

		SetShotInfo(siHit);
		fCandidate.SetShotInfo(siCrash);
		fCandidate.m_crashDirection = m_direction;
		if(dLeft == fCandidate.m_crashDirection)
		{
			fCandidate.m_posX -= GameConstants::FightXShift;
		}
		else
		{
			fCandidate.m_posX += GameConstants::FightXShift;
		}
		fCandidate.CheckPosX();
		
		fCandidate.m_isPrepareShot = false;
		fCandidate.m_isReadyToShot = false;

		if (fCandidate.m_isDownFromPlatform)
		{
			fCandidate.m_sleepAfterCrash = true;
			fCandidate.m_sleepAfterCrashTimer.Init();
		}
		else
		{
			fCandidate.m_movingState = msSleepAfterFlight;
			fCandidate.m_sleepAfterFlightTimer.Init();
		}
		
		return fCandidate.m_isDownFromPlatform;
	}

	private:

	void SetShotInfo(EShotInfo shotInfo)
	{
		m_shotInfo = (siNone == shotInfo) ? siNone : (EShotInfo)(m_shotInfo | shotInfo);
	}

	void CheckPosX()
	{
		static const float delta = 10.0;
		static const float fminX = GameConstants::MinX + delta;
		static const float fmaxX = GameConstants::MaxX - delta;

		if(m_posX <= fminX)
		{
			m_posX = fminX;
		}
		else if(m_posX >= fmaxX)
		{
			m_posX = fmaxX;
		}

		if(!GameConstants::IsOnFightPlatform(m_posX))
		{
			m_isDownFromPlatform = true;
			LT_DoLog("m_isDownFromPlatform = true");
		}
	}


	bool IsFightComplete(float candidatePosX)
	{
		float unitWidth = GameConstants::HalfUnitWidth() * 2;

		float dx = (dLeft == m_direction) ? m_posX - candidatePosX : candidatePosX - m_posX;
		return (10 < dx) && (dx < unitWidth + 15);
	}

		bool IsMoveKeyPressed()
	{
		return ((mLeft == m_move) && (dLeft == m_direction))
				|| ((mRight == m_move) && (dRight == m_direction));
	}

	bool IsMoveKeyUnpressed()
	{
		return ((mLeft != m_move) && (dLeft == m_direction))
				|| ((mRight != m_move) && (dRight == m_direction));
	}
	void SwitchToPressedFirstStepDoing()
	{
		m_direction = (mLeft == m_move) ? dLeft : dRight;
		m_movementDestinationPosition = (dRight == m_direction) 
			? m_posX + 30.1f : m_posX - 30.1f;
		m_movingState = msPressedFirstStepDoing;
	}

	bool MakeMovement(bool checkDestinationPosition)
	{
		bool isTerminated = false;
		float shift = GameConstants::FightXVelocity() * GameConstants::GetDeltaTime();
		float coef = (dRight == m_direction) ? +1.f : -1.f;
		m_posX += coef*shift;
		if (checkDestinationPosition)
		{
			if ( ((dRight == m_direction) && (m_posX >= m_movementDestinationPosition))
				|| ((dLeft == m_direction) && (m_posX <= m_movementDestinationPosition)))
			{
				m_posX = m_movementDestinationPosition;
				isTerminated = true;
			}
		}
		CheckPosX();
		LT_DoLog1("MakeMovement pos=%f", m_posX); 
		return isTerminated;
	}

};