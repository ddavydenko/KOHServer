#pragma once
#include "logger.h"

enum EDirection {dRight = 0, dLeft = 1};

class GameConstants
{
	static float realDeltaTime;

	static const int FightPlatformLeftIndent = 52;
	static const int FightPlatformRightIndent = 57;
	static const int JumpPlatformLeftIndent = 55;
	static const int JumpPlatformRightIndent = 60;
	static float FightToJumpCoef()
	{
		static const float c = ((float)(DeviceWidth - (FightPlatformLeftIndent + FightPlatformRightIndent))) / 
			((float)(DeviceWidth - (JumpPlatformLeftIndent + JumpPlatformRightIndent)));
		return c;
	}
public:

	static float AdjustLeftGrain(int left) {return ((float)left) - (GameConstants::HalfUnitWidth() - 5.f);}
	static float AdjustRightGrain(int right) {return ((float)right) + (GameConstants::HalfUnitWidth() - 5.f);}

	static const int DeviceWidth = 480;
	static const int HalfDeviceWidth = DeviceWidth/2;

	static float RecalcPosJumpToFight(float jumpXpos)
	{
		return (jumpXpos - HalfDeviceWidth) * FightToJumpCoef() + HalfDeviceWidth;
	}
	
	static float RecalcPosFightToJump(float fightXPos)
	{
		return (fightXPos - HalfDeviceWidth) / FightToJumpCoef() + HalfDeviceWidth;
	}

	static float GetInitialXPos(int index, int numClients)
	{
		float delta = ((float)DeviceWidth)/(numClients + 1);
		return delta * (index + 1);
	}
	static bool IsOnFightPlatform(float posX)
	{
		return (posX > AdjustLeftGrain(FightPlatformLeftIndent) 
			&& posX < AdjustRightGrain(DeviceWidth - FightPlatformRightIndent));
	}

	static bool IsOnJumpPlatform(float posX)
	{
		return (posX > AdjustLeftGrain(JumpPlatformLeftIndent) 
			&& posX < AdjustRightGrain(DeviceWidth - JumpPlatformRightIndent));
	}

	static const int FightXShift = 128;

	static const int MinX = 0;
	static const int MaxX = DeviceWidth;
	static const int MaxHeightForJump = 180;
	
	static float GetPlatformYPosition(){ return 960.f - 85.f + HalfUnitHeight();}

	static float HalfUnitHeight() {return 17.f;}
	static float HalfUnitWidth() {return 16.f;}

	static float GetUnitBottom(float posY)
	{
		return posY - HalfUnitHeight();
	}
	static float GetUnitTop(float posY)
	{
		return posY + HalfUnitHeight();
	}
	static float GetUnitRight(float posX)
	{
		return posX + HalfUnitWidth();
	}
	static float GetUnitLeft(float posX)
	{
		return posX - HalfUnitWidth();
	}

	static float CorelY() {return -1050.0f;}//-1050
	static float MinYVelocity() {return -450.f;}//
	static float VelYAfterCollision() {return 280.f;}
	static float CollisionRotationVel() {return 125.f;}//-2.f/.03f
	static float CollisionXVel() {return 150.f;}
	static float FightXVelocity() {return 200.f;}

	static float GetDeltaTime() {return realDeltaTime;}

	static void SetDeltaTime(float delta)
	{
		realDeltaTime = delta/(1000.f * 1000.f);
	}

	static float JumpVelocity() {return 501.f;}//450
};
