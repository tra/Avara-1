/*
    Copyright �1996, Juri Munkki
    All rights reserved.

    File: CTracker.c
    Created: Friday, April 19, 1996, 18:52
    Modified: Sunday, September 1, 1996, 11:26
*/

#include "CTracker.h"
#include "CTagBase.h"
#include "AvaraTCP.h"
#include "CUDPComm.h"
#include "UDPTags.h"
#include <OSUtils.h>
#include "math.h"

#ifdef	 LONE_TRACKER
#include "CEventHandler.h"
#define	kMaxAvaraPlayers	6
#else
#include "CNetManager.h"
#include "CAvaraApp.h"
#endif

#include "RAMFiles.h"
#include "JAMUtil.h"
#include "CharWordLongPointer.h"

#define	kTrackerMinRegPeriod	2000	//	Eight seconds (or so)
#define	kTrackerMaxRegPeriod	60000L	//	Four minutes (or so)
#define	kUDPTrackerDialogId		415

#define	kTrackerStatusStrings	420
#define	kTrackerLeftTemplate	420
#define	kTrackerRightTemplate	421
#define	kTrackerHelpResource	422
#define	kTabRightTrackerInfo	48

enum
{
	kStatIdle = 1,
	kStatLookup,
	kStatSending,
	kStatWaiting,
	kStatReceiving,
	kStatWriteFailed,
	kStatLookupFailed,
	kStatNotActive,
	kStatFirstNetStat
};

enum
{	kCloseItem = 1,
	kQueryItem,
	kSelectItem,
	kSelectIPItem,
	kTrackerNameItem,
	kTrackerPopItem,
	kInputItem,
	kResultListItem,

	kTrackerStatusItem,
	kTrackerInfoUserBox
};

static
short	StripPortNumber(
	StringPtr	host,
	short		defaultPort)
{
	short			newPort = 0;
	short			mult = 1;
	short			i;
	unsigned char	theChar = 0;
	
	for(i = host[0];i;i--)
	{	
		theChar = host[i];
		if(theChar >= '0' && theChar <= '9')
		{	newPort += mult * (theChar - '0');
			mult *= 10;
		}
		else
			break;
	}
	
	if(theChar == ':' && newPort)
	{	host[0] = i-1;
		return newPort;
	}
	else
		return defaultPort;
}

OSErr	CTracker::PrepareToSend()
{
	Str255		trackerHostName;
	short		err;
	ip_addr		theAddr;
	short		portNumber;
	
	isPrepared = true;
	prefs->ReadString(kUDPTrackerNameTag, trackerHostName);
	
	portNumber = StripPortNumber(trackerHostName, kTrackerPortNumber);
	err = PascalStringToAddress(trackerHostName, &theAddr);
	
	if(err == noErr)
	{	sendPB.udpStream = stream;
		sendPB.ioCRefNum = gMacTCP;
		sendPB.csParam.send.reserved = 0;
		sendPB.csParam.send.checkSum = true;
		sendPB.csParam.send.wdsPtr = writeDataTable;
		sendPB.csParam.send.userDataPtr = (Ptr) this;
		
		sendPB.csParam.send.remoteHost = theAddr;
		sendPB.csParam.send.remotePort = portNumber;
		sendPB.csParam.send.localPort = localPort;
	}
	else
	{	CloseStream();
	}
	
	return err;
}

void	CTracker::SendTrackerInfo(Boolean sayGoodBye)
{
	while(sendPB.ioResult == 1);	//	While we are busy sending...

	if(!isPrepared)
	{	PrepareToSend();
	}

	HUnlock(trackerDataHandle);
	BuildServerTags();
	WriteCharTag(ktsCommand, sayGoodBye ? ktcGoodbye : ktcHello);
	HLock(trackerDataHandle);

	writeDataTable[0].ptr = *trackerDataHandle;
	writeDataTable[0].length = logicalSize;
	writeDataTable[1].ptr = NULL;
	writeDataTable[1].length = 0;

	UDPWrite(&sendPB, true);
}

void	CTracker::WakeUp()
{
	if(stream && sendPB.ioResult != 1)
	{	SendTrackerInfo(false);
		wakeUp = owner->lastClock + sleepPeriod;
		sleepPeriod += sleepPeriod;
		if(sleepPeriod > kTrackerMaxRegPeriod)
		{	sleepPeriod = kTrackerMaxRegPeriod;
		}
	}
}

