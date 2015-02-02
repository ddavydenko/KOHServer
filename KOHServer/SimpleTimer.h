#pragma once
#include "./Raknet/GetTime.h"

class CSimpleTimer
{
	RakNetTime m_startTime;
	int m_period;
public:
	CSimpleTimer(int period)
	{
		m_period = period;
		Init();
	}
	
	uint64_t GetElapsed()
	{
		return RakNet::GetTime() - m_startTime;
	}

	bool IsElapsed()
	{
		return GetElapsed() > m_period;
	}
	
	int GetRestTime()
	{
		int restTime = m_period - (int)GetElapsed();
		if (restTime<0)
		{
			restTime = 0;
		}
		return restTime;
	}

	void Init()
	{
		m_startTime = RakNet::GetTime();
	}

	void Init(int period)
	{
		m_period = period;
		m_startTime = RakNet::GetTime();
	}
};
