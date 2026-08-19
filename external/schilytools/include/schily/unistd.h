/* @(#)unistd.h	1.28 17/04/30 Copyright 1996-2017 J. Schilling */
/*
 *	Definitions for unix system interface
 *
 *	Copyright (c) 1996-2017 J. Schilling
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
#ifndef _SCHILY_UNISTD_H
#define _SCHILY_UNISTD_H

#include <retroburner/config.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#include <direct.h>
#include <stdlib.h>
#ifndef access
#define access _access
#endif
#ifndef close
#define close _close
#endif
#ifndef dup
#define dup _dup
#endif
#ifndef dup2
#define dup2 _dup2
#endif
#ifndef read
#define read _read
#endif
#ifndef write
#define write _write
#endif
#ifndef unlink
#define unlink _unlink
#endif
#ifndef lseek
#define lseek _lseek
#endif
#else
#include <unistd.h>
#endif

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif

#endif /* _SCHILY_UNISTD_H */
