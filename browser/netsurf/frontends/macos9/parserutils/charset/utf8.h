#ifndef PARSERUTILS_CHARSET_UTF8_H
#define PARSERUTILS_CHARSET_UTF8_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    PARSERUTILS_OK          = 0,
    PARSERUTILS_NOMEM       = 1,
    PARSERUTILS_BADPARM     = 2,
    PARSERUTILS_INVALID     = 3,
    PARSERUTILS_FILENOTFOUND = 4,
    PARSERUTILS_NEEDDATA    = 5,
    PARSERUTILS_BADENCODING = 6,
    PARSERUTILS_EOF         = 7
} parserutils_error;

parserutils_error parserutils_charset_utf8_to_ucs4(
    const uint8_t *s, size_t len, uint32_t *ucs4, size_t *readlen);

parserutils_error parserutils_charset_utf8_from_ucs4(
    uint32_t ucs4, uint8_t **s, size_t *len);

parserutils_error parserutils_charset_utf8_length(
    const uint8_t *s, size_t max, size_t *len);

parserutils_error parserutils_charset_utf8_char_byte_length(
    const uint8_t *s, size_t *len);

parserutils_error parserutils_charset_utf8_prev(
    const uint8_t *s, size_t off, size_t *prev);

parserutils_error parserutils_charset_utf8_next(
    const uint8_t *s, size_t len, size_t off, size_t *next);

#endif
