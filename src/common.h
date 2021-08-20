/*
 * common.h <z64.me>
 *
 * things shared between GUI and eventual CLI version
 *
 */

#ifndef TEXVIEW_COMMON_H_INCLUDED

#define TEXVIEW_COMMON_H_INCLUDED

#define  PROGNAME    "zztexview"
#define  PROGVER     "v1.0.2"
#define  PROGATTRIB  "<z64.me>"
#define  PROG_NAME_VER_ATTRIB PROGNAME" "PROGVER" "PROGATTRIB

#include "stb_image.h"
#include "stb_image_write.h"

/* line ending used for log file generation */
#ifdef _WIN32
#	define ENDLINE "\r\n"
#else
#	define ENDLINE "\n"
#endif

#endif

