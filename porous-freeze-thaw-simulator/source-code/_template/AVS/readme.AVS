The Automatic Versioning System (AVS) provides a means for
automatic build counter incrementation. The counter increases upon each build
of the application BINARY and is available to the application so that it can
print out the build counter or use it for whatever else purpose.

In order to use AVS, the following prerequisites must be met:

1) The application must include the AVS.h header, which provides functions
that obtain the parts of the current version number.

2) The application must have the following files in its build directory:
_AVScurrentv.h	(contains current version definition)
_AVS.c		(contains the definitions of functions from AVS.h)
_AVSversionInc	(script for build counter increment)

3) The application must be built using a special AVS-compliant makefile
   (the template is available in this directory)

NOTE:	If you want to use AVS with more than one application in one directory,
	you need to have separate renamed copies of the files _AVScurrentv.h and
	_AVS.c and you also have to modify the makefiles for all applications
	in order to match with the appropriate copy of the mentioned files.
