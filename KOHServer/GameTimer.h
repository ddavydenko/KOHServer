#pragma once

class CGameTimer
{
	uint64_t m_startTime;
	uint64_t m_elapsed;
	bool m_isStarted;
public:
	CGameTimer()
	{
		m_startTime = 0;
		m_elapsed = 0;
		m_isStarted = false;
	}

	void Reset()
	{
		m_elapsed = 0;
		m_startTime = GetNow();
		m_isStarted = false;
	}

	void Start()
	{
		if (!m_isStarted)
		{
			m_startTime = GetNow();
			m_isStarted = true;
		}
	}
	
	void Stop()
	{
		if (m_isStarted)
		{
			m_elapsed += GetNow() - m_startTime;
			m_isStarted = false;
		}
	}
	
	uint64_t GetElapsed()//sec
	{
		uint64_t result = m_isStarted ? GetNow() - m_startTime : 0;
		result += m_elapsed;
		return result;
	}
	
	//void SetElapsed(long elapsed)
	//{
	//	m_elapsed = elapsed;
	//}
private:
	uint64_t GetNow()
	{
		return RakNet::GetTime();
	}
};

