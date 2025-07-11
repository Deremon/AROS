#ifndef _STDC_WCHAR_H_
#define _STDC_WCHAR_H_
/*
    Copyright © 1995-2025, The AROS Development Team. All rights reserved.
    $Id$

    Standard C Library: Extended multibyte and wide character handling.
*/

#include <aros/system.h>

#include <aros/types/wchar_t.h>
#include <aros/types/size_t.h>
#include <aros/types/mbstate_t.h>
#include <aros/types/wint_t.h>

#include <stdarg.h>
#include <stdint.h>

struct tm;

#include <aros/types/null.h>
#define	WCHAR_MIN	(-__WCHAR_MAX__ - 1l)
#define WCHAR_MAX	__WCHAR_MAX__

#ifndef WEOF
#define WEOF		((wint_t)-1)
#endif

/*  wchar.h should not include stdio.h */
struct __sFILE;


__BEGIN_DECLS

/* Internal functions */
int __wvcformat (void * data, int (* outc)(int, void *),
        const wchar_t * format, va_list args);
/* Formatted wide-character input/output functions */
/* NOTIMPL int swprintf(wchar_t * restrict s, size_t n,
	const wchar_t * restrict format, ...); */
/* NOTIMPL int swscanf(const wchar_t * restrict s,
	const wchar_t * restrict format, ...); */
/* NOTIMPL int vswprintf(wchar_t * restrict s, size_t n,
	const wchar_t * restrict format, va_list arg); */
/* NOTIMPL int vswscanf(const wchar_t * restrict s,
	const wchar_t * restrict format, va_list arg); */
int vwprintf(const wchar_t * restrict format,
	va_list args);
/* NOTIMPL int vwscanf(const wchar_t * restrict format,
	va_list arg); */
int wprintf(const wchar_t * restrict format, ...);
/* NOTIMPL int wscanf(const wchar_t * restrict format, ...); */

/* General wide-string utilities */
/* NOTIMPL double wcstod(const wchar_t * restrict nptr,
	wchar_t ** restrict endptr); */
/* NOTIMPL float wcstof(const wchar_t * restrict nptr,
	wchar_t ** restrict endptr); */
/* NOTIMPL long double wcstold(const wchar_t * restrict nptr,
	wchar_t ** restrict endptr); */

/* NOTIMPL long int wcstol(const wchar_t * restrict nptr,
	wchar_t ** restrict endptr, int base); */
/* NOTIMPL unsigned long int wcstoul(const wchar_t * restrict nptr,
	wchar_t ** restrict endptr,
	int base); */
/* NOTIMPL long long int wcstoll(const wchar_t * restrict nptr,
	wchar_t ** restrict endptr, int base); */
/* NOTIMPL unsigned long long int wcstoull(const wchar_t * restrict nptr,
	wchar_t ** restrict endptr, int base); */

wchar_t *wcscat(wchar_t * restrict s1, const wchar_t * restrict s2);
wchar_t *wcsncat(wchar_t * restrict s1, const wchar_t * restrict s2, size_t n);
int wcscmp(const wchar_t *s1, const wchar_t *s2);
wchar_t *wcscpy(wchar_t *s1, const wchar_t *s2);
/* NOTIMPL int wcscoll(const wchar_t *s1, const wchar_t *s2); */
int wcsncmp(const wchar_t *s1, const wchar_t *s2, size_t n);
wchar_t *wcsncpy(wchar_t *s1, const wchar_t *s2, size_t n);
/* NOTIMPL size_t wcsxfrm(wchar_t * restrict s1, const wchar_t * restrict s2, size_t n); */
wchar_t *wcschr(const wchar_t *s, wchar_t c);
size_t wcscspn(const wchar_t *s1, const wchar_t *s2);
size_t wcslen(const wchar_t *s);
wchar_t *wcspbrk(const wchar_t *s1, const wchar_t *s2);
wchar_t *wcsrchr(const wchar_t *s, wchar_t c);
size_t wcsspn(const wchar_t *s1, const wchar_t *s2);
wchar_t *wcsstr(const wchar_t *s1, const wchar_t *s2);
wchar_t *wcstok(wchar_t * restrict s1, const wchar_t * restrict s2, wchar_t ** restrict ptr);
wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n);
int wmemcmp(const wchar_t * s1, const wchar_t * s2, size_t n);
wchar_t *wmemcpy(wchar_t * restrict s1, const wchar_t * restrict s2, size_t n);
wchar_t *wmemmove(wchar_t *s1, const wchar_t *s2, size_t n);
wchar_t *wmemset(wchar_t *s, wchar_t c, size_t n);

/* Wide-character time conversion utilities */
/* NOTIMPL size_t wcsftime(wchar_t * restrict s, size_t maxsize,
	const wchar_t * restrict format,
	const struct tm * restrict timeptr); */

/* Extended multibyte and wide character utilities */
/* NOTIMPL wint_t btowc(int c); */
/* NOTIMPL int wctob(wint_t c); */
/* NOTIMPL int wcwidth(wchar_t); */
/* NOTIMPL int mbsinit(const mbstate_t *ps); */
size_t mbrlen(const char * restrict s, size_t n, mbstate_t * restrict ps);
size_t mbrtowc(wchar_t * restrict pwc, const char * restrict s, size_t n,
	mbstate_t * restrict ps);
size_t wcrtomb(char * restrict s, wchar_t wc, mbstate_t * restrict ps);
size_t mbsrtowcs(wchar_t * restrict dst, const char ** restrict src, size_t len, 
	mbstate_t * restrict ps);
size_t mbsnrtowcs(wchar_t * restrict dst, const char ** restrict src, size_t nms, 
	size_t len, mbstate_t * restrict ps);
size_t wcsrtombs(char * restrict dst, const wchar_t ** restrict src, size_t len, 
	mbstate_t * restrict ps);
size_t wcsnrtombs(char * restrict dst,	const wchar_t ** restrict src, size_t nwc, 
	size_t len, mbstate_t * restrict ps);

/* Formatted wide-character input/output functions */
int fwprintf(struct __sFILE * restrict fh,
        const wchar_t * restrict format, ...);
/* NOTIMPL int fwscanf(struct __sFILE * restrict stream,
        const wchar_t * restrict format, ...); */
int vfwprintf(struct __sFILE * restrict stream,
	const wchar_t * restrict format, va_list args);
/* NOTIMPL int vfwscanf(struct __sFILE * restrict stream,
	const wchar_t * restrict format, va_list arg); */

/* Wide-character input/output functions. */
wint_t fgetwc(struct __sFILE *stream);
wchar_t *fgetws(wchar_t * restrict s,
	int n, struct __sFILE * restrict stream);
wint_t fputwc(wchar_t wc, struct __sFILE *fp);
/* NOTIMPL int fputws(const wchar_t * restrict s,
	struct __sFILE * restrict stream); */
/* NOTIMPL int fwide(struct __sFILE *stream, int mode); */
/* NOTIMPL wint_t getwc(struct __sFILE *stream); */
/* NOTIMPL wint_t getwchar(void); */
wint_t putwc(wchar_t wc, struct __sFILE *fp);
wint_t putwchar(wchar_t wc);
/* NOTIMPL wint_t ungetwc(wint_t c, struct __sFILE *stream); */

__END_DECLS

#endif /* _STDC_WCHAR_H_ */
