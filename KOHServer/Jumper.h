#pragma once
#include "helper.h"
#include "GameConstants.h"

class CJumper
{
	enum EJumpState { jsNormal = 0, jsProtected, jsFalling, jsFrozen, jsCollision};
	enum ECollisionState {csNone = 0, csCollisionWinner = 1, csCollisionLoser = 2};
	EDirection m_direction;

	float m_accelX;
	EDirection m_collisionDirection;
	float m_collisionRotation;
	ECollisionState m_collisionState;
	
	float m_velY;
	float m_velX;
	CSimpleTimer m_intermediateStateTimer;
	bool m_inIntermediateState;

	EJumpState m_jumpState;
	bool m_isFallen;

	float m_prevMaxPosY;
	float m_posX;
	float m_posY;
	int m_index;
public:
	CJumper()
		: m_intermediateStateTimer(1000)
	{
		Clear();
	}
	
	void Init(int index)
	{
		m_index = index;
		SetInitialPosition();
		//m_posX = GameConstants::GetInitialXPos(index, numClients);
		//m_posY = GameConstants::GetInitialYPos();
		m_prevMaxPosY = m_posY;

		m_velY = GameConstants::JumpVelocity();// posY;
		SetInterMediateState(jsProtected);
		m_direction = dRight;
		m_velX=0.f;
	}

	void Clear() // removeClient
	{
		m_index = 0;
		m_accelX=0;
		m_posX=0;
		m_posY=0;
		m_prevMaxPosY = 0;
		m_velX=0.f;
		m_velY=0;
		m_jumpState = jsNormal;
		m_direction = dLeft;
		m_collisionDirection = dLeft;
		m_collisionRotation = 0.f;
		m_collisionState = csNone;
		m_isFallen = false;
	}

	bool PackStatePacket(RakNet::BitStream& outStream)
	{
		//LT_DoLog2("Jumper.Pack x=%f y=%f", m_posX, m_posY);
		//LT_DoLog2("Jumper.Pack vx=%f vy=%f", GetXVelocity(), GetYVelocity());
		outStream.Write((int)GetDirection());

		outStream.Write(m_posX);
		outStream.Write(m_posY);
		outStream.Write(GetXVelocity());
		outStream.Write(GetYVelocity());
		outStream.Write(GetXAccel());
		outStream.Write(GetYAccel());

		outStream.Write(GetRotation());
		outStream.Write(GetRotationVelocity());

		outStream.Write(CHelper::BoolToInt((jsFrozen == m_jumpState) && m_isFallen));

		outStream.Write((int)m_collisionState);
		bool packetMustBeSend = csNone != m_collisionState;
		m_collisionState = csNone;
		return packetMustBeSend;
	}

	void SetAccelerometerData(float data)
	{
		m_accelX = data;
	}

	//todo:remove
	float GetXPos()
	{
		return m_posX;
	}
	
	EDirection Direction()
	{
		return m_direction;
	}

	float GetYPos()
	{
		return m_posY;
	}

	bool NeedSwitchToFighting()
	{
		return (jsNormal == m_jumpState)
			&& GameConstants::IsOnJumpPlatform(m_posX)
			&& (m_posY >= GameConstants::GetPlatformYPosition());
	}

	void SetNormalState()
	{
		m_jumpState = jsNormal;
		m_velX = 0.f;
		m_inIntermediateState=false;
	}
	
	bool IsInNormalState()
	{
		return jsNormal == m_jumpState;
	}

	void SetProtectedMode()
	{
		SetInterMediateState(jsProtected);
	}

	void SetFrozenMode()
	{
		SetInterMediateState(jsFrozen);
	}

	void OnStopFight(float posX, EDirection direction, bool wasCrashed)
	{
		m_direction = direction;
		m_posX = posX;
		m_posY = GameConstants::GetPlatformYPosition();
		m_velY = wasCrashed ? -250.f : 0.f;
		m_velX = wasCrashed ? ((posX > GameConstants::HalfDeviceWidth) ? 30.f : -30.f) : 0.f;
		m_prevMaxPosY = m_posY;
		SetInterMediateState(jsFalling);
	}
	void ProccessJump(bool isGamePaused)
	{
		if(jsFrozen == m_jumpState)
		{
			if (isGamePaused)
			{
				SetInterMediateState(jsFrozen);
				m_isFallen = false;
			}
			return;
		}
		m_isFallen = false;

		const float corelDT = GameConstants::GetDeltaTime();

		if (jsCollision == m_jumpState)
		{
			m_collisionRotation += GetRotationVelocity() * corelDT;
			if ((m_collisionRotation > 360.f) || (m_collisionRotation < -360.f))
			{
				m_collisionRotation = 0.f;
				m_jumpState = jsFalling;
				LT_DoLog("Rotation complete");
			}
		}

		m_velY += GetYAccel() * corelDT;
		float prevPosY = m_posY;
		m_posY += GetYVelocity()*corelDT;

		m_velX += GetXAccel() * corelDT;
		m_posX += GetXVelocity()*corelDT;


		if(m_posX <= GameConstants::MinX)
		{
			m_posX = GameConstants::MaxX-5.;
		}
		else if(m_posX >= GameConstants::MaxX)
		{
			m_posX = GameConstants::MinX+5.;
		}

		if (prevPosY >= m_posY)
		{
			float nearestPosition = //m_posY;
				GetNearestPlatformPosition(m_posX, prevPosY);
			if (nearestPosition >= m_posY)
			{
				m_posY = nearestPosition;
				m_velY = GameConstants::JumpVelocity();
				m_prevMaxPosY = m_posY;
				if (m_posY < 0)
				{
					SetInitialPosition();
					SetInterMediateState(jsFrozen);
					m_isFallen = false;
					LT_DoLog("Switch to frozen at initial position");
				}
				else if (jsFalling == m_jumpState || jsCollision == m_jumpState || isGamePaused)
				{
					SetInterMediateState(jsFrozen);
					m_isFallen = !isGamePaused;
					LT_DoLog("Switch to frozen");
				}
			}
			else if ( (jsNormal == m_jumpState) && (m_prevMaxPosY - m_posY) > GameConstants::MaxHeightForJump)
			{
				m_jumpState = jsFalling;
				LT_DoLog("Switch to folling");
			}
		}
		else
		{
			m_prevMaxPosY = m_posY;
		}
	}

