/* @(#)varargs.h	1.8 14/01/06 Copyright 1998-2014 J. Schilling */
/*
 *	Generic header for users of var args ...
 *
 *	Includes a default definition for va_copy()
 *	and some magic know how about the SVr4 Power PC var args ABI
 *	to create a __va_arg_list() macro.
 *
 *	The __va_arg_list() macro is needed to fetch a va_list type argument
 *	from a va_list. This is needed to implement a recursive "%r" printf.
 *
 *	Copyright (c) 1998-2014 J. Schilling
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
 * RetroBurner modern forwarding header.
 * Legacy compiler-selection logic has been retired.
 */
#ifndef _SCHILY_VARARGS_H
#define _SCHILY_VARARGS_H

#include <stdarg.h>

#endif /* _SCHILY_VARARGS_H */
