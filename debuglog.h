#ifndef ndDebug_H
#define ndDebug_H

#include <stdio.h>

// Set verbosity using loglevels
// 0: DISABLED: no logging at all
// 1: ERROR: all errors
// 2: WARN: all errors and warnings
// 3: INFO: all errors, warnings and informational (default)
// 4: DEBUG: all errors, warnings, informational and debug messages

#define NDLOGLEVEL 4
#define NDLOGPREFIX ""

#define LERROR(x)    if(NDLOGLEVEL>0) { Serial.print(NDLOGPREFIX); Serial.println(x); }
#define LERROR1(x,y) if(NDLOGLEVEL>2) { Serial.print(NDLOGPREFIX); Serial.print(x); Serial.print(" "); Serial.println(y); }
#define LWARN(x)     if(NDLOGLEVEL>1) { Serial.print(NDLOGPREFIX); Serial.println(x); }
#define LWARN1(x,y)  if(NDLOGLEVEL>2) { Serial.print(NDLOGPREFIX); Serial.print(x); Serial.print(" "); Serial.println(y); }
#define LINFO(x)     if(NDLOGLEVEL>2) { Serial.print(NDLOGPREFIX); Serial.println(x); }
#define LINFO0(x)    if(NDLOGLEVEL>2) { Serial.print(x); }
#define LINFO00(x)   if(NDLOGLEVEL>2) { Serial.println(x); }
#define LINFO1(x,y)  if(NDLOGLEVEL>2) { Serial.print(NDLOGPREFIX); Serial.print(x); Serial.print(" "); Serial.println(y); }

#define LDEBUG(x)      if(NDLOGLEVEL>3) { Serial.println(x); }
#define LDEBUG0(x)     if(NDLOGLEVEL>3) { Serial.print(x); }
#define LDEBUG1(x,y)   if(NDLOGLEVEL>3) { Serial.print(x); Serial.print(" "); Serial.println(y); }
#define LDEBUG10(x,y)  if(NDLOGLEVEL>3) { Serial.print(x); Serial.print(" "); Serial.print(y); }
#define LDEBUG2(x,y,z) if(NDLOGLEVEL>3) { Serial.print(x); Serial.print(" "); Serial.print(y); Serial.print(" "); Serial.println(z); }


#endif
