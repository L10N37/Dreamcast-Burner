/* @(#)scsisense.h	2.18 04/09/04 Copyright 1986 J. Schilling */
/*
 *	Definitions for the SCSI status code and sense structure
 *
 *	Copyright (c) 1986 J. Schilling
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
 * The following exceptions apply:
 * CDDL �3.6 needs to be replaced by: "You may create a Larger Work by
 * combining Covered Software with other code if all other code is governed by
 * the terms of a license that is OSI approved (see www.opensource.org) and
 * you may distribute the Larger Work as a single product. In such a case,
 * You must make sure the requirements of this License are fulfilled for
 * the Covered Software."
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file CDDL.Schily.txt from this distribution.
 */

#ifndef	_SCG_SCSISENSE_H
#define	_SCG_SCSISENSE_H

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * SCSI status completion block.
 */
#define	SCSI_EXTENDED_STATUS

#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct	scsi_status {
	unsigned char	vu_00	: 1;	/* vendor unique */
	unsigned char	chk	: 1;	/* check condition: sense data available */
	unsigned char	cm	: 1;	/* condition met */
	unsigned char	busy	: 1;	/* device busy or reserved */
	unsigned char	is	: 1;	/* intermediate status sent */
	unsigned char	vu_05	: 1;	/* vendor unique */
#define	st_scsi2	vu_05	/* SCSI-2 modifier bit */
	unsigned char	vu_06	: 1;	/* vendor unique */
	unsigned char	st_rsvd	: 1;	/* reserved */

#ifdef	SCSI_EXTENDED_STATUS
#define	ext_st1	st_rsvd		/* extended status (next byte valid) */
	/* byte 1 */
	unsigned char	ha_er	: 1;	/* host adapter detected error */
	unsigned char	reserved: 6;	/* reserved */
	unsigned char	ext_st2	: 1;	/* extended status (next byte valid) */
	/* byte 2 */
	unsigned char	byte2;		/* third byte */
#endif	/* SCSI_EXTENDED_STATUS */
};

#else	/* Motorola byteorder */

struct	scsi_status {
	unsigned char	st_rsvd	: 1;	/* reserved */
	unsigned char	vu_06	: 1;	/* vendor unique */
	unsigned char	vu_05	: 1;	/* vendor unique */
#define	st_scsi2	vu_05	/* SCSI-2 modifier bit */
	unsigned char	is	: 1;	/* intermediate status sent */
	unsigned char	busy	: 1;	/* device busy or reserved */
	unsigned char	cm	: 1;	/* condition met */
	unsigned char	chk	: 1;	/* check condition: sense data available */
	unsigned char	vu_00	: 1;	/* vendor unique */
#ifdef	SCSI_EXTENDED_STATUS
#define	ext_st1	st_rsvd		/* extended status (next byte valid) */
	/* byte 1 */
	unsigned char	ext_st2	: 1;	/* extended status (next byte valid) */
	unsigned char	reserved: 6;	/* reserved */
	unsigned char	ha_er	: 1;	/* host adapter detected error */
	/* byte 2 */
	unsigned char	byte2;		/* third byte */
#endif	/* SCSI_EXTENDED_STATUS */
};
#endif

/*
 * OLD Standard (Non Extended) SCSI Sense. Used mainly by the
 * Adaptec ACB 4000 which is the only controller that
 * does not support the Extended sense format.
 */
#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct	scsi_sense {		/* scsi sense for error classes 0-6 */
	unsigned char	code	: 7;	/* error class/code */
	unsigned char	adr_val	: 1;	/* sense data is valid */
#ifdef	comment
	unsigned char	high_addr:5;	/* high byte of block addr */
	unsigned char	rsvd	: 3;
#else
	unsigned char	high_addr;	/* high byte of block addr */
#endif
	unsigned char	mid_addr;	/* middle byte of block addr */
	unsigned char	low_addr;	/* low byte of block addr */
};

#else	/* Motorola byteorder */

struct	scsi_sense {		/* scsi sense for error classes 0-6 */
	unsigned char	adr_val	: 1;	/* sense data is valid */
	unsigned char	code	: 7;	/* error class/code */
#ifdef	comment
	unsigned char	rsvd	: 3;
	unsigned char	high_addr:5;	/* high byte of block addr */
#else
	unsigned char	high_addr;	/* high byte of block addr */
#endif
	unsigned char	mid_addr;	/* middle byte of block addr */
	unsigned char	low_addr;	/* low byte of block addr */
};
#endif