void	CTracker::OpenStream()
{
	UDPiopb		param;
	OSErr		err;

	if(OpenAvaraTCP() == noErr)
	{	param.ioCRefNum = gMacTCP;
		param.csParam.create.rcvBuff = streamBuffer;
		param.csParam.create.rcvBuffLen = TRACKERSTREAMBUFFERSIZE;
		param.csParam.create.notifyProc = NULL;
		param.csParam.create.localPort = 0;			//	Anything goes.
		param.csParam.create.userDataPtr = (Ptr) this;
		
		err = UDPCreate(&param,false); // create the stream...
		
		localPort = param.csParam.create.localPort;
		
		if(err)	stream = 0;
		else	stream = param.udpStream;
	}
}

void	CTracker::CloseStream()
{
	if(stream)
	{	UDPiopb			param;

		while(sendPB.ioResult == 1);	//	While we are busy sending...

		param.ioCRefNum = gMacTCP;
		param.udpStream = stream;
		UDPRelease(&param, false);
		stream = 0;
		sendPB.ioResult = noErr;
	}
}

void	CTracker::ITracker(
	CUDPComm	*theComm,
	CTagBase	*thePrefs)
{
	MachineLocation		myLocation;

	prefs = thePrefs;
	owner = theComm;
	stream = 0;
	sendPB.ioResult = noErr;
	
	wakeUp = owner->lastClock;
	sleepPeriod = kTrackerMinRegPeriod;
	
	trackerDataHandle = NewHandle(512);
	logicalSize = 0;
	realSize = GetHandleSize(trackerDataHandle);
	
	ReadLocation(&myLocation);
	globalLocation.h = myLocation.longitude >> 16;
	globalLocation.v = myLocation.latitude >> 16;
	
	isTracking = false;
	isPrepared = false;
}

void	CTracker::Dispose()
{
	if(trackerDataHandle)
	{	DisposeHandle(trackerDataHandle);
		trackerDataHandle = NULL;
		realSize = 0;
		logicalSize = 0;
	}

	if(stream)
	{	CloseStream();
	}
	inherited::Dispose();
}

static
pascal
void	NullRectProc(
	GrafVerb verb,
	Rect *r)
{

}

void	CTracker::DrawInfoBox(
	DialogPtr	itsWindow,
	Rect		*theRect)
{
	long			len;
	CQDProcs		myQDProcs;
	CQDProcs		*savedProcs;
	TextSettings	nText;
	RgnHandle		theClip;

	theClip = NewRgn();
	GetClip(theClip);


	FrameRect(theRect);
	InsetRect(theRect, 4, 4);
	EraseRect(theRect);
	ClipRect(theRect);

	GetSetTextSettings(&nText, geneva, 0, 9, srcOr);

	SetStdCProcs(&myQDProcs);
	myQDProcs.rectProc = (void *)NullRectProc;
	savedProcs = (void *)itsWindow->grafProcs;
	itsWindow->grafProcs = (void *)&myQDProcs;

	len = GetHandleSize(infoLeftHandle);
	if(len)
	{	HLock(infoLeftHandle);
		TextBox(*infoLeftHandle, len, theRect, teForceLeft);
		HUnlock(infoLeftHandle);
	}
	
	len = GetHandleSize(infoRightHandle);
	if(len)
	{	theRect->left += kTabRightTrackerInfo;
		theRect->right += 2048;
		HLock(infoRightHandle);
		TextBox(*infoRightHandle, len, theRect, teForceLeft);
		HUnlock(infoRightHandle);
	}

	itsWindow->grafProcs = (void *)savedProcs;
	RestoreTextSettings(&nText);

	SetClip(theClip);
	DisposeRgn(theClip);
}

