/* @(#)xconfig.h	1.13 13/07/23 Copyright 1995-2013 J. Schilling */
/*
 *	This file either includes the dynamic or manual autoconf stuff.
 *
 *	Copyright (c) 1995-2013 J. Schilling
 *
 *	This file is included from <schily/mconfig.h> and usually
 *	includes $(SRCROOT)/incs/$(OARCH)/xconfig.h via
 *	-I$(SRCROOT)/incs/$(OARCH)/
 *
 *	Use only cpp instructions.
 *
 *	NOTE: SING: (Schily Is Not Gnu)
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
#ifndef _SCHILY_XCONFIG_H
#define _SCHILY_XCONFIG_H

#include <retroburner/config.h>

/* Legacy name retained only as an include-path compatibility point. */

#endif /* _SCHILY_XCONFIG_H */
