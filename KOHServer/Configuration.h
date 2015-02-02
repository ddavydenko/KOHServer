#pragma once
#include "stdafx.h"
#include <windows.h>
#include <string>

#include "logger.h"

class CConfiguration
{
	std::string m_sIniFileName;
	static bool m_isInited;
	static CConfiguration m_instance;

	int m_numberOfRooms;
	int m_lowPingTime;
	int m_disconnectTimeout;
	int m_pauseTimeout;
	CConfiguration(void);

public:

	static const CConfiguration& GetInstance()
	{
		if (!m_isInited)
		{
			m_instance.Init();
		}
		return m_instance;
	}

	int GetPingPeriod() const
	{
		return 500;
	}


	int GetPauseTimeout() const
	{
		return m_pauseTimeout;
	}

	int GetLowPingTime() const
	{
		return m_lowPingTime;
	}

	int GetDisconnectTimeout() const
	{
		return m_disconnectTimeout;
	}

	int GetNumberOfPlayersInRoom(int index) const
	{
		return ReadIntFromRoom(index, _T("NumberOfPlayers"));
	}

	int GetRoomTime(int index) const
	{
		return ReadIntFromRoom(index, _T("TimeSec"));
	}

	int GetNumberOfRooms() const
	{
		return m_numberOfRooms;
	}
private:
	void Init()
	{
		SetIniFileName();
		m_numberOfRooms = ReadIntFromSettings("NumberOfRooms", 0);
		if (m_numberOfRooms <= 0)
		{
			LT_DoErr("NumberOfRooms not specified or <= 0");
			exit(-1);
		}

		m_lowPingTime = ReadIntFromSettings(_T("LowPingTimeMs"), 150);
		m_disconnectTimeout = ReadIntFromSettings(_T("DisconnectTimeoutMs"), 10 * 1000);
		m_pauseTimeout = ReadIntFromSettings(_T("PauseTimeoutMs"), 20 * 1000);

	}

	int ReadIntFromRoom(int roomIndex, LPCTSTR szKey) const
	{
		TCHAR roomName[256] = {0};
		sprintf(roomName, "room_%i", roomIndex);
		int result = ReadInt(roomName, szKey, -1);
		if (result == -1)
		{
			LT_DoErr2("ReadIntFromRoom %s, %s", roomName, szKey);
			exit(-1);
		}
		return result;
	}

	int ReadIntFromSettings(LPCTSTR szKey, int iDefaultValue) const
	{
		return ReadInt(_T("Settings"), szKey, iDefaultValue);
	}

	int ReadInt(LPCTSTR szSection, LPCTSTR szKey, int iDefaultValue) const
	{
		return GetPrivateProfileInt(szSection,  szKey, iDefaultValue, m_sIniFileName.c_str());
	}

	void SetIniFileName()
	{
		TCHAR lpIniFileName[MAX_PATH + 1] = {0};
		TCHAR thisModulePath[MAX_PATH] = {0};
		GetModuleFileName(NULL, thisModulePath, MAX_PATH); 

		LPTSTR filePart = NULL;
		GetFullPathName(thisModulePath, MAX_PATH + 1, lpIniFileName, &filePart);
		*filePart = 0;
		wsprintf(filePart, _T("settings.ini"));

		m_sIniFileName = lpIniFileName;
	}
};