/*
 * SCSI extended sense parameter block.
 */
#ifdef	comment
#define	SC_CLASS_EXTENDED_SENSE 0x7	/* indicates extended sense */
#endif

#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct	scsi_ext_sense {	/* scsi extended sense for error class 7 */
	/* byte 0 */
	unsigned char	type	: 7;	/* fixed at 0x70 */
	unsigned char	adr_val	: 1;	/* sense data is valid */
	/* byte 1 */
	unsigned char	seg_num;	/* segment number, applies to copy cmd only */
	/* byte 2 */
	unsigned char	key	: 4;	/* sense key, see below */
	unsigned char		: 1;	/* reserved */
	unsigned char	ili	: 1;	/* incorrect length indicator */
	unsigned char	eom	: 1;	/* end of media */
	unsigned char	fil_mk	: 1;	/* file mark on device */
	/* bytes 3 through 7 */
	unsigned char	info_1;		/* information byte 1 */
	unsigned char	info_2;		/* information byte 2 */
	unsigned char	info_3;		/* information byte 3 */
	unsigned char	info_4;		/* information byte 4 */
	unsigned char	add_len;	/* number of additional bytes */
	/* bytes 8 through 13, CCS additions */
	unsigned char	optional_8;	/* CCS search and copy only */
	unsigned char	optional_9;	/* CCS search and copy only */
	unsigned char	optional_10;	/* CCS search and copy only */
	unsigned char	optional_11;	/* CCS search and copy only */
	unsigned char 	sense_code;	/* sense code */
	unsigned char	qual_code;	/* sense code qualifier */
	unsigned char	fru_code;	/* Field replacable unit code */
	unsigned char	bptr	: 3;	/* bit pointer for failure (if bpv) */
	unsigned char	bpv	: 1;	/* bit pointer is valid */
	unsigned char		: 2;
	unsigned char	cd	: 1;	/* pointers refer to command not data */
	unsigned char	sksv	: 1;	/* sense key specific valid */
	unsigned char	field_ptr[2];	/* field pointer for failure */
	unsigned char	add_info[2];	/* round up to 20 bytes */
};

#else	/* Motorola byteorder */

struct	scsi_ext_sense {	/* scsi extended sense for error class 7 */
	/* byte 0 */
	unsigned char	adr_val	: 1;	/* sense data is valid */
	unsigned char	type	: 7;	/* fixed at 0x70 */
	/* byte 1 */
	unsigned char	seg_num;	/* segment number, applies to copy cmd only */
	/* byte 2 */
	unsigned char	fil_mk	: 1;	/* file mark on device */
	unsigned char	eom	: 1;	/* end of media */
	unsigned char	ili	: 1;	/* incorrect length indicator */
	unsigned char		: 1;	/* reserved */
	unsigned char	key	: 4;	/* sense key, see below */
	/* bytes 3 through 7 */
	unsigned char	info_1;		/* information byte 1 */
	unsigned char	info_2;		/* information byte 2 */
	unsigned char	info_3;		/* information byte 3 */
	unsigned char	info_4;		/* information byte 4 */
	unsigned char	add_len;	/* number of additional bytes */
	/* bytes 8 through 13, CCS additions */
	unsigned char	optional_8;	/* CCS search and copy only */
	unsigned char	optional_9;	/* CCS search and copy only */
	unsigned char	optional_10;	/* CCS search and copy only */
	unsigned char	optional_11;	/* CCS search and copy only */
	unsigned char 	sense_code;	/* sense code */
	unsigned char	qual_code;	/* sense code qualifier */
	unsigned char	fru_code;	/* Field replacable unit code */
	unsigned char	sksv	: 1;	/* sense key specific valid */
	unsigned char	cd	: 1;	/* pointers refer to command not data */
	unsigned char		: 2;
	unsigned char	bpv	: 1;	/* bit pointer is valid */
	unsigned char	bptr	: 3;	/* bit pointer for failure (if bpv) */
	unsigned char	field_ptr[2];	/* field pointer for failure */
	unsigned char	add_info[2];	/* round up to 20 bytes */
};
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SCG_SCSISENSE_H */
