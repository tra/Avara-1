/*
    Copyright �1996, Juri Munkki
    All rights reserved.

    File: CUDPConnection.h
    Created: Monday, January 29, 1996, 13:35
    Modified: Saturday, August 3, 1996, 6:48
*/

#pragma once
#include "CDirectObject.h"
#include "CCommManager.h"

#define	ROUTER_CAPABLE

#define	kSerialNumberStepSize		2
#define	kNumReceivedOffsets			128

typedef struct	
{
	PacketInfo		packet;

	long			birthDate;
	long			nextSendTime;
	short			serialNumber;

}	UDPPacketInfo;

typedef struct
{
	long		host;
	short		port;
}	CompleteAddress;

typedef struct
{
	long		hostIP;
	
	long		estimatedRoundTrip;
	long		averageRoundTrip;
	long		pessimistRoundTrip;
	long		optimistRoundTrip;
	
	short		connectionType;
} UDPConnectionStatus;

#define	kDebugBufferSize		1024

#define	kPleaseSendAcknowledge	((UDPPacketInfo *)-1L)

enum	{	kReceiveQ, kTransmitQ, kBusyQ, kQueueCount	};

class	CUDPConnection : public CDirectObject
{
public:
	class	CUDPConnection	*next;
	class	CUDPComm		*itsOwner;
	
			QHdr			queues[kQueueCount];

			long			seed;

			long			ipAddr;
			short			port;
					
			short			myId;

			short			serialNumber;
			short			receiveSerial;
			short			maxValid;

			Boolean			haveToSendAck;
			long			nextAckTime;
			long			nextWriteTime;
			
			long			validTime;

			long			retransmitTime;
			long			roundTripTime;
			long			pessimistTime;
			long			optimistTime;
			long			realRoundTrip;
			long			deviation;
			
			long			quota;
			short			cramData;
			short			busyQLen;
			
			short			routingMask;

#if	DEBUG_AVARA
			short			dp;
			OSType			d[kDebugBufferSize];
#endif

volatile	short			*offsetBufferBusy;
			long			ackBitmap;
			short			ackBase;

			Boolean			killed;
	
	virtual	void			IUDPConnection(CUDPComm *theMaster);
	virtual	void			SendQueuePacket(UDPPacketInfo *thePacket, short theDistribution);
	virtual	void			RoutePacket(UDPPacketInfo *thePacket);
	virtual	UDPPacketInfo *	GetOutPacket(long curTime, long cramTime, long urgencyAdjust);
	virtual	UDPPacketInfo *	FindBestPacket(long curTime, long cramTime, long urgencyAdjust);
	virtual	void			ProcessBusyQueue(long curTime);

	virtual	char *			WriteAcks(char *dest);
	virtual	void			RunValidate();
	
	virtual	void			ValidatePacket(UDPPacketInfo *thePacket, long when);
	virtual	char *			ValidatePackets(char *validateInfo, long curTime);
	virtual	void			ReceivedPacket(UDPPacketInfo *thePacket);
	
	virtual	void			FlushQueues();
	virtual	void			Dispose();

	virtual	void			MarkOpenConnections(CompleteAddress *table);
	virtual	void			OpenNewConnections(CompleteAddress *table);
	
	virtual	Boolean			AreYouDone();
	
	virtual	void			FreshClient(long remoteHost, short remotePort, long firstReceiveSerial);

#if	DEBUG_AVARA
	virtual	void			DebugEvent(char eType, long pNumber);
#endif
	virtual	void			CloseSlot(short theId);
	virtual	void			ErrorKill();
	
	virtual	void			ReceiveControlPacket(PacketInfo *thePacket);
	
	virtual	void			GetConnectionStatus(short slot, UDPConnectionStatus *parms);
};