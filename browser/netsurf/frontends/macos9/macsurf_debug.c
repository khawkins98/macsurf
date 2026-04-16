/*
 * MacSurf — macsurf_debug.c
 *
 * MS_LOG writes to the front window title bar. No DebugStr, no
 * debugger stops. Each checkpoint overwrites the previous one so
 * the title always shows the last pipeline stage reached.
 */

#include "macsurf_debug.h"

#ifdef MACSURF_DEBUG

#include <string.h>

#ifdef __MACOS9__
#include <MacWindows.h>

void
macsurf_debug_set_title(const char *msg)
{
	WindowRef w;
	unsigned char pstr[256];
	size_t len;

	w = FrontWindow();
	if (w == NULL || msg == NULL) return;

	len = strlen(msg);
	if (len > 255) len = 255;
	pstr[0] = (unsigned char)len;
	memcpy(pstr + 1, msg, len);
	SetWTitle(w, pstr);
}

void
macsurf_debug_log_int(const char *label, long value)
{
	char buf[128];
	long v;
	int neg;
	char digits[12];
	int di;
	int len;
	int i;

	if (label == NULL) label = "?";
	len = 0;
	while (label[len] != '\0' && len < 80) {
		buf[len] = label[len];
		len++;
	}
	buf[len++] = ':';
	buf[len++] = ' ';

	v = value;
	neg = 0;
	if (v < 0) { neg = 1; v = -v; }
	di = 0;
	do {
		digits[di++] = (char)('0' + (int)(v % 10));
		v /= 10;
	} while (v > 0 && di < 11);
	if (neg) digits[di++] = '-';
	for (i = di - 1; i >= 0 && len < 126; i--)
		buf[len++] = digits[i];
	buf[len] = '\0';

	macsurf_debug_set_title(buf);
}

void
macsurf_debug_log_str(const char *label, const char *value)
{
	char buf[128];
	int len;
	int vi;

	if (label == NULL) label = "?";
	if (value == NULL) value = "(null)";
	len = 0;
	while (label[len] != '\0' && len < 60) {
		buf[len] = label[len];
		len++;
	}
	buf[len++] = ':';
	buf[len++] = ' ';
	vi = 0;
	while (value[vi] != '\0' && len < 126)
		buf[len++] = value[vi++];
	buf[len] = '\0';

	macsurf_debug_set_title(buf);
}

#else
#include <stdio.h>

void macsurf_debug_set_title(const char *msg)
{
	fprintf(stderr, "MS_LOG: %s\n", msg != NULL ? msg : "(null)");
}

void macsurf_debug_log_int(const char *label, long value)
{
	fprintf(stderr, "MS_LOG: %s: %ld\n", label ? label : "?", value);
}

void macsurf_debug_log_str(const char *label, const char *value)
{
	fprintf(stderr, "MS_LOG: %s: %s\n", label ? label : "?", value ? value : "(null)");
}
#endif
#endif
