/*
 * MacSurf — Mac OS 9 frontend for NetSurf
 * mac_iconv.h — iconv shim using Text Encoding Converter
 *
 * This file is part of MacSurf, built on the NetSurf engine.
 * Licensed under GPL v2.
 */

#ifndef MAC_ICONV_H
#define MAC_ICONV_H

#include <stddef.h>

#ifdef __MACOS9__
#include <TextEncodingConverter.h>
#endif

typedef void *iconv_t;

iconv_t iconv_open(const char *tocode, const char *fromcode);
size_t  iconv(iconv_t cd, char **inbuf, size_t *inbytesleft,
              char **outbuf, size_t *outbytesleft);
int     iconv_close(iconv_t cd);

#endif /* MAC_ICONV_H */
