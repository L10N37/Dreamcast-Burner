/* @(#)time.h	1.20 13/10/01 Copyright 1996-2013 J. Schilling */
/*
 *	Generic header for users of time(), gettimeofday() ...
 *
 *	It includes definitions for time_t, struct timeval, ...
 *
 *	Copyright (c) 1996-2013 J. Schilling
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
 * RetroBurner modern portability implementation.
 * This replaces legacy configure-era platform substitution with
 * compiler/OS standard headers and CMake feature detection.
 */
#ifndef _SCHILY_TIME_H
#define _SCHILY_TIME_H

#include <time.h>
#include <retroburner/config.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifndef timerclear
#define timerclear(tvp) ((tvp)->tv_sec = (tvp)->tv_usec = 0)
#endif
#ifndef timerfix1
#define timerfix1(tvp) do { while ((tvp)->tv_usec < 0) { (tvp)->tv_sec--; (tvp)->tv_usec += 1000000; } } while (0)
#endif
#ifndef timerfix2
#define timerfix2(tvp) do { while ((tvp)->tv_usec > 1000000) { (tvp)->tv_sec++; (tvp)->tv_usec -= 1000000; } } while (0)
#endif
#ifndef timerfix
#define timerfix(tvp) do { timerfix1(tvp); timerfix2(tvp); } while (0)
#endif

#endif /* _SCHILY_TIME_H */
