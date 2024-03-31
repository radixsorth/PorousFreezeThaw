/*************************************************************************\
* Explicit byte order specification for systems that don't have endian.h  *
* (C) 2005-2006 Pavel Strachota                                           *
* file: _endian.h                                              	          *
\*************************************************************************/

#if !defined ___endian
#define ___endian

/*
Some compilers do not provide <endian.h> and thus the information about the byte order can not
be obtained. On the other hand, gcc or g++ include the <endian.h> file in from several other
headers, for example if g++ is used, <endian.h> is included from <stdlib.h>. This may sometimes
lead to unexpected results when we really want explicit byte order with g++ and we write e.g.

#include "_endian.h"
#include <stdlib.h>		<-- redefines the values from _endian.h

In this case, it is recommended to use the custom byte order information symbol _BYTEORDER that
is defined at the bottom of this file instead of __BYTE_ORDER. All programs in this suite that
have to deal with byte order do use this custom macro.
*/

#ifdef __EXPLICIT_BYTE_ORDER

	#define	__LITTLE_ENDIAN	1234
	#define	__BIG_ENDIAN	4321
	#define	__PDP_ENDIAN	3412

	/* designed for the HP-UX PA-RISC system */
	#define __BYTE_ORDER	__BIG_ENDIAN
#else
	#include <endian.h>
#endif

/*
This would not solve anything, because the macro expansion is carried out at the point of
its use:
#define _BYTEORDER	__BYTE_ORDER
*/

#if __BYTE_ORDER == __LITTLE_ENDIAN
	#define _BYTEORDER	__LITTLE_ENDIAN
#elif __BYTE_ORDER == __BIG_ENDIAN
	#define _BYTEORDER	__BIG_ENDIAN
#else
	#define _BYTEORDER	__PDP_ENDIAN
#endif

#endif	/* ___endian */
