/*
 * MacSurf — macsurf_debug.h
 *
 * Debug instrumentation. MS_LOG writes the message to the front
 * window's title bar so you can see pipeline progress without
 * MacsBug and without stopping in CW8's debugger.
 */

#ifndef MACSURF_DEBUG_H
#define MACSURF_DEBUG_H

#ifdef MACSURF_DEBUG

void macsurf_debug_set_title(const char *msg);

#define MS_LOG(msg)          macsurf_debug_set_title(msg)
#define MS_BREAK(msg)        macsurf_debug_set_title(msg)
#define MS_ASSERT(cond, msg) do { if (!(cond)) macsurf_debug_set_title(msg); } while(0)

void macsurf_debug_log_int(const char *label, long value);
void macsurf_debug_log_str(const char *label, const char *value);

#else

#define MS_LOG(msg)
#define MS_BREAK(msg)
#define MS_ASSERT(cond, msg)
#define macsurf_debug_log_int(label, value)
#define macsurf_debug_log_str(label, value)

#endif
#endif
