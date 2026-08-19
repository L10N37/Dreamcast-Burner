/* @(#)prototyp.h	1.17 15/12/26 Copyright 1995-2015 J. Schilling */
/*
 *	Definitions for dealing with ANSI / KR C-Compilers
 *
 *	Copyright (c) 1995-2015 J. Schilling
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
 * <schily/mconfig.h> includes <schily/prototyp.h>
 * To be correct, we need to include <schily/mconfig.h> before we test
 * for _SCHILY_PROTOTYP_H
 *
 * In order to keep the silly Solaris hdrchk(1) quiet, we are forced to
 * have the _SCHILY_PROTOTYP_H first in <schily/prototyp.h>.
 * To keep hdrchk(1) quiet and be correct, we need to introduce a second
 * guard _SCHILY_PROTOTYP_X_H.
 */

/*
 * RetroBurner modern portability implementation.
 * The original cdrtools copyright/license above is retained.
 */
#ifndef _SCHILY_PROTOTYP_H
#define _SCHILY_PROTOTYP_H

#include <schily/ccomdefs.h>

#define PROTOTYPES 1
#define __PR(args) args

#endif /* _SCHILY_PROTOTYP_H */
