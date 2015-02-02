#include <stdio.h>
#include "logger.h"
#include <windows.h>
#include <tchar.h>
#include "ensureCleanup.h"

int mTypeMask = lmtError | lmtWarning 
#if defined(_DEBUG)
| lmtMessage 
#endif
| lmtStartLog | lmtEndLog;

TCHAR g_logFileName[MAX_PATH+1] = {0};

LPCTSTR GetFullLogFileName()
{
	if (g_logFileName[0] == 0)
	{
		TCHAR thisModulePath[MAX_PATH] = {0};
		GetModuleFileName(NULL, thisModulePath, MAX_PATH); 
		LPTSTR filePart = NULL;

		GetFullPathName(thisModulePath, MAX_PATH + 1, g_logFileName, &filePart);
		*filePart = 0;
		SYSTEMTIME st= {0};
		GetSystemTime(&st);
		wsprintf(filePart, _T("KOTH_%02hu_%02hu_%02hu.log"), st.wYear, st.wMonth, st.wDay);
	}
	return g_logFileName;
}

int LoggerPutHeader( LPTSTR lpBuffer, LMType mType )
{
	SYSTEMTIME sysTime;
	GetLocalTime( &sysTime);

	LPTSTR lpMessageType =
					( mType & lmtError ) !=0
					? "E"//"ERROR"
					: ( mType & lmtWarning ) !=0
						? "W"//"WARNING"
						: ( mType & lmtMessage ) !=0
							? "M"//"MESSAGE"
							: ( mType & lmtException) !=0
								? "EXCEPTION"
								: ( mType & lmtStartLog) !=0
									? "START_LOGGING"
									: ( mType & lmtEndLog) !=0
										? "END_LOGING"
										: "UNCKNOWN";
	return wsprintf(lpBuffer,
//				"\r\n%08X\t"
//				"%08X\t"
				"%02hu.%02hu.%02hu"	//Дата
				" %02hu:%02hu:%02hu"	//Время
				".%02hu"					//Миллисекунды
				"\t%s\t",					//Тип сообщения
//				GetCurrentProcessId(),
//				GetCurrentThreadId(),
				sysTime.wDay, sysTime.wMonth, sysTime.wYear,
				sysTime.wHour, sysTime.wMinute, sysTime.wSecond
				,sysTime.wMilliseconds
				,lpMessageType
				);
}
static DWORD dwWritedMessageCount = 0;
void WriteLog(LPCTSTR lpBuffer, int iLen)
{
	//Open log file for append
	HANDLE hFile;
	int iCount=0;
	while( (hFile = CreateFile(GetFullLogFileName(), GENERIC_WRITE, FILE_SHARE_READ, NULL,
								OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE
			&& iCount++ < 100)
		if( GetLastError() != ERROR_SHARING_VIOLATION) break;
		else	Sleep(10);

	if( hFile != INVALID_HANDLE_VALUE)
	{
		DWORD dwWritten;
		SetFilePointer( hFile, 0, NULL, FILE_END);

		if(dwWritedMessageCount == 0)
		{	//Write "Start logging" message
			TCHAR chString[MAX_PATH];
			iCount = LoggerPutHeader( chString, lmtStartLog);
			WriteFile( hFile, chString, (DWORD) iCount, &dwWritten, NULL);
			LPCTSTR nl = _T("\n");
			iCount = sizeof(TCHAR) * strlen(nl);
			WriteFile( hFile, nl, (DWORD) iCount, &dwWritten, NULL);
		}

		// Write log record
		WriteFile(hFile, lpBuffer, (DWORD)iLen, &dwWritten, NULL);
		dwWritedMessageCount++;
		CloseHandle(hFile);
	}
}

void LoggerTraceV(DWORD_PTR dwAddress, LMType mType, LPCTSTR lpszFormat, va_list args)
{
	//Check, if that message would be logged
	if( ((mType & mTypeMask) == 0) &&
		((mType & lmtEndLog) == 0))		//EndLogging message must be written
		return;
	if( dwAddress == 0)
		__asm{
			mov EAX, dword ptr [EBP+4];
			mov	dword ptr dwAddress, EAX
		}
	static const int iMaxLogLineSize = 1024;
	TCHAR buffer[iMaxLogLineSize] = {0};
	int iLen = LoggerPutHeader(buffer, mType);
	//iLen += wsprintf(buffer + iLen, "0x%08X\t", dwAddress);
	iLen += vsnprintf(buffer + iLen, iMaxLogLineSize - iLen - 4, lpszFormat, args);
	iLen += wsprintf(buffer + iLen, "\n", dwAddress);
#ifdef _DEBUG
	{
//		OutputDebugString(buffer);
		printf(buffer);
	}
#endif
	WriteLog(buffer, iLen);
}

void LoggerTrace(DWORD_PTR dwAddress, LMType mType, LPCTSTR lpszFormat, ...)
{
	va_list args;
	va_start(args, lpszFormat);
	if( dwAddress == 0)
	{
		__asm
		{
			mov EAX, dword ptr [EBP+4];
			mov	dword ptr dwAddress, EAX
		}
	}
	LoggerTraceV(dwAddress, mType, lpszFormat, args);
	va_end(args);
}

//

#include <DbgHelp.h>
#include <sstream>
#pragma comment(lib, "dbghelp.lib")
#include <vector>
#include <map>
struct FunctionCall 
{
	std::string FunctionName;
	std::string FileName;
	int			LineNumber;
};
std::vector<FunctionCall> g_vecCallStack;
typedef std::map<DWORD,const char*> CodeDescMap;
CodeDescMap g_mapCodeDesc;
DWORD g_dwMachineType; // Machine type matters when trace the call stack (StackWalk64)
DWORD g_dwExceptionCode;

#define CODE_DESCR(code) CodeDescMap::value_type(code, #code)

void InitStackTracer(void)
{
	// Get machine type
	g_dwMachineType = 0;
    TCHAR* wszProcessor = ::_tgetenv(_T("PROCESSOR_ARCHITECTURE"));
	if (wszProcessor)
	{
		if ((!_tcscmp(_T("EM64T"), wszProcessor)) ||!_tcscmp(_T("AMD64"), wszProcessor))
		{
			g_dwMachineType = IMAGE_FILE_MACHINE_AMD64;
		}
		else if (!_tcscmp(_T("x86"), wszProcessor))
		{
			g_dwMachineType = IMAGE_FILE_MACHINE_I386;
		}
	}

	// Exception code description
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_ACCESS_VIOLATION));       
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_DATATYPE_MISALIGNMENT));  
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_BREAKPOINT));             
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_SINGLE_STEP));            
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_ARRAY_BOUNDS_EXCEEDED));  
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_FLT_DENORMAL_OPERAND));   
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_FLT_DIVIDE_BY_ZERO));     
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_FLT_INEXACT_RESULT));     
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_FLT_INVALID_OPERATION));  
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_FLT_OVERFLOW));           
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_FLT_STACK_CHECK));        
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_FLT_UNDERFLOW));          
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_INT_DIVIDE_BY_ZERO));     
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_INT_OVERFLOW));           
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_PRIV_INSTRUCTION));       
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_IN_PAGE_ERROR));          
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_ILLEGAL_INSTRUCTION));    
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_NONCONTINUABLE_EXCEPTION));
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_STACK_OVERFLOW));         
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_INVALID_DISPOSITION));    
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_GUARD_PAGE));             
	g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_INVALID_HANDLE));         
	//g_mapCodeDesc.insert(CODE_DESCR(EXCEPTION_POSSIBLE_DEADLOCK));      
}

