/*
*/

#include "CMarkerActor.h"
#include "CPlayerActor.h"
#include "CWalkerActor.h"
#include "CSphereActor.h"
#include "CHologramActor.h"
#include "CGroundColorAdjuster.h"
#include "CSkyColorAdjuster.h"
#include "CSolidActor.h"
#include "CTriPyramidActor.h"
#include "CDoorActor.h"
#include "CAreaActor.h"
#include "CGuardActor.h"

#define	LOOSESTRLIST	1024

static	Handle			objectNamesHandle = NULL;

enum	{
		koNoObject = 0,
		koMarker,
		koPlayer,
		koWalker,
		koSphere,
		koHologram,
		koSolid,
		koTriPyramid,
		koDoor,
		koArea,
		koGuard,
		koGroundColor,
		koSkyColor,
		koLastObject
		};


void *	CreateObjectByIndex(
	short	objectId)
{
	switch(objectId)
	{
		case koMarker:		return new CMarkerActor;
		case koPlayer:		return new CPlayerActor;
		case koWalker:		return new CWalkerActor;
		case koSphere:		return new CSphereActor;
		case koHologram:	return new CHologramActor;
		case koSolid:		return new CSolidActor;
		case koTriPyramid:	return new CTriPyramidActor;
		case koDoor:		return new CDoorActor;
		case koArea:		return new CAreaActor;
		case koGuard:		return new CGuardActor;
		case koGroundColor:	return new CGroundColorAdjuster;
		case koSkyColor:	return new CSkyColorAdjuster;
		default:			return NULL;
	}
}

void	InitLinkLoose()
{
	objectNamesHandle = GetResource('STR#', LOOSESTRLIST);
}

void *	CreateNamedObject(
	StringPtr	theName)
{
	StringPtr	string;
	StringPtr	charp;
	short		tokCount;
	short		ind;
	short		i;
	short		stringLen;
	
	if(objectNamesHandle == NULL)	InitLinkLoose();
	
	stringLen = *theName++;
	tokCount = *(short *)(*objectNamesHandle);
	string = ((StringPtr)*objectNamesHandle)+2;
	
	for(ind = 1;ind <= tokCount; ind++)
	{	if(string[0] == stringLen)
		{	charp = string+1;
		
			for(i=0;i<stringLen;i++)
			{	if(*charp++ != theName[i])
				{	break;
				}
			}
			
			if(i==stringLen)
			{	break;
			}
		}
		
		string += string[0] + 1;
	}
	
	return CreateObjectByIndex(ind);
}