	void ProcessIntermediateState()
	{
		if (m_inIntermediateState)
		{
			if(m_intermediateStateTimer.IsElapsed())
			{
				SetNormalState();
			}
		}
	}

	bool CheckCollision(CJumper& candidate)
	{
		float checkerBottom = GameConstants::GetUnitBottom(m_posY);
		float checkerLeft = GameConstants::GetUnitLeft(m_posX);
		float checkerRight = GameConstants::GetUnitRight(m_posX);

		float candidateTop = GameConstants::GetUnitTop(candidate.m_posY);
		float candidateBottom = GameConstants::GetUnitBottom(candidate.m_posY);

		if(checkerBottom < candidateTop && checkerBottom > candidateBottom)
		{
			float candidateLeft = GameConstants::GetUnitLeft(candidate.m_posX);
			float candidateRight = GameConstants::GetUnitRight(candidate.m_posX);
			if(checkerRight >= candidateLeft && checkerRight < candidateRight)
			{
				m_collisionState = csCollisionWinner;
				candidate.SetCollisionState(dRight);
				return true;
			}
			else if(checkerLeft<=candidateRight && checkerLeft>candidateLeft)
			{
				m_collisionState = csCollisionWinner;
				candidate.SetCollisionState(dLeft);
				return true;
			}
		}
		return false;
	}

	void SetInitialPosition()
	{
		GetPosOnPlatform(m_index, m_posX, m_posY);
	}
	private:

	EDirection GetDirection()
	{
		float xVelocity = GetXVelocity();
		if ((0.f != xVelocity) && (jsCollision != m_jumpState))
		{
			m_direction = (xVelocity > 0) ? dRight : dLeft;
		}
		return m_direction;
	}

	float GetXAccel()
	{
		if (jsFalling == m_jumpState)
		{
			return (m_velX / -2.f) * GameConstants::GetDeltaTime();
		}
		return 0.f;
	}

	float GetXVelocity()
	{
		switch(m_jumpState)
		{
		case jsNormal:
		case jsProtected:
			return m_accelX;
		case jsCollision:
			return m_collisionDirection == dLeft
				? -GameConstants::CollisionXVel()
				: +GameConstants::CollisionXVel();
		case jsFalling:
			return m_velX;
			break;
		case jsFrozen:
		default:
			break;
		}
		return 0.;
	}

	float GetRotation()
	{
		switch(m_jumpState)
		{
		case jsCollision:
			return m_collisionRotation;
		case jsFalling:
			return 0.f;
		}
		return 0.f;
	}

	float GetRotationVelocity()
	{
		if (jsCollision == m_jumpState)
		{
			return m_collisionDirection == dLeft 
				? -GameConstants::CollisionRotationVel()
				: +GameConstants::CollisionRotationVel();
		}
		return 0.f;
	}

	float GetYVelocity()
	{
		if (jsFrozen == m_jumpState)
			return 0.f;

		if (m_velY <= GameConstants::MinYVelocity())
		{
			return GameConstants::MinYVelocity();
		}
		return m_velY;
	}

	float GetYAccel()
	{
		if (jsFrozen == m_jumpState)
			return 0.f;

		if (m_velY <= GameConstants::MinYVelocity())
		{
			return 0.f;
		}
		return GameConstants::CorelY();
	}

	void SetCollisionState(EDirection direction)
	{
		m_collisionState = csCollisionLoser;
		m_jumpState = jsCollision;
		m_collisionDirection = direction;
		m_collisionRotation = 0.f;
		m_velY = GameConstants::VelYAfterCollision();
	}

	void SetInterMediateState(EJumpState js)
	{
		m_inIntermediateState = true;
		m_intermediateStateTimer.Init(1000);
		m_jumpState = js;
	}
};
