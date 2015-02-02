#pragma once
#include <windows.h>

enum LMType {
	lmtError	= 0x1,
	lmtWarning	= 0x2,
	lmtMessage	= 0x4,
	lmtStartLog = 0x8,
	lmtEndLog	= 0x10,
	lmtException= 0x20
//	lmtSaveModuleList = 0x40
};

__declspec(naked) inline DWORD_PTR LT_GetAddress()
{
	__asm mov eax, [esp];
	__asm ret;
}

void LoggerTrace(DWORD_PTR dwAddress, LMType mType, LPCTSTR lpszFormat, ...);

#define LT_DoLogEx(type, message)					{ LoggerTrace(LT_GetAddress(), type, message); }
#define LT_DoLog1Ex(type, message, arg1)			{ LoggerTrace(LT_GetAddress(), type, message, arg1); }
#define LT_DoLog2Ex(type, message, arg1, arg2)		{ LoggerTrace(LT_GetAddress(), type, message, arg1, arg2); }
#define LT_DoLog3Ex(type, message, arg1, arg2, arg3)	{ LoggerTrace(LT_GetAddress(), type, message, arg1, arg2, arg3); }


#define LT_AssertTyped(f, m_type, message)					{if( !(f) ) LT_DoLogEx(m_type, message) }
#define LT_AssertTyped1(f, m_type, message, arg1)			{if( !(f) ) LT_DoLog1Ex(m_type, message, arg1) }
#define LT_AssertTyped2(f, m_type, message, arg1, arg2)		{if( !(f) ) LT_DoLog2Ex(m_type, message, arg1, arg2) }

#define LT_Assert(f, message)					LT_AssertTyped(f, lmtError, message)
#define LT_Assert1(f, message, arg1)			LT_AssertTyped1(f, lmtError, message, arg1)
#define LT_Assert2(f, message, arg1, arg2)		LT_AssertTyped2(f, lmtError, message, arg1, arg2)

#define LT_DoLog(message)					LT_DoLogEx(lmtMessage, message)
#define LT_DoLog1(message, arg1)			LT_DoLog1Ex(lmtMessage, message, arg1)
#define LT_DoLog2(message, arg1, arg2)		LT_DoLog2Ex(lmtMessage, message, arg1, arg2)
#define LT_DoLog3(message, arg1, arg2, arg3)	LT_DoLog3Ex(lmtMessage, message, arg1, arg2, arg3)

#define LT_DoErr(message)					LT_DoLogEx(lmtError, message)
#define LT_DoErr1(message, arg1)			LT_DoLog1Ex(lmtError, message, arg1)
#define LT_DoErr2(message, arg1, arg2)		LT_DoLog2Ex(lmtError, message, arg1, arg2)

#define LT_DoWar(message)					LT_DoLogEx(lmtWarning, message)

LONG __stdcall ExceptionTracerFilter(LPEXCEPTION_POINTERS e);