std::string GetExceptionMsg()
{
	std::ostringstream  m_ostringstream;

	// Exception Code
	CodeDescMap::iterator itc = g_mapCodeDesc.find(g_dwExceptionCode);
	
	m_ostringstream<<"------------------------------------------------------------------\r\n";
	if(itc != g_mapCodeDesc.end())
		m_ostringstream << "Exception: " << itc->second << "\r\n";
	else
		m_ostringstream << "Unknown Exception...\r\n";


	// Call Stack
	std::vector<FunctionCall>::iterator itbegin = g_vecCallStack.begin();
	std::vector<FunctionCall>::iterator itend = g_vecCallStack.end();
	std::vector<FunctionCall>::iterator it;
	for (it = itbegin; it < itend; it++)
	{
		std::string strFunctionName = it->FunctionName.empty() ? "UnkownFunction" : it->FunctionName;
		std::string strFileName = it->FileName.empty() ? "UnkownFile" : it->FileName;

		m_ostringstream << strFunctionName << " [" << strFileName << " , " << it->LineNumber << "]\r\n"; 
	}
	m_ostringstream<<"------------------------------------------------------------------\r\n";

	return m_ostringstream.str();
}

enum {CALLSTACK_DEPTH = 100};
void TraceCallStack(CONTEXT* pContext)
{
	InitStackTracer();
	// Initialize stack frame
	STACKFRAME64 sf;
	memset(&sf, 0, sizeof(STACKFRAME));

#if defined(_WIN64)
	sf.AddrPC.Offset = pContext->Rip;
	sf.AddrStack.Offset = pContext->Rsp;
	sf.AddrFrame.Offset = pContext->Rbp;
#elif defined(WIN32)
	sf.AddrPC.Offset = pContext->Eip;
	sf.AddrStack.Offset = pContext->Esp;
	sf.AddrFrame.Offset = pContext->Ebp;
#endif
	sf.AddrPC.Mode = AddrModeFlat;
	sf.AddrStack.Mode = AddrModeFlat;
	sf.AddrFrame.Mode = AddrModeFlat;

	if (0 == g_dwMachineType)
		return;

	// Walk through the stack frames.
	HANDLE hProcess = GetCurrentProcess();
	HANDLE hThread = GetCurrentThread();
	while(StackWalk64(g_dwMachineType, hProcess, hThread, &sf, pContext, 0, SymFunctionTableAccess64, SymGetModuleBase64, 0))
	{
		if( sf.AddrFrame.Offset == 0 || g_vecCallStack.size() >= CALLSTACK_DEPTH)
			break;

		// 1. Get function name at the address
		const int nBuffSize = (sizeof(SYMBOL_INFO) + MAX_SYM_NAME*sizeof(TCHAR) + sizeof(ULONG64) - 1) / sizeof(ULONG64);
		ULONG64 symbolBuffer[nBuffSize];
		PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)symbolBuffer;

		pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		pSymbol->MaxNameLen = MAX_SYM_NAME;

		FunctionCall curCall = {"", "", 0};

		DWORD64 dwSymDisplacement = 0;
		if(SymFromAddr( hProcess, sf.AddrPC.Offset, &dwSymDisplacement, pSymbol ))
		{
			curCall.FunctionName = std::string(pSymbol->Name);
		}

		//2. get line and file name at the address
		IMAGEHLP_LINE64 lineInfo = { sizeof(IMAGEHLP_LINE64) };
		DWORD dwLineDisplacement = 0;

		if(SymGetLineFromAddr64(hProcess, sf.AddrPC.Offset, &dwLineDisplacement, &lineInfo ))
		{
			curCall.FileName = std::string(lineInfo.FileName);
			curCall.LineNumber = lineInfo.LineNumber;
		}

		// Call stack stored
		g_vecCallStack.push_back(curCall);
	}
}

LONG __stdcall ExceptionTracerFilter(LPEXCEPTION_POINTERS e)
{
	g_dwExceptionCode = e->ExceptionRecord->ExceptionCode;
	g_vecCallStack.clear();

	HANDLE hProcess = INVALID_HANDLE_VALUE;

	// Initializes the symbol handler
	if(!SymInitialize( GetCurrentProcess(), NULL, TRUE ))
	{
		SymCleanup(hProcess);
		return EXCEPTION_EXECUTE_HANDLER;
	}

	// Work through the call stack upwards.
	TraceCallStack( e->ContextRecord);

	// ...
	SymCleanup(hProcess);
	
	std::string message = GetExceptionMsg();
	WriteLog(message.c_str(), message.length() * sizeof(TCHAR));
	LT_DoErr("Exception");
	return(EXCEPTION_EXECUTE_HANDLER) ;
}

