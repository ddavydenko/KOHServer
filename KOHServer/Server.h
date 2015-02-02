#pragma once
#include "./Raknet/RakPeerInterface.h"
#include "./Raknet/RakNetStatistics.h"
#include "./Raknet/BitStream.h"

class IMessageReceiver
{
public:
	virtual void OnConnectionLost(const SystemAddress& address) = 0;
	virtual void OnPong(const SystemAddress& address, int packetLoss, int pingTime)=0;
	virtual void OnCustomMessage(const SystemAddress& address, unsigned char pkgId, RakNet::BitStream& inputStream)=0;
};

class CServer
{
	RakPeerInterface *m_server;

public:
	CServer(void);
	~CServer(void);

	void Start(unsigned short maxConnection);

	void Ping(SystemAddress address)
	{
		RakNet::RakString clientHost = address.ToString(false);
		unsigned short clientPort = address.port;
		m_server->Ping(clientHost, clientPort, false);
	}

	//void Send(RakNet::BitStream& outStream)
	//{
	//	m_server->Send(&outStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, UNASSIGNED_SYSTEM_ADDRESS, true);
	//}

	void Send(RakNet::BitStream& outStream, const SystemAddress& address, bool reliable)
	{
//		reliable = true;
		PacketReliability reability = reliable ? RELIABLE_ORDERED : UNRELIABLE_SEQUENCED;
		m_server->Send(&outStream, HIGH_PRIORITY, reability, 0, AddressOrGUID(address), false);
	}

	void ListenIncomingPackets(IMessageReceiver& receiver);

private:
	unsigned char GetPacketIdentifier(Packet *p);
	void ProccessPong(IMessageReceiver& receiver, Packet* packet);
};

