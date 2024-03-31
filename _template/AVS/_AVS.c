/*******************************************************\
* Automatic Versioning System interface			*
* (C) 2005-2006 Digithell, Inc. (Pavel Strachota)	*
* file: _AVS.c						*
\*******************************************************/


/* !!! DO NOT MODIFY UNLESS YOU REALLY KNOW WHAT YOU ARE DOING !!! */

#include "AVS.h"
#include "_AVScurrentv.h"

const char * AVS_MajorVersion()
{
	return(__MAJOR_VERSION);
}

const char * AVS_MinorVersion()
{
	return(__MINOR_VERSION);
}

const char * AVS_Release()
{
	return(__RELEASE);
}

const char * AVS_VersionInfo()
{
	return(__VERSION_INFO);
}

int AVS_Build()
{
	return(__BUILD);
}