void	CTracker::ListClicked()
{
	Point			aCell;
	char			oldState;
	Ptr				theData;
	short			offset;
	short			len;
	charWordLongP	p;
	char			findStr[] = { '�', 0, '�', 13 };
	char			zaps[] = "SVAdscm123456i";
	char			*zapper;
	Str255			tempStr;
	short			slotMap = 0;
	short			i;
	Boolean			hadSelected;

	aCell.h = 0;
	aCell.v = 0;
	hadSelected = LGetSelect(true, &aCell, gameList);
	
	if(hadSelected == false)
	{	if(		GetHandleSize(infoLeftHandle)
			||	GetHandleSize(infoRightHandle))
		{	SetHandleSize(infoLeftHandle, 0);
			SetHandleSize(infoRightHandle, 0);
			InvalRect(&infoRect);
		}
	}
	else
	if(aCell.v != shownSelected)
	{	Handle	templateData;
	
		shownSelected = aCell.v;
	
		LFind(&offset, &len, aCell, gameList);
	 
		oldState = HGetState((*gameList)->cells);
		HLock((*gameList)->cells);
		theData = ((*(*gameList)->cells)+offset);
		
		templateData = GetResource('TEXT', kTrackerLeftTemplate);
		HLock(templateData);
		PtrToXHand(*templateData, infoLeftHandle, GetHandleSize(templateData));
		HUnlock(templateData);

		templateData = GetResource('TEXT', kTrackerRightTemplate);
		HLock(templateData);
		PtrToXHand(*templateData, infoRightHandle, GetHandleSize(templateData));
		HUnlock(templateData);
		
		p.c = theData;
		theData += len;
		
		while(p.c < theData)
		{	char			theTag;
			unsigned char	tagLen;
			
			theTag = *p.c++;
			tagLen = *p.uc++;
			if(theTag >= 0)
			{	switch(theTag)
				{	case ktsHostDomain:			findStr[1] = 'A';	goto leftMunger;
					case ktsInvitation:			findStr[1] = 'i';	goto leftMunger;
					
					leftMunger:
						Munger(infoLeftHandle, 0, findStr, 3, p.str, tagLen);
						break;
						
					case ktsSoftwareVersion:	findStr[1] = 'V';	goto rightMunger;
					case ktsLevelDirectory:		findStr[1] = 'c';	goto rightMunger;
					case ktsLevelName:			findStr[1] = 'm';	goto rightMunger;

					rightMunger:
						Munger(infoRightHandle, 0, findStr, 3, p.str, tagLen);
						break;

					case ktsGameStatus:
						GetIndString(tempStr, kTrackerStatusStrings, kStatFirstNetStat + *p.uc);
						findStr[1] = 's';
						Munger(infoRightHandle, 0, findStr, 3, tempStr+1, tempStr[0]);
						break;
				}
			}
			else
			{	char	index;
			
				index = *p.c++;
				tagLen--;
				switch(-theTag)
				{	case kisPlayerIPPort:
						if(index == 0)
						{	AddrToStr(*p.l, (char *)tempStr+1);
							for(tempStr[0] = 0; tempStr[tempStr[0]+1]; tempStr[0]++);
							findStr[1] = 'A';
							Munger(infoRightHandle, 0, findStr, 3, tempStr+1, tempStr[0]);
						}
						break;
					case kisPlayerLives:
						if(*p.uc >= 0)	NumToString(*p.uc, tempStr);
						else			GetIndString(tempStr, kTrackerStatusStrings, kStatNotActive);
	
						findStr[1] = '1' + index;
						Munger(infoLeftHandle, 0, findStr, 3, tempStr+1, tempStr[0]);
						
						slotMap |= 1 << index;
						break;
					case kisPlayerNick:
						findStr[1] = '1' + index;
						Munger(infoRightHandle, 0, findStr, 3, p.str, tagLen);
						slotMap |= 1 << index;
						break;
					case kisPlayerLocation:
						if(index == 0)
						{	Point		serverLoc;
							double		ax, ay, az;
							double		bx, by, bz;
							double		longitude, latitude;
							
							serverLoc = *(Point *)p.l;
							
							longitude = serverLoc.h * (3.1415926 / 32768);
							latitude = serverLoc.v * (3.1415926 / 32768);
							ax = cos(latitude);
							ay = ax * cos(longitude);
							ax *= sin(longitude);
							az = sin(latitude);

							longitude = globalLocation.h * (3.1415926 / 32768);
							latitude = globalLocation.v * (3.1415926 / 32768);
							bx = cos(latitude);
							by = bx * cos(longitude);
							bx *= sin(longitude);
							bz = sin(latitude);
							
							ax *= bx;
							ay *= by;
							az *= bz;
							
							ax = acos(ax + ay + az);
							ax = ax * 40030.0 / 2*3.1415926;
							NumToString((long)ax, tempStr);
							findStr[1] = 'd';
							Munger(infoRightHandle, 0, findStr, 3, tempStr+1, tempStr[0]);
						}
						break;
				}
			}
			
			p.c += tagLen;
		}
		
		for(i=0;i<kMaxAvaraPlayers;i++)
		{	if(((1 << i) & slotMap) == 0)
			{	findStr[1] = '1' + i;
				Munger(infoLeftHandle, 0, findStr, 4, findStr, 0);
				Munger(infoRightHandle, 0, findStr, 4, findStr, 0);
			}
		}

		GetIndString(tempStr, kTrackerStatusStrings, kStatNotActive);
		for(zapper = zaps;*zapper;zapper++)
		{	findStr[1] = *zapper;
			Munger(infoLeftHandle, 0, findStr, 3, tempStr+1, tempStr[0]);
			Munger(infoRightHandle, 0, findStr, 3, tempStr+1, tempStr[0]);
		}
		
		HSetState(((*gameList)->cells), oldState);
		InvalRect(&infoRect);
	}
}

