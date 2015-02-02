#pragma once

class PacketTypeId
{
public:
//	static const unsigned char DisconnectFromJumpMode=55;
//	static const unsigned char DOWN_ID=66;
//	static const unsigned char RESUME_ID=77;
	static const unsigned char GAME_STATE_ID=88;
	static const unsigned char GAME_CONTROLS=99;
//	static const unsigned char ConnectToJumpScene = 100;//FIRST_CONNECTION_ID=100;
//	static const unsigned char FDOWN_ID=22;
//	static const unsigned char FIRST_FIGHT_CONNECTION_ID=33;
	static const unsigned char FIGHT_ID=44;

	static const unsigned char RoomsInfo = 101;
	static const unsigned char ConnectToRoom = 102;

	static const unsigned char WaitingForPlayers = 103;
	static const unsigned char GameOver = 104;
	static const unsigned char CountdownToGame = 105;
	static const unsigned char PlayerDisconnected = 106;
	static const unsigned char PauseSignal = 107;
};
