#include "Clouds.h"
#include "GameConstants.h"
//////////////////////////////// chDIMOF Macro ////////////////////////////////

// This macro evaluates to the number of elements in an array. 
#define chDIMOF(Array) (sizeof(Array) / sizeof(Array[0]))

// This macro evaluates to the number of elements in an array. 
#define chDIMOFS(Array) ((sizeof(Array) / sizeof(Array[0])) - 1)

struct CCloud
{
	float left, right;
	float level;

	CCloud(float leftGranX, float rightGranX, float granY)
	{
		left = GameConstants::AdjustLeftGrain(leftGranX);
		right = GameConstants::AdjustRightGrain(rightGranX);

		level = granY;
	}

	void GetPosOnPlatform(float& x, float& y)
	{
		y = this->level + GameConstants::HalfUnitHeight();
		x = this->left + (this->right - this->left)/2.f;
	}
};

CCloud g_clouds[] = {
	  CCloud( 15.5, 104.5, 67.5)	//0
	, CCloud(142.5, 203.5, 67.5)	//1
	, CCloud(254.5, 364.5, 67.5)	//2
	, CCloud(387.5, 477.5, 67.5)	//3
	, CCloud(301.5, 356.5, 151.5)	//4
	, CCloud( 72.5, 125.5, 162.5)	//5
	, CCloud(349.5, 404.5, 216.5)	//6
	, CCloud(151.5, 264.5, 250.5)	//7
	, CCloud(238.5, 294.5, 334.5)	//8
	, CCloud(127.5, 185.5, 405.5)	//9
	, CCloud(198.5, 256.5, 464.5)	//10
	, CCloud(311.5, 364.5, 541.5)	//11
	, CCloud( 83.5, 137.5, 539.5)	//12
	, CCloud(182.5, 235.5, 606.5)	//13
	, CCloud(214.5, 268.5, 710.5)	//14
	, CCloud( 85.5, 141.5, 768.5)	//15
	, CCloud(272.5, 328.5, 797.5)	//16
};

void GetPosOnPlatform(int index, float& x, float& y)
{
	g_clouds[index].GetPosOnPlatform(x,y);
}

float GetNearestPlatformPosition(float posX, float posY)
{
	float result = -100.f;
	float unitBottom = GameConstants::GetUnitBottom(posY);
	float bestDif = unitBottom - result;
	for (int i = 0; i != chDIMOF(g_clouds); ++i)
	{
		if (posX >= g_clouds[i].left && posX <= g_clouds[i].right)
		{
			float platformY = g_clouds[i].level;
			if (unitBottom >= platformY)
			{
				float dif = unitBottom - platformY;
				if (dif < bestDif)
				{
					bestDif = dif;
					result = platformY;
				}
			}
		}
	}
	return result + GameConstants::HalfUnitHeight();
}