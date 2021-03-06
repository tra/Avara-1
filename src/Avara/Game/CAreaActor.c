/*
    Copyright �1994-1996, Juri Munkki
    All rights reserved.

    File: CAreaActor.c
    Created: Sunday, December 4, 1994, 12:53
    Modified: Thursday, September 12, 1996, 0:41
*/

#include "CAreaActor.h"
#include "CSmartPart.h"

void	CAreaActor::BeginScript()
{
	inherited::BeginScript();
	
	ProgramLongVar(iExit, 0);
	ProgramLongVar(iEnter, 0);
	
	ProgramLongVar(iHeight, 0);
	ProgramLongVar(iFrequency, 5);
	ProgramLongVar(iWatchMask, kPlayerBit);
	ProgramLongVar(iMask, -1);
	

	radius = GetLastOval(location);
	ProgramFixedVar(iRange, radius);
}

CAbstractActor *CAreaActor::EndScript()
{
	inherited::EndScript();

	exitMsg = ReadLongVar(iExit);
	enterMsg = ReadLongVar(iEnter);

	radius = ReadFixedVar(iRange);
	location[1] = ReadFixedVar(iHeight) + ReadFixedVar(iBaseHeight);
	frequency = ReadLongVar(iFrequency);
	watchBits = ReadLongVar(iWatchMask);
	watchTeams = ReadLongVar(iMask);

	sleepTimer = 1;
	areaFlag = false;

	isActive = kIsInactive;

	return this;
}

void	CAreaActor::FrameAction()
{
				Boolean			newFlag = false;
	register	CAbstractActor	*theActor;
	register	CSmartPart		*thePart;

	sleepTimer = frequency;
	
	BuildPartProximityList(location, radius, watchBits);
	
	for(thePart = proximityList.p; thePart; thePart = (CSmartPart *)thePart->nextTemp)
	{	theActor = thePart->theOwner;
		if(theActor->teamMask & watchTeams)
		{	if(radius + thePart->header.enclosureRadius >
					FDistanceEstimate(	location[0]-thePart->sphereGlobCenter[0],
										location[1]-thePart->sphereGlobCenter[1],
										location[2]-thePart->sphereGlobCenter[2]))
				{	newFlag = true;
					break;
				}
		}
	}

	if(newFlag != areaFlag)
	{	areaFlag = newFlag;
		
		if(areaFlag)
		{	if(enterMsg)
				itsGame->FlagMessage(enterMsg);			
		}
		else
		{	if(exitMsg)
				itsGame->FlagMessage(exitMsg);
		}
	}
}