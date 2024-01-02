/*******************************************************\
* Automatic Versioning System interface			*
* (C) 2005-2006 Digithell, Inc. (Pavel Strachota)	*
* file: AVS.h						*
\*******************************************************/


#if !defined __AVS
#define __AVS

#ifdef __cplusplus
extern "C" {
#endif

const char * AVS_MajorVersion();
/*
returns the current major version
*/

const char * AVS_MinorVersion();
/*
returns the current minor version
*/

const char * AVS_Release();
/*
returns the current release
*/

const char * AVS_VersionInfo();
/*
returns a short version description
*/

int AVS_Build();
/*
returns the current build counter status
If used properly, this counter increases upon each application build.
*/

#ifdef __cplusplus
}
#endif

#endif		/* __AVSversion */
