// KOHServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <assert.h>
#include "application.h"
#include "Logger.h"
#include "EnsureCleanup.h"

//RAKNET_THREADSAFE in RakNetDefines.h

class CServiceStatus : SERVICE_STATUS
{
	SERVICE_STATUS_HANDLE m_hStatus;
	CRITICAL_SECTION m_cs;
	CEnsureCloseHandle m_hEvent;
	CEnsureCloseHandle m_hTerminateEvent;
public:
	CServiceStatus()
	{
		ZeroMemory(this, sizeof(SERVICE_STATUS));
		m_hStatus = NULL;

		InitializeCriticalSection(&m_cs);

		dwServiceType = SERVICE_WIN32;
		dwCurrentState = SERVICE_START_PENDING;
		dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
		dwWin32ExitCode = 0;
		dwServiceSpecificExitCode = 0;
		dwCheckPoint = 0;
		dwWaitHint = 0;
		m_hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		m_hTerminateEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	}

	HANDLE GetEvent()
	{
		return m_hEvent;
	}
	HANDLE GetTerminateEvent()
	{
		return m_hTerminateEvent;
	}

	bool Register(PCTSTR szServiceName, LPHANDLER_FUNCTION pfnHandler)
	{
		m_hStatus = RegisterServiceCtrlHandler(szServiceName, pfnHandler);
		return (m_hStatus != (SERVICE_STATUS_HANDLE)0);
	}

	void Set(bool isRunning)
	{
		if (!isRunning)
		{
			SetEvent(m_hEvent);
			if (WAIT_TIMEOUT == WaitForSingleObject(m_hTerminateEvent, 20 * 1000))
			{
				LT_DoErr("Terminating service - timeout occured");
			}
		}
		EnterCriticalSection(&m_cs);
		dwCurrentState = isRunning ? SERVICE_RUNNING : SERVICE_STOPPED;
		SetServiceStatus(m_hStatus, this);
		LeaveCriticalSection(&m_cs);
	}
};
TCHAR g_szServiceName[] = TEXT("King of the hill");
CServiceStatus g_serviceStatus;

void __stdcall ControlHandler(DWORD request) 
{
	switch(request)
	{
		case SERVICE_CONTROL_STOP:
		case SERVICE_CONTROL_SHUTDOWN:
			LT_DoWar("Service stopped");
			g_serviceStatus.Set(false);
		return; 
	}

	g_serviceStatus.Set(true);

	return;
}

void __stdcall ServiceMain(DWORD /*dwNumServicesArgs*/, LPSTR* /*lpServiceArgVectors*/)
{
	LT_DoLog("ServiceMain");

	if (g_serviceStatus.Register(g_szServiceName, ControlHandler))
	{
		LT_DoWar("Service started");
	}
	else
	{
		LT_DoWar("Service was not started");
		return;
	}


	g_serviceStatus.Set(true);
	
	{
		CApplication app;
		app.Start(g_serviceStatus.GetEvent());
	}

	SetEvent(g_serviceStatus.GetTerminateEvent());
	LT_DoWar("Service thread terminated");

	return; 
}

/////////////////////////////////////////////////////////////////////////////////////////////
void InstallService() 
{
	LT_DoLog("InstallService");
	// Open the SCM on this machine.
	CEnsureCloseServiceHandle hSCM = 
		OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);

	// Get our full pathname
	TCHAR szModulePathname[_MAX_PATH * 2];
	GetModuleFileName(NULL, szModulePathname, chDIMOF(szModulePathname));

	// Append the switch that causes the process to run as a service.
	lstrcat(szModulePathname, TEXT(" /service"));   

	// Add this service to the SCM's database.
	CEnsureCloseServiceHandle hService = 
		CreateService(hSCM, g_szServiceName, g_szServiceName,
		SERVICE_CHANGE_CONFIG, SERVICE_WIN32_OWN_PROCESS, 
		SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE,
		szModulePathname, NULL, NULL, NULL, NULL, NULL);

	SERVICE_DESCRIPTION sd = { TEXT("King of the hill service") };
	ChangeServiceConfig2(hService, SERVICE_CONFIG_DESCRIPTION, &sd);
}

void RemoveService() 
{
	LT_DoLog("RemoveService");

	// Open the SCM on this machine.
	CEnsureCloseServiceHandle hSCM = 
		OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);

	// Open this service for DELETE access
	CEnsureCloseServiceHandle hService = 
		OpenService(hSCM, g_szServiceName, DELETE);

	// Remove this service from the SCM's database.
	DeleteService(hService);
}

/////////////////////////////////////////////////////////////////////////////////////////////
//int CrtReportHook(int reportType, char *message, int *returnValue)
//{
//	LT_DoErr(message);
//	return 0;
//}

int _tmain(int argc, _TCHAR* argv[])
{
	SetUnhandledExceptionFilter(ExceptionTracerFilter);

#ifdef _DEBUG
	//int tmpFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	//tmpFlag |= _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF; //_CRTDBG_CHECK_CRT_DF;
	//_CrtSetDbgFlag(tmpFlag);
	//_CrtSetReportHook(CrtReportHook);
#endif

	enum EStartType {eInstall, eRemove, eDebug, eService};
	EStartType type = eDebug;

	if (argc > 1)
	{
		for (int i = 1; i < argc; i++)
		{
			if ((argv[i][0] == TEXT('-')) || (argv[i][0] == TEXT('/')))
			{
				if (lstrcmpi(&argv[i][1], TEXT("install")) == 0) 
					type = eInstall;

				if (lstrcmpi(&argv[i][1], TEXT("remove"))  == 0)
					type = eRemove;

				if (lstrcmpi(&argv[i][1], TEXT("debug"))   == 0)
					type = eDebug;

				if (lstrcmpi(&argv[i][1], TEXT("service")) == 0)
					type = eService;
			}
		}
	}
	else
	{
		LT_DoWar("Accessible options are [/install] [/remove] [/debug] [/service] (debug is default)");
	}

	switch(type)
	{
		case eInstall:
			InstallService();
			break;
		case eRemove:
			RemoveService();
			break;
		case eDebug:
			{
				CApplication app;
				app.Start(NULL);
			}
			break;
		case eService:
			{
				SERVICE_TABLE_ENTRY serviceTable[] = { {g_szServiceName, ServiceMain}, {NULL, NULL} };
				StartServiceCtrlDispatcher(serviceTable);
			}
			break;
	}

	return 0;
}