static	CTracker	*curTracker;
static	short		defaultButton;

pascal
Boolean	UDPTrackerDialogFilter(
	DialogPtr	theDialog,
	EventRecord	*theEvent,
	short		*itemHit)
{
	Rect	iRect;
	Handle	iHandle;
	short	iType;
	GrafPtr	saved;
	Boolean	didHandle = false;
	short	doHilite = 0;

	GetPort(&saved);
	SetPort(theDialog);

	switch(theEvent->what)
	{	case updateEvt:
			if(theDialog == (DialogPtr)theEvent->message)
			{	GetDItem(theDialog, defaultButton, &iType, &iHandle, &iRect);
				PenSize(3,3);
				InsetRect(&iRect, -4, -4);
				FrameRoundRect(&iRect, 16, 16);
				PenSize(1,1);
				
				GetDItem(theDialog, kTrackerInfoUserBox, &iType, &iHandle, &iRect);
				curTracker->DrawInfoBox(theDialog, &iRect);
			}
			else
			{	gApplication->ExternalEvent(theEvent);
			}
			break;
		case mouseDown:
			{	Point	where;

				where = theEvent->where;
				GlobalToLocal(&where);
				if(PtInRect(where, &curTracker->gameRect))
				{	didHandle = true;
				
					if(LClick(where, 0, curTracker->gameList))
					{
#ifndef LONE_TRACKER
						*itemHit = kSelectItem;
						doHilite = kSelectItem;
#endif
					}
					
					curTracker->ListClicked();
				}
			}
			break;
		case keyDown:
			{	char	theChar;
			
				theChar = theEvent->message;
				if(theChar == 13 || theChar == 3)
				{	*itemHit = defaultButton;
					didHandle = true;
					doHilite = defaultButton;
				}
				else if(theChar == 27 ||
#ifdef LONE_TRACKER
					 ((theChar == 'Q' || theChar == 'q') && (theEvent->modifiers & cmdKey)) ||
#endif
					 (theChar == '.' && (theEvent->modifiers & cmdKey)))
				{	*itemHit = kCloseItem;
					doHilite = kCloseItem;
					didHandle = true;
				}
			}
			break;
		case nullEvent:
			if(curTracker->receivePB.ioResult == 0
				&& curTracker->stream)
			{	*itemHit = 9999;
				didHandle = true;
			}
			break;
	}
	
	if(doHilite)
	{	ControlHandle	theControl;
		long			finalTick;
	
		GetDItem(theDialog, doHilite, &iType, (Handle *)&theControl, &iRect);
		HiliteControl(theControl, 1);
		Delay(3, &finalTick);
		HiliteControl(theControl, 0);
	}

	SetPort(saved);
	return didHandle;
}

#define		HILITEMODE	LMSetHiliteMode(127 & LMGetHiliteMode())

static
void	DrawTrackerListItem(
	Rect	*theRect,
	Ptr		theData,
	short	len)
{
	TextSettings	sText;
	short			y;
	short			curPlayers = 0;
	charWordLongP	p;
	unsigned char	inInfo[] = {	5, '0', ' ', '/', ' ', '6', '+', 'P'	};
	
	theRect->top++;
	y = theRect->top + GetSetTextSettings(&sText, geneva, 0, 10, srcOr);
	
	theRect->left += 4;
	theRect->right -= 4;
	p.c = theData;
	theData += len;
	
	while(p.c < theData)
	{	char			theTag;
		unsigned char	tagLen;
		
		theTag = *p.c++;
		tagLen = *p.uc++;
		if(theTag >= 0)
		{	switch(theTag)
			{	case ktsHostDomain:
					MoveTo(theRect->left, y);
					DrawText(p.str, 0, tagLen);
					break;
				case ktsInvitation:
					{	Rect	subRect;
					
						subRect.right = theRect->right - 36;
						subRect.left = theRect->left;
						subRect.top = theRect->top + sText.nextLine;
						subRect.bottom = theRect->bottom;
						TextBox(p.str, tagLen, &subRect, 0);
					}
					break;
				case ktsPlayerLimit:
					inInfo[5] = '0' + *p.uc;
					break;
				case ktsHasPassword:
					inInfo[0] = 7;
					break;
			}
		}
		else
		{	char	index;
		
			index = *p.c++;
			tagLen--;
			switch(-theTag)
			{	case kisPlayerNick:
					curPlayers++;
					if(index == 0)
					{	MoveTo(theRect->right - TextWidth(p.str, 0, tagLen), y);
						DrawText(p.str, 0, tagLen);
					}
					break;
			}
		}
		
		p.c += tagLen;
	}
	
	inInfo[1] = '0' + curPlayers;
	MoveTo(theRect->right - StringWidth(inInfo), y + sText.nextLine);
	DrawString(inInfo);

	RestoreTextSettings(&sText);
}

