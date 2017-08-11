/*
 * pasprintf.c -- example of how to use vsnprintf() portably
 */

#include "4DPluginAPI.h"

#if VERSIONWIN

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

/* size of first buffer malloc; start small to exercise grow routines */
#define	FIRSTSIZE	1


int
pasprintf(char **ret, const char *fmt, ...)
{
	va_list args;
	char *buf;
	size_t bufsize;
	char *newbuf;
	size_t nextsize;
	long int outsize;

	bufsize = 0;
	for (;;) {
		if (bufsize == 0) {
			if ((buf = (char *)malloc(FIRSTSIZE)) == NULL) {
				*ret = NULL;
				return -1;
			}
			bufsize = 1;
		} else if ((newbuf = (char *)realloc(buf, nextsize)) != NULL) {
			buf = newbuf;
			bufsize = nextsize;
		} else {
			free(buf);
			*ret = NULL;
			return -1;
		}

		va_start(args, fmt);
		outsize = vsnprintf(buf, bufsize, fmt, args);
		va_end(args);

		if (outsize == -1) {
			/* Clear indication that output was truncated, but no
			 * clear indication of how big buffer needs to be, so
			 * simply double existing buffer size for next time.
			 */
			nextsize = bufsize * 2;

		} else if (outsize == bufsize) {
			/* Output was truncated (since at least the \0 could
			 * not fit), but no indication of how big the buffer
			 * needs to be, so just double existing buffer size
			 * for next time.
			 */
			nextsize = bufsize * 2;

		} else if (outsize > bufsize) {
			/* Output was truncated, but we were told exactly how
			 * big the buffer needs to be next time. Add two chars
			 * to the returned size. One for the \0, and one to
			 * prevent ambiguity in the next case below.
			 */
			nextsize = outsize + 2;

		} else if (outsize == bufsize - 1) {
			/* This is ambiguous. May mean that the output string
			 * exactly fits, but on some systems the output string
			 * may have been trucated. We can't tell.
			 * Just double the buffer size for next time.
			 */
			nextsize = bufsize * 2;

		} else {
			/* Output was not truncated */
			break;
		}
	}
	*ret = buf;
	return 0;
}
#endif