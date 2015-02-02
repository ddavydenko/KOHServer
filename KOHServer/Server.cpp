#include "Server.h"
#include "Logger.h"
#include "./Raknet/RakNetworkFactory.h"
#include "./Raknet/MessageIdentifiers.h"
#include "./Raknet/GetTime.h"

#include <stdlib.h>

#include "configuration.h"

CServer::CServer(void)
{
	m_server = NULL;
}


CServer::~CServer(void)
{
	if (NULL != m_server)
	{
		RakNetworkFactory::DestroyRakPeerInterface(m_server);
		LT_DoLog("CServer::~CServer");
	}
}

void CServer::Start(unsigned short maxConnection)
{
	m_server=RakNetworkFactory::GetRakPeerInterface();
	m_server->SetIncomingPassword("Rumpelstiltskin", (int)strlen("Rumpelstiltskin"));
	m_server->SetTimeoutTime(CConfiguration::GetInstance().GetDisconnectTimeout(),UNASSIGNED_SYSTEM_ADDRESS);

	//	clientID=UNASSIGNED_SYSTEM_ADDRESS;
	LT_DoLog("Starting server.");
	unsigned short port = 60200;
	SocketDescriptor socketDescriptor(port,0);
	if (m_server->Startup(maxConnection, 25, &socketDescriptor, 1 ))
	{
		LT_DoLog("Server started, waiting for connections.");
	}
	else
	{ 
		LT_DoLog("Server failed to start.  Terminating.");
		exit(1);
	}
	m_server->SetMaximumIncomingConnections(maxConnection);
	m_server->SetOccasionalPing(false);
	m_server->SetUnreliableTimeout(1000);
	DataStructures::List<RakNetSmartPtr<RakNetSocket> > sockets;
	m_server->GetSockets(sockets);
	LT_DoLog("Ports used by RakNet:");
	for (unsigned int i=0; i < sockets.Size(); i++)
	{
		LT_DoLog2("%i. %i", i+1, sockets[i]->boundAddress.port);
	}

	LT_DoLog1("Server local IP is %s", m_server->GetLocalIP(0));
	LT_DoLog1("Server GUID is %s", m_server->GetGuidFromSystemAddress(UNASSIGNED_SYSTEM_ADDRESS).ToString());
}


unsigned char CServer::GetPacketIdentifier(Packet *p)
{
	if (p==0)
		return 255;

	if ((unsigned char)p->data[0] == ID_TIMESTAMP)
	{
		RakAssert(p->length > sizeof(unsigned char) + sizeof(unsigned long));
		return (unsigned char) p->data[sizeof(unsigned char) + sizeof(unsigned long)];
	}
	else
		return (unsigned char) p->data[0];
}

void CServer::ProccessPong(IMessageReceiver& receiver, Packet* packet)
{
	RakNet::BitStream pong( packet->data+1, sizeof(RakNetTime), false);
	RakNetTime time;
	pong.Read(time);
	int pingTime = (int)(RakNet::GetTime()-time);

	int packetloss = 0;
	RakNetStatistics *statistics = m_server->GetStatistics(packet->systemAddress);
	if (NULL != statistics)
	{
		packetloss = (int)statistics->packetloss;
	}
	else
	{
		LT_DoErr("ProccessPong: statistics is NULL");
	}
	receiver.OnPong(packet->systemAddress, packetloss, pingTime);
}

void CServer::ListenIncomingPackets(IMessageReceiver& receiver)
{
	for (Packet* p=m_server->Receive(); p; m_server->DeallocatePacket(p), p = m_server->Receive())
	{
		// We got a packet, get the identifier with our handy function
		unsigned char packetIdentifier = GetPacketIdentifier(p);

		// Check if this is a network message packet
		switch (packetIdentifier)
		{
		case ID_DISCONNECTION_NOTIFICATION: // Connection lost normally
			LT_DoLog1("ID_DISCONNECTION_NOTIFICATION from %s", p->systemAddress.ToString(true));
			receiver.OnConnectionLost(p->systemAddress);
			break;
		case ID_NEW_INCOMING_CONNECTION: // Somebody connected.  We have their IP now
			LT_DoLog2("ID_NEW_INCOMING_CONNECTION from %s with GUID %s", p->systemAddress.ToString(true), p->guid.ToString());
			break;
		case ID_INCOMPATIBLE_PROTOCOL_VERSION:
			LT_DoLog("ID_INCOMPATIBLE_PROTOCOL_VERSION");
			break;
		case ID_MODIFIED_PACKET: // Cheater!
			LT_DoLog("ID_MODIFIED_PACKET");
			break;
		case ID_DETECT_LOST_CONNECTIONS:
			LT_DoLog("Another client has lost the connection.");
			break;
		case ID_CONNECTION_LOST: // Couldn't deliver a reliable packet - i.e. the other system was abnormally terminated
			LT_DoLog1("ID_CONNECTION_LOST from %s", p->systemAddress.ToString(true));;
			receiver.OnConnectionLost(p->systemAddress);
			break;
		case ID_PONG:
			ProccessPong(receiver, p);
			break;
		default:
			RakNet::BitStream inStream(p->data, p->length, false); 

			unsigned char pkgId;
			inStream.Read(pkgId);

			receiver.OnCustomMessage(p->systemAddress, pkgId, inStream);
			break;
		}
	}
}
