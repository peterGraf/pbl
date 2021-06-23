#ifndef _PBL_CGI_H_
#define _PBL_CGI_H_
/*
 pblCgi.h - include file for Common Gateway Interface functions.

 Copyright (c) 2018 Peter Graf.

 This file is part of PBL - The Program Base Library.
 PBL is free software.

	MIT License

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.

 For more information on the Program Base Library or Peter Graf,
 please see: https://www.mission-base.com/.

$Log: pblCgi.h,v $
Revision 1.35  2021/06/23 14:32:50  peter
Switch to MIT license

Revision 1.34  2021/06/12 11:18:27  peter

 */

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <memory.h>
#include <malloc.h>
#include <assert.h>
#include <stdlib.h>

#ifdef _WIN32

#include <winsock2.h>
#include <direct.h>
#include <windows.h> 
#include <process.h>

#else

#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#endif

#include "pbl.h"

	/*****************************************************************************/
	/* #defines                                                                  */
	/*****************************************************************************/

#define PBL_CGI_KEY_DURATION                   "pblCgiDURATION"

#define PBL_CGI_MAX_LINE_LENGTH                (4 * 1024)

#define PBL_CGI_TRACE if(pblCgiTraceFile) pblCgiTrace

#define PBL_CGI_COOKIE                         "PBL_CGI_COOKIE"
#define PBL_CGI_COOKIE_PATH                    "PBL_CGI_COOKIE_PATH"
#define PBL_CGI_COOKIE_DOMAIN                  "PBL_CGI_COOKIE_DOMAIN"

#define PBL_CGI_TRACE_FILE                     "TraceFilePath"

#define pblCgiRand() (abs((rand()<<24) ^ (rand()<<16) ^ (rand()<<8) ^ rand()))

	/*****************************************************************************/
	/* Variable declarations                                                     */
	/*****************************************************************************/

	extern PblMap* pblCgiConfigMap;

	extern struct timeval pblCgiStartTime;
	extern FILE* pblCgiTraceFile;

	extern char* pblCgiQueryString;
	extern char* pblCgiCookieKey;
	extern char* pblCgiCookieTag;
	extern char* pblCgiPostData;
	extern int pblCgiContentLength;

	/*****************************************************************************/
	/* Function declarations                                                     */
	/*****************************************************************************/

	extern void pblCgiSetSelfDestruction(int seconds);
	extern char* pblCgiMalloc(char* tag, size_t size);
	extern char* pblCgiConfigValue(char* key, char* defaultValue);
	extern void pblCgiInitTrace(struct timeval* startTime, char* traceFilePath);
	extern void pblCgiTrace(const char* format, ...);

	extern FILE* pblCgiTryFopen(char* filePath, char* openType);
	extern FILE* pblCgiFopen(char* traceFilePath, char* openType);
	extern char* pblCgiGetEnv(char* name);

	extern void pblCgiExitOnError(const char* format, ...);
	extern char* pblCgiSprintf(const char* format, ...);

	extern int pblCgiStrArrayContains(char** array, char* string);
	extern char* pblCgiStrNCpy(char* dest, char* string, size_t n);
	extern char* pblCgiStrTrim(char* string);
	extern int pblCgiStrIsNullOrWhiteSpace(char* string);
	extern char* pblCgiStrRangeDup(char* start, char* end);
	extern char* pblCgiStrDup(char* string);
	extern int pblCgiStrEquals(char* s1, char* s2);
	extern int pblCgiStrCmp(char* s1, char* s2);
	extern char* pblCgiStrCat(char* s1, char* s2);
	extern char* pblCgiStrReplace(char* string, char* oldValue, char* newValue);
	extern char* pblCgiStrFromTimeAndFormat(time_t t, char* format);
	extern char* pblCgiStrFromTime(time_t t);
	extern int pblCgiStrSplit(char* string, char* splitString, size_t size, char* result[]);
	extern PblList* pblCgiStrSplitToList(char* string, char* splitString);
	extern char* pblCgiStrToHexFromBuffer(unsigned char* buffer, size_t length);

	extern PblMap* pblCgiNewMap(void);
	extern int pblCgiMapIsEmpty(PblMap* map);
	extern void pblCgiMapFree(PblMap* map);
	extern PblMap* pblCgiFileToMap(PblMap* map, char* traceFilePath);

	extern void pblCgiParseQuery(int argc, char* argv[]);
	extern char* pblCgiQueryValue(char* key);
	extern char* pblCgiQueryValueForIteration(char* key, int iteration);

	extern PblMap* pblCgiValueMap(void);
	extern void pblCgiSetValue(char* key, char* value);
	extern void pblCgiSetValueForIteration(char* key, char* value, int iteration);
	extern void pblCgiSetValueToMap(char* key, char* value, int iteration, PblMap* map);
	extern void pblCgiUnSetValue(char* key);
	extern void pblCgiUnSetValueForIteration(char* key, int iteration);
	extern void pblCgiUnSetValueFromMap(char* key, int iteration, PblMap* map);
	extern void pblCgiClearValues(void);
	extern char* pblCgiValue(char* key);
	extern char* pblCgiValueForIteration(char* key, int iteration);
	extern char* pblCgiValueFromMap(char* key, int iteration, PblMap* map);

	extern char* pblCgiGetCoockie(char* cookieKey, char* cookieTag);
	extern void pblCgiPrint(char* directory, char* fileName, char* contentType);

#ifdef WIN32

	extern int gettimeofday(struct timeval* tp, struct timezone* tzp);

#endif

#ifdef __cplusplus
}
#endif

#endif
