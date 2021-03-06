/*
    Copyright �1996, Juri Munkki
    All rights reserved.

    File: CUDPComm.h
    Created: Monday, January 29, 1996, 13:32
    Modified: Saturday, August 31, 1996, 21:34
*/

#pragma once
#include "CCommManager.h"
#include "MacTCPLib.h"

#define	ROUTER_CAPABLE

#define	UDPSTREAMBUFFERSIZE			16384
#define	UDPSENDBUFFERSIZE			1024
#define	CRAMTIME					5000	//	About 20 seconds.
#define	CRAMPACKSIZE				64
#define	kClientConnectTimeoutTicks	(60*30)

enum	{	udpCramInfo	};	//	Selectors for kpPacketProtocolControl packer p1 params.

class	CUDPComm : public CCommManager
{
public:
			Ptr					ourA5;

			long				seed;
			short				softwareVersion;
			short				maxClients;
			short				clientLimit;
			Str255				password;

			UDPiopb				sendPB;
			UDPiopb				receivePB;
			UDPiopb				rejectPB;
			char				rejectData[32];
			wdsEntry			rejectDataTable[2];

			UDPIOCompletionUPP	readComplete;
			UDPIOCompletionUPP	writeComplete;
			UDPIOCompletionUPP	bufferReturnComplete;

			ReceiverRecord		receiverRecord;
			
			OSErr				writeErr;
			OSErr				readErr;

	class	CTagBase			*prefs;
	class	CUDPConnection		*connections;
	class	CUDPConnection		*nextSender;
	class	CTracker			*tracker;
	
			long				retransmitToRoundTripRatio;	//	In fixed point 24.8 format

			long				nextWriteTime;
			long				latencyConvert;
			long				urgentResendTime;
			long				lastClock;
			long				lastQuotaTime;

			long				localIP;	//	Just a guess, but that's all we need for the tracker.
			short				localPort;
			StreamPtr			stream;
			char				streamBuffer[UDPSTREAMBUFFERSIZE];
			char				sendBuffer[UDPSENDBUFFERSIZE];
			wdsEntry			writeDataTable[2];
			
	volatile	
			short				*isSending;
			
			short				cramData;

	volatile OSErr				rejectReason;
	volatile Boolean			clientReady;
			Boolean				isConnected;
			Boolean				isServing;
			Boolean				isClosed;
			Boolean				isClosing;
			Boolean				turboMode;

			Boolean				readIsComplete;
			Boolean				writeIsComplete;
			Boolean				blockReadComplete;
			Boolean				blockWriteComplete;
			Boolean				specialWakeup;
			Str255				inviteString;

	virtual	void		IUDPComm(short clientCount, short bufferCount, short version, long urgentTimePeriod);
	virtual	OSErr		AllocatePacketBuffers(short numPackets);
	virtual	void		Disconnect();
	virtual	void		WritePrefs();
	virtual	void		Dispose();
	
			long		GetClock();
	
	virtual	void		ReadComplete();
	virtual	void		WriteComplete();

	virtual	void		WriteAndSignPacket(PacketInfo *thePacket);
	virtual	void		ForwardPacket(PacketInfo *thePacket);
	virtual	void		ProcessQueue();
	
	virtual	void		SendConnectionTable();
	virtual	void		ReadFromTOC(PacketInfo *thePacket);
	
	virtual	void		SendRejectPacket(ip_addr remoteHost, short remotePort, OSErr loginErr);

	virtual	CUDPConnection * DoLogin(PacketInfo *thePacket);

	virtual	Boolean		PacketHandler(PacketInfo *thePacket);
	
	virtual	OSErr		AsyncRead();
	virtual	Boolean		AsyncWrite();
	
	virtual	void		ReceivedGoodPacket(PacketInfo *thePacket);
	
	virtual	OSErr		CreateStream(short streamPort);
	
	virtual	void		CreateServer();
	virtual	OSErr		ContactServer(ip_addr serverHost, short serverPort);

	virtual	Boolean		ServerSetupDialog(Boolean disableSome);
	
	virtual	void		Connect();				//	Client
	virtual	void		StartServing();			//	Server.
	
	virtual	Boolean		DoHotPop(Rect *popRect, StringPtr hostName, short *hostPort, OSType listTag);

	virtual	void		OptionCommand(long theCommand);
	virtual	void		DisconnectSlot(short slotId);

	virtual	short		GetStatusInfo(short slotId, Handle leftColumn, Handle rightColumn);

	virtual	Boolean		ReconfigureAvailable();
	virtual	void		Reconfigure();
	virtual	long		GetMaxRoundTrip(short distribution);

	virtual	void		BuildServerTags();
};