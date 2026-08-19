/* @(#)string.h	1.12 11/11/24 Copyright 1996-2011 J. Schilling */
/*
 *	Definitions for strings
 *
 *	Copyright (c) 1996-2011 J. Schilling
 */
/*
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * See the file CDDL.Schily.txt in this distribution for details.
 * A copy of the CDDL is also available via the Internet at
 * http://www.opensource.org/licenses/cddl1.txt
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file CDDL.Schily.txt from this distribution.
 */

/*
 * RetroBurner modern standard-library forwarding header.
 *
 * The historical filename remains temporarily because inherited
 * optical source still includes <schily/...>.  The legacy fallback
 * declarations are intentionally gone; supported compilers provide
 * the ISO C / platform headers directly.
 */
#ifndef _SCHILY_STRING_H
#define _SCHILY_STRING_H

#include <string.h>

#ifdef _WIN32
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
#else
#include <strings.h>
#endif

#endif /* _SCHILY_STRING_H */