static
pascal void	trackerListProc(
	short		message,
	Boolean		select,
	Rect		*rect,
	Point		tcell,
	short		offset,
	short		len,
	ListHandle	theList)
{
	short		savedFont, savedSize, savedFace;
	char		oldState;
	Ptr			data;
	StringPtr	inviteString, addrString;
	
	switch(message)
	{	case lInitMsg:
			break;
		case lDrawMsg:
			if(len)
			{	oldState = HGetState((*theList)->cells);
				HLock((*theList)->cells);
				data = ((*(*theList)->cells)+offset);
				DrawTrackerListItem(rect, data, len);
				HSetState(((*theList)->cells), oldState);
			}
			if(!select)	break;
		case lHiliteMsg:
			HILITEMODE;
			InvertRect(rect);
			break;
		case lCloseMsg:
			break;
	}
}

pascal	void	TrackerDrawList(
	WindowPtr	theWindow,
	short		item)
{
	LUpdate(theWindow->visRgn, curTracker->gameList);
	FrameRect(&curTracker->gameRect);
}

void	CTracker::PrepareGameList()
{
	Handle	iHandle;
	short	iType;
	Rect	emptyBounds = {0,0,0,1};
	Point	cellSize;
	Rect	contentRect;
	Point	theCell;

	GetDItem(trackerDialog, kTrackerInfoUserBox, &iType, &iHandle, &infoRect);
	
	GetDItem(trackerDialog, kResultListItem, &iType, &iHandle, &gameRect);
	SetDItem(trackerDialog, kResultListItem, iType, (Handle)TrackerDrawList, &gameRect);
	
	cellSize.h = gameRect.right - gameRect.left - 16;
	cellSize.v = 28;
	
	*(void **)(1+(short *)*GetResource('LDEF', 410)) = trackerListProc;
	
	FlushDataCache();
	FlushInstructionCache();
	
	contentRect = gameRect;
	contentRect.right -= 16;
	contentRect.top += 1;
	contentRect.left += 1;
	contentRect.bottom -= 1;

	gameList = LNew(&contentRect, &emptyBounds, cellSize, 410,
					trackerDialog, false, false, false, true);

	LDoDraw(true, gameList);
	shownSelected = -1;
}

void	CTracker::SetStatusDisplay(
	short		stat,
	StringPtr	theStr)
{
	Str255	tempString;
	Handle	iHandle;
	Rect	iRect;
	short	iType;
	
	GetDItem(trackerDialog, kTrackerStatusItem, &iType, &iHandle, &iRect);

	if(stat)
	{	if(!hasSpecialMessage)
		{	curMessage = stat;
			GetIndString(tempString, kTrackerStatusStrings, stat);
			SetIText(iHandle, tempString);
		}
	}
	else
	{	curMessage = 0;
		hasSpecialMessage = true;
		SetIText(iHandle, theStr);
	}	
}


void	CTracker::ReceivedNewGameRecord(
	Ptr		theData,
	long	len)
{
	Point			theCell;
	Boolean			isResponse = false;
	Boolean			idMatch = false;
	charWordLongP	p;
	Ptr				endData;
	short			thisRecordNumber = -1;
	short			recordCount = -2;
	
	p.c = theData;
	endData = theData + len;
	
	while(p.c < endData)
	{	char			theTag;
		unsigned char	tagLen;
		
		theTag = *p.c++;
		tagLen = *p.uc++;
		switch(theTag)
		{	case ktsCommand:
				isResponse = (ktcResponse == *p.uc);
				break;

			case ktsQueryID:
				idMatch = (queryId == *p.l);
				break;

			case ktsStatusMessage:
				SetStatusDisplay(0, p.str-1);
				break;

			case ktsInfoMessage:
				PtrToXHand(p.c, infoLeftHandle, tagLen);
				SetHandleSize(infoRightHandle, 0);
				InvalRect(&infoRect);
				hasSpecialMessage = true;
				break;
			case ktsResponseIndex:
				thisRecordNumber = *p.w;
				break;
			case ktsResponseCount:
				recordCount = *p.w;
				break;
		}
		
		p.c += tagLen;
	}

	if(thisRecordNumber == recordCount - 1)
	{	SetStatusDisplay(kStatIdle, NULL);
	}
	else
	{	if(curMessage != kStatReceiving &&
			curMessage != kStatIdle)
		{	SetStatusDisplay(kStatReceiving, NULL);
		}
	}

	if(		idMatch
		&&	isResponse
		&&	GetHandleSize((*gameList)->cells) + len < 32000)
	{	theCell.v = (*gameList)->dataBounds.bottom;
		theCell.h = 0;
		LAddRow(1, theCell.v, gameList);
		LSetCell(theData, len, theCell, gameList);
	}
}

