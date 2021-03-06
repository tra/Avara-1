/*
    Copyright �1994-1995, Juri Munkki
    All rights reserved.

    File: CAvaraApp.h
    Created: Wednesday, November 16, 1994, 1:25
    Modified: Monday, June 12, 1995, 6:35
*/

#pragma once
#include "CEventHandler.h"
#include "PolyColor.h"

#define	kFirstPrefsCreate		'PREF'
#define	kLastSavedFileTag		'FILE'
#define	kFunctionMapTag			'FMP0'
#define	kGameWindowRectTag		'GRCT'
#define	kGameWindowVisTag		'GVIS'
#define	kRosterWindowRectTag	'RRCT'
#define	kRosterWindowVisTag		'RVIS'
#define	kResultsSortOrderTag	'RSOR'
#define	kKeyWindowRectTag		'KRCT'
#define	kKeyWindowVisTag		'KVIS'
#define	kInfoPanelRectTag		'IRCT'
#define	kInfoPanelVisTag		'IVIS'
#define	kInfoPanelStyleTag		'ISTL'
#define	kMouseJoystickTag		'MOJO'
#define	kMouseSensitivityTag	'SNIF'
#define	kAutoSaveScoresTag		'AUTS'

#define	kBackgroundProcessTag	'BGPR'
#define	kUpdateRosterTag		'UPRO'
#define	kHorizonDetailTag		'HRZD'
#define	kAmbientHologramsTag	'AHLO'
#define	kSimpleExplosionsTag	'SIMX'

#include "UDPTags.h"

#define	kDefaultAutoLatency		true
#define	kLatencyToleranceTag	'LATE'
#define	kDefaultLatencyTolerance	0
#define	kNetworkOptionsTag		'NET '
#define	kServerOptionsTag		'SERV'
#define	kDefaultServerOptions		128

#define	kBellNewArrivalsTag		'BELL'
#define	kBellSoundTag			'Bell'

#define	gestaltMacTCP			'mtcp'

#define	kSoundRateOptionTag		'SRat'
#define	kSoundChannelsOptionTag	'SCha'
#define	kMonoStereoOptionsTag	'MOST'
#define	k16BitSoundOptionTag	'SN16'
#define	kSoundSwitchesTag		'SSWT'


typedef struct
{
	Str63	fileName;
	OSType	creator;
	OSType	dirTag;
	long	createDate;
	long	changeDate;
	long	forkSize;
} DirectoryLocator;

class	CTagBase;
class	CCompactTagBase;

class	CAvaraApp : public CEventHandler
{
public:
	class	CGameWind			*theGameWind;
	class	CRosterWindow		*theRosterWind;
	class	CInfoPanel			*theInfoPanel;
	class	CCapWind			*capEditor;
	class	CAvaraGame			*itsGame;
	class	CLevelListWind		*bottomLevelListWindow;
	class	CBusyWindow			*busyWindow;
			
			short				loadedDirectoryRef;
			short				directoryRef;
			FSSpec				directorySpec;
			OSType				currentLevelDirectory;
			long				defaultLevelDirectoryDirID;

	class	CLevelDescriptor	*levelList;
		
	class	CNetManager			*gameNet;
	
			short				networkOptions;
			
			Boolean				fastMachine;
			Boolean				macTCPAvailable;

	PolyColorTable		myPolyColorTable;

	virtual	void		InitApp();
	virtual	void		Dispose();
	virtual	void		DoCommand(long theCommand);
	virtual	void		AdjustBellMenu();
	virtual	void		AdjustMenus(CEventHandler *master);
	virtual	void		UpdateLevelInfoTags(CTagBase *theTags);

	virtual	OSErr		OpenDirectoryFile(FSSpec *theFile);
	virtual	OSErr		OpenFSSpec(FSSpec *theFile, Boolean notify);
	virtual	void		OpenStandardFile();
	
	virtual	void		GetDirectoryLocator(DirectoryLocator *theDir);
	virtual	Boolean		LookForDirectory(DirectoryLocator *theDir);
	virtual	OSErr		FetchLevel(DirectoryLocator *theDir, OSType theLevel, short *crc);

	virtual	Boolean		DoEvent(CEventHandler *master, EventRecord *theEvent);
	virtual	void		GotEvent();
	
	virtual	void		DoAbout();
	
	virtual	void		RememberLastSavedFile(FSSpec *theFile);
	virtual	void		DoOpenApplication();
};
