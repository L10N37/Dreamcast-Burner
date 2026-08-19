/* @(#)mmcvendor.h	1.4 06/09/13 Copyright 2002-2004 J. Schilling */
/*
 *	Copyright (c) 2002-2004 J. Schilling
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

#ifndef	_MMCVENDOR_H
#define	_MMCVENDOR_H

#include <schily/utypes.h>
#include <schily/btorder.h>

#if defined(_BIT_FIELDS_LTOH)	/* Intel bitorder */

struct ricoh_mode_page_30 {
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0xE = 14 Bytes */
	unsigned char	BUEFS		:1;	/* Burn-Free supported	*/
	unsigned char	TWBFS		:1;	/* Test Burn-Free sup.	*/
	unsigned char	res_2_23	:2;
	unsigned char	ARSCS		:1;	/* Auto read speed control supp. */
	unsigned char	AWSCS		:1;	/* Auto write speed control supp. */
	unsigned char	res_2_67	:2;
	unsigned char	BUEFE		:1;	/* Burn-Free enabled	*/
	unsigned char	res_2_13	:3;
	unsigned char	ARSCE		:1;	/* Auto read speed control enabled */
	unsigned char	AWSCD		:1;	/* Auto write speed control disabled */
	unsigned char	res_3_67	:2;
	unsigned char	link_counter[2];	/* Burn-Free link counter */
	unsigned char	res[10];		/* Padding up to 16 bytes */
};

#else				/* Motorola bitorder */

struct ricoh_mode_page_30 {
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0xE = 14 Bytes */
	unsigned char	res_2_67	:2;
	unsigned char	AWSCS		:1;	/* Auto write speed control supp. */
	unsigned char	ARSCS		:1;	/* Auto read speed control supp. */
	unsigned char	res_2_23	:2;
	unsigned char	TWBFS		:1;	/* Test Burn-Free sup.	*/
	unsigned char	BUEFS		:1;	/* Burn-Free supported	*/
	unsigned char	res_3_67	:2;
	unsigned char	AWSCD		:1;	/* Auto write speed control disabled */
	unsigned char	ARSCE		:1;	/* Auto read speed control enabled */
	unsigned char	res_2_13	:3;
	unsigned char	BUEFE		:1;	/* Burn-Free enabled	*/
	unsigned char	link_counter[2];	/* Burn-Free link counter */
	unsigned char	res[10];		/* Padding up to 16 bytes */
};
#endif

struct cd_mode_vendor {
	struct scsi_mode_header header;
	union cd_v_pagex {
		struct ricoh_mode_page_30 page30;
	} pagex;
};


#endif	/* _MMCVENDOR_H */