static
void	ScanForAddrInfo(
	Ptr			theData,
	short		len,
	StringPtr	hostDomain,
	long		*hostIP,
	short		*hostPort)
{
	charWordLongP	p;

	*hostIP = 0;
	*hostPort = 19567;
	
	p.c = theData;
	theData += len;
	
	while(p.c < theData)
	{	char			theTag;
		unsigned char	tagLen;
		
		theTag = *p.c++;
		tagLen = *p.uc++;
		if(theTag >= 0)
		{	switch(theTag)
			{	case ktsHostDomain:
					BlockMoveData(p.str, hostDomain+1, tagLen);
					hostDomain[0] = tagLen;
					break;
			}
		}
		else
		{	char	index;
		
			index = *p.c++;
			tagLen--;
			switch(-theTag)
			{	case kisPlayerIPPort:
					if(index == 0)
					{	*hostIP = *p.ul++;
						*hostPort = *p.uw++;
						tagLen -= sizeof(long) + sizeof(short);
					}
					break;
			}
		}
		
		p.c += tagLen;
	}
}

#ifdef	LONE_TRACKER
void	TryWindowLocation(
	WindowPtr	theWindow,
	Rect		*tryRect)
{
	Rect		dragRect;
	Rect		sizeRect;
	RgnHandle	desktop;

	desktop = GetGrayRgn();

	dragRect = *tryRect;
	dragRect.left += 4;
	dragRect.right -= 4;
	dragRect.bottom = dragRect.top - 4;
	dragRect.top -= 18;
		
	if(RectInRgn(&dragRect, desktop))
	{	MoveWindow(theWindow, tryRect->left, tryRect->top, false);
	}
}
#endif

