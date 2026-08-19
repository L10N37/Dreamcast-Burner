/* @(#)libport.h	1.52 21/06/16 Copyright 1995-2021 J. Schilling */
/*
 *	Prototypes for POSIX standard functions that may be missing on the
 *	local platform and thus are implemented inside libschily.
 *
 *	Copyright (c) 1995-2021 J. Schilling
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
 * The original cdrtools copyright/license above is retained.
 */
#ifndef _SCHILY_LIBPORT_H
#define _SCHILY_LIBPORT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#include <direct.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#endif

/*
 * Standard/POSIX declarations now come from the platform headers.
 * RetroBurner no longer redeclares libc or uid/gid interfaces here.
 */

#endif /* _SCHILY_LIBPORT_H */
