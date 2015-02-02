#pragma once

class CHelper
{
public :
	static int ToClientTime(uint64_t serverTime, bool roundUp)
	{
		return (int)((serverTime + (roundUp ? 999 : 0) )/1000);
	}
	static int BoolToInt(bool b)
	{
		return b ? 1 : 0;
	}
};