Boolean	CTracker::QueryTracker(
	StringPtr	hostName,
	short		*hostPort)
{
	Handle			trackerHandle;
	Handle			inputHandle;
	short			itemHit;
	ModalFilterUPP	myFilter;
	Str255			tempString;
	Rect			iRect;
	short			iType;
	Rect			popRect;
	Handle			iHandle;
	ControlHandle	selectButton, selectIP;
	Boolean			newRead = true;
	Boolean			didChange = false;
	GrafPtr			savedPort;
	
	queryId = 0;
	hasSpecialMessage = false;
	curMessage = 1;

	GetPort(&savedPort);

	infoLeftHandle = GetResource('TEXT', kTrackerHelpResource);
	DetachResource(infoLeftHandle);
	infoRightHandle = NewHandle(0);

	curTracker = this;
	defaultButton = kQueryItem;
	enabledState = true;

	myFilter = NewModalFilterProc(UDPTrackerDialogFilter);

	trackerDialog = GetNewDialog(kUDPTrackerDialogId, 0, (WindowPtr)-1);
	SetPort(trackerDialog);
#ifdef LONE_TRACKER
	iRect = trackerDialog->portRect;
	LocalToGlobal(&topLeft(iRect));
	LocalToGlobal(&botRight(iRect));
	prefs->ReadRect('�Rec', &iRect);

	TryWindowLocation(trackerDialog, &iRect);
	ShowWindow(trackerDialog);
#endif

	PrepareGameList();

	GetDItem(trackerDialog, kTrackerPopItem, &iType, &iHandle, &popRect);

	GetDItem(trackerDialog, kSelectItem, &iType, (Handle *)&selectButton, &iRect);
	GetDItem(trackerDialog, kSelectIPItem, &iType, (Handle *)&selectIP, &iRect);
	
	GetDItem(trackerDialog, kTrackerNameItem, &iType, &trackerHandle, &iRect);
	GetDItem(trackerDialog, kInputItem, &iType, &inputHandle, &iRect);

	prefs->ReadString(kUDPTrackerNameTag, tempString);
	if(tempString[0])
	{	SetIText(trackerHandle, tempString);
	}

	prefs->ReadString(kUDPQueryStringTag, tempString);
	if(tempString[0])
	{	SetIText(inputHandle, tempString);
	}

	SelIText(trackerDialog, kInputItem, 0, 32767);

	do
	{	Point	aCell;
	
		aCell.h = 0;
		aCell.v = 0;

		if(enabledState != LGetSelect(true, &aCell, gameList))
		{	short	state;
		
			enabledState = !enabledState;
			state = enabledState ? false : 255;
			HiliteControl(selectButton, state);
			HiliteControl(selectIP, state);
		}

#ifdef LONE_TRACKER
		{	EventRecord		theEvent;
		
			itemHit = 0;
			GetNextEvent(everyEvent, &theEvent);

			if(IsDialogEvent(&theEvent))
			{	if(!UDPTrackerDialogFilter(trackerDialog, &theEvent, &itemHit))
				{	DialogSelect(&theEvent, &trackerDialog, &itemHit);
				}
			}
			else
			{	WindowPtr		whichWindow;
				short			thePart;
				
				thePart = FindWindow(theEvent.where, &whichWindow);
				switch(thePart)
				{	case inDrag:
						{	Rect	dragRect;
							GrafPtr	wMgrPort;
						
							GetWMgrPort(&wMgrPort);
							dragRect = wMgrPort->portRect;
							InsetRect(&dragRect, 4, 4);
							DragWindow(whichWindow,theEvent.where,&dragRect);
						}
						break;
					case inMenuBar:
						gApplication->ExternalEvent(&theEvent);
						break;
					case inSysWindow:
					case inDesk:
						{	
							SystemClick(&theEvent, whichWindow);
						}
						break;
				}
			}
		}
#else
		ModalDialog(myFilter, &itemHit);
#endif
		if(stream && receivePB.ioResult == noErr && receivePB.csParam.receive.rcvBuffLen)
		{	ReceivedNewGameRecord(receivePB.csParam.receive.rcvBuff, receivePB.csParam.receive.rcvBuffLen);
			UDPBfrReturn(&receivePB,false);
			newRead = true;
		}
		
		if(stream && newRead)
		{
			newRead = false;
			
			receivePB.ioResult = noErr;
			receivePB.udpStream = stream;
			receivePB.ioCRefNum = gMacTCP;
			receivePB.ioCompletion = NULL;
			receivePB.ioResult = noErr;
			receivePB.csParam.receive.timeOut = 0;
			receivePB.csParam.receive.secondTimeStamp = 0;	//	Reserved by MacTCP
			receivePB.csParam.receive.userDataPtr = (Ptr) this;
			
			UDPRead(&receivePB, true);
		}

		switch(itemHit)
		{	case kTrackerPopItem:
				{	short	newPort = kTrackerPortNumber;
				
					GetIText(trackerHandle, tempString);
					SetPort(trackerDialog);
					if(owner->DoHotPop(&popRect, tempString, &newPort, kUDPTrackerHotListTag))
					{	SelIText(trackerDialog, kInputItem, 0, 32767);
						SetIText(trackerHandle, tempString);
					}
				}
				break;
			case kQueryItem:
				{	SetCursor(&gWatchCursor);
				
					GetIText(trackerHandle, tempString);
					prefs->WriteString(kUDPTrackerNameTag, tempString);

					GetIText(inputHandle, tempString);
					queryId = TickCount();

					hasSpecialMessage = false;
					ResetTagBuffer();
					WriteCharTag(ktsCommand, ktcQuery);
					WriteLongTagIndexed(kisPlayerLocation, 0, *(long *)&globalLocation);
					WriteLongTag(ktsQueryID, queryId);
					WriteStringTag(ktsQueryString, tempString);

					writeDataTable[0].ptr = *trackerDataHandle;
					writeDataTable[0].length = logicalSize;
					writeDataTable[1].ptr = NULL;
					writeDataTable[1].length = 0;

					SetStatusDisplay(kStatLookup, NULL);
					if(!stream)	
						OpenStream();

					if(PrepareToSend() != noErr)
					{	SetStatusDisplay(kStatLookupFailed, NULL);
					}
					else
					{	SetStatusDisplay(kStatSending, NULL);
						if(UDPWrite(&sendPB, false))
							SetStatusDisplay(kStatWriteFailed, NULL);
						else
							SetStatusDisplay(kStatWaiting, NULL);
					}

					shownSelected = -1;
					LDelRow(0, 0, gameList);
					ListClicked();
				}

				break;
#ifdef DEBUGGING_BUTTON
			case kSelectIPItem:
				BuildServerTags();
				HLock(trackerDataHandle);
				WriteStringTag(ktsInvitation, "\PHello folks, this is a really long invitation string with lots of stuff that will not fit anywhere.");
				WriteStringTag(ktsHostDomain, "\Phyperion.ambrosiasw.com");
				WriteStringTag(ktsInfoMessage, "\PIBM - I buy Macintosh.");
				ReceivedNewGameRecord(*trackerDataHandle, logicalSize);
				HUnlock(trackerDataHandle);
				break;
#endif
		}

		SetCursor(&qd.arrow);

	} while(itemHit != kCloseItem && itemHit != kSelectItem
			&& itemHit != kSelectIPItem);
	
	if(itemHit == kSelectIPItem || itemHit == kSelectItem)
	{	Point	aCell = {0,0};
	
		if(LGetSelect(true, &aCell, gameList))
		{	short			len;
			short			offset;
			long			hostIP;

			HLock((*gameList)->cells);
			LFind(&offset, &len, aCell, gameList);
			ScanForAddrInfo(offset + *(*gameList)->cells, len, hostName, &hostIP, hostPort);
			
			if(itemHit == kSelectIPItem)
			{	AddrToStr(hostIP, (char *)hostName+1);
				for(hostName[0] = 0; hostName[hostName[0]+1]; hostName[0]++);
			}
			
			didChange = true;
		}
	}

	GetIText(trackerHandle, tempString);
	prefs->WriteString(kUDPTrackerNameTag, tempString);

	GetIText(inputHandle, tempString);
	prefs->WriteString(kUDPQueryStringTag, tempString);	

	LDispose(gameList);

#ifdef LONE_TRACKER
	iRect = trackerDialog->portRect;
	LocalToGlobal(&topLeft(iRect));
	LocalToGlobal(&botRight(iRect));
	prefs->WriteRect('�Rec', &iRect);
#endif
	
	DisposeDialog(trackerDialog);
	DisposeRoutineDescriptor(myFilter);

	CloseStream();
	SetPort(savedPort);

	return didChange;
}

Ptr		CTracker::WriteBuffer(TrackTag tag, short len)
{
	long	whereTo;
	
	whereTo = logicalSize;

	if(OddIncreaseRamFile(trackerDataHandle, &realSize, &logicalSize, len + sizeof(TrackTag) + 1))
	{	DebugStr("\PTracker Panic!");
	}
	
	(*trackerDataHandle)[whereTo] = tag;
	(*trackerDataHandle)[whereTo+1] = len;

	return *trackerDataHandle + whereTo + 2;
}

Ptr		CTracker::WriteBufferIndexed(TrackTag tag, char index, short len)
{	Ptr		whereTo;
	
	whereTo = WriteBuffer(-tag, len + 1);
	*whereTo++ = index;
	
	return whereTo;
}

void	CTracker::WriteNullTag(TrackTag tag)					{	WriteBuffer(tag, 0);									}
void	CTracker::WriteCharTag(TrackTag tag, char value)		{	*((char *)WriteBuffer(tag, sizeof(value))) = value;		}
void	CTracker::WriteShortTag(TrackTag tag, short value)		{	*((short *)WriteBuffer(tag, sizeof(value))) = value;	}
void	CTracker::WriteLongTag(TrackTag tag, long value)		{	*((long *)WriteBuffer(tag, sizeof(value))) = value;		}
void	CTracker::WriteStringTag(TrackTag tag, StringPtr str)	{	BlockMoveData(str+1, WriteBuffer(tag, str[0]), str[0]);	}

void	CTracker::WriteNullTagIndexed(TrackTag tag, char index)					{	WriteBufferIndexed(tag, index, 0);										}
void	CTracker::WriteCharTagIndexed(TrackTag tag, char index, char value)		{	*((char *)WriteBufferIndexed(tag, index, sizeof(value))) = value;		}
void	CTracker::WriteShortTagIndexed(TrackTag tag, char index, short value)	{	*((short *)WriteBufferIndexed(tag, index, sizeof(value))) = value;		}
void	CTracker::WriteLongTagIndexed(TrackTag tag, char index, long value)		{	*((long *)WriteBufferIndexed(tag, index, sizeof(value))) = value;		}
void	CTracker::WriteStringTagIndexed(TrackTag tag, char index, StringPtr str){	BlockMoveData(str+1, WriteBufferIndexed(tag, index, str[0]), str[0]);	}

void	CTracker::ResetTagBuffer()
{
	HUnlock(trackerDataHandle);
	logicalSize = 0;

	WriteShortTag(ktsProtocolVersion, owner->softwareVersion);
	WriteStringTag(ktsSoftwareVersion, gApplication->shortVersString);
}

void	CTracker::BuildServerTags()
{
	ResetTagBuffer();

	owner->BuildServerTags();

#ifndef LONE_TRACKER
	((CAvaraApp *)gApplication)->gameNet->BuildTrackerTags(this);
#endif
}

void	CTracker::StartTracking()
{
	if(!isTracking)
	{	isTracking = true;
		SendTrackerInfo(false);
	}
}

void	CTracker::StopTracking()
{
	if(isTracking)
	{	isTracking = false;
		SendTrackerInfo(true);
	}
}