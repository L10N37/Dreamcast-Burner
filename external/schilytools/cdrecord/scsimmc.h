/* @(#)scsimmc.h	1.19 06/12/02 Copyright 1997-2006 J. Schilling */
/*
 *	Definitions for SCSI/mmc compliant drives
 *
 *	Copyright (c) 1997-2006 J. Schilling
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

#ifndef	_SCSIMMC_H
#define	_SCSIMMC_H

#include <schily/utypes.h>
#include <schily/btorder.h>

typedef struct opc {
	unsigned char	opc_speed[2];
	unsigned char	opc_val[6];
} opc_t;

#if defined(_BIT_FIELDS_LTOH)	/* Intel bitorder */

struct disk_info {
	unsigned char	data_len[2];		/* Data len without this info	*/
	unsigned char	disk_status	: 2;	/* Status of the disk		*/
	unsigned char	sess_status	: 2;	/* Status of last session	*/
	unsigned char	erasable	: 1;	/* Disk is erasable		*/
	unsigned char	dtype		: 3;	/* Disk information data type	*/
	unsigned char	first_track;		/* # of first track on disk	*/
	unsigned char	numsess;		/* # of sessions		*/
	unsigned char	first_track_ls;		/* First track in last session	*/
	unsigned char	last_track_ls;		/* Last track in last session	*/
	unsigned char	bg_format_stat	: 2;	/* Background format status	*/
	unsigned char	dbit		: 1;	/* Dirty Bit of defect table	*/
	unsigned char	res7_3		: 1;	/* Reserved			*/
	unsigned char	dac_v		: 1;	/* Disk application code valid	*/
	unsigned char	uru		: 1;	/* This is an unrestricted disk	*/
	unsigned char	dbc_v		: 1;	/* Disk bar code valid		*/
	unsigned char	did_v		: 1;	/* Disk id valid		*/
	unsigned char	disk_type;		/* Disk type			*/
	unsigned char	numsess_msb;		/* # of sessions (MSB)		*/
	unsigned char	first_track_ls_msb;	/* First tr. in last ses. (MSB)	*/
	unsigned char	last_track_ls_msb;	/* Last tr. in last ses. (MSB)	*/
	unsigned char	disk_id[4];		/* Disk identification		*/
	unsigned char	last_lead_in[4];	/* Last session lead in time	*/
	unsigned char	last_lead_out[4];	/* Last session lead out time	*/
	unsigned char	disk_barcode[8];	/* Disk bar code		*/
	unsigned char	disk_appl_code;		/* Disk application code	*/
	unsigned char	num_opc_entries;	/* # of OPC table entries	*/
	opc_t	opc_table[1];		/* OPC table 			*/
};

#else				/* Motorola bitorder */

struct disk_info {
	unsigned char	data_len[2];		/* Data len without this info	*/
	unsigned char	dtype		: 3;	/* Disk information data type	*/
	unsigned char	erasable	: 1;	/* Disk is erasable		*/
	unsigned char	sess_status	: 2;	/* Status of last session	*/
	unsigned char	disk_status	: 2;	/* Status of the disk		*/
	unsigned char	first_track;		/* # of first track on disk	*/
	unsigned char	numsess;		/* # of sessions		*/
	unsigned char	first_track_ls;		/* First track in last session	*/
	unsigned char	last_track_ls;		/* Last track in last session	*/
	unsigned char	did_v		: 1;	/* Disk id valid		*/
	unsigned char	dbc_v		: 1;	/* Disk bar code valid		*/
	unsigned char	uru		: 1;	/* This is an unrestricted disk	*/
	unsigned char	dac_v		: 1;	/* Disk application code valid	*/
	unsigned char	res7_3		: 1;	/* Reserved			*/
	unsigned char	dbit		: 1;	/* Dirty Bit of defect table	*/
	unsigned char	bg_format_stat	: 2;	/* Background format status	*/
	unsigned char	disk_type;		/* Disk type			*/
	unsigned char	numsess_msb;		/* # of sessions (MSB)		*/
	unsigned char	first_track_ls_msb;	/* First tr. in last ses. (MSB)	*/
	unsigned char	last_track_ls_msb;	/* Last tr. in last ses. (MSB)	*/
	unsigned char	disk_id[4];		/* Disk identification		*/
	unsigned char	last_lead_in[4];	/* Last session lead in time	*/
	unsigned char	last_lead_out[4];	/* Last session lead out time	*/
	unsigned char	disk_barcode[8];	/* Disk bar code		*/
	unsigned char	disk_appl_code;		/* Disk application code	*/
	unsigned char	num_opc_entries;	/* # of OPC table entries	*/
	opc_t	opc_table[1];		/* OPC table 			*/
};

#endif

struct cd_mode_data {
	struct scsi_mode_header	header;
	union cd_pagex	{
		struct cd_mode_page_05	page05;
		struct cd_mode_page_2A	page2A;
	} pagex;
};

struct tocheader {
	unsigned char	len[2];
	unsigned char	first;
	unsigned char	last;
};

/*
 * Full TOC entry
 */
struct ftrackdesc {
	unsigned char	sess_number;

#if defined(_BIT_FIELDS_LTOH)		/* Intel byteorder */
	unsigned char	control		: 4;
	unsigned char	adr		: 4;
#else					/* Motorola byteorder */
	unsigned char	adr		: 4;
	unsigned char	control		: 4;
#endif

	unsigned char	track;
	unsigned char	point;
	unsigned char	amin;
	unsigned char	asec;
	unsigned char	aframe;
	unsigned char	res7;
	unsigned char	pmin;
	unsigned char	psec;
	unsigned char	pframe;
};

struct fdiskinfo {
	struct tocheader	hd;
	struct ftrackdesc	desc[1];
};



#if defined(_BIT_FIELDS_LTOH)	/* Intel bitorder */

struct atipdesc {
	unsigned char	ref_speed	: 3;	/* Reference speed		*/
	unsigned char	res4_3		: 1;	/* Reserved			*/
	unsigned char	ind_wr_power	: 3;	/* Indicative tgt writing power	*/
	unsigned char	res4_7		: 1;	/* Reserved (must be "1")	*/
	unsigned char	res5_05		: 6;	/* Reserved			*/
	unsigned char	uru		: 1;	/* Disk is for unrestricted use	*/
	unsigned char	res5_7		: 1;	/* Reserved (must be "0")	*/
	unsigned char	a3_v		: 1;	/* A 3 Values valid		*/
	unsigned char	a2_v		: 1;	/* A 2 Values valid		*/
	unsigned char	a1_v		: 1;	/* A 1 Values valid		*/
	unsigned char	sub_type	: 3;	/* Disc sub type		*/
	unsigned char	erasable	: 1;	/* Disk is erasable		*/
	unsigned char	res6_7		: 1;	/* Reserved (must be "1")	*/
	unsigned char	lead_in[4];		/* Lead in time			*/
	unsigned char	lead_out[4];		/* Lead out time		*/
	unsigned char	res15;			/* Reserved			*/
	unsigned char	clv_high	: 4;	/* Highes usable CLV recording speed */
	unsigned char	clv_low		: 3;	/* Lowest usable CLV recording speed */
	unsigned char	res16_7		: 1;	/* Reserved (must be "0")	*/
	unsigned char	res17_0		: 1;	/* Reserved			*/
	unsigned char	tgt_y_pow	: 3;	/* Tgt y val of the power mod fun */
	unsigned char	power_mult	: 3;	/* Power multiplication factor	*/
	unsigned char	res17_7		: 1;	/* Reserved (must be "0")	*/
	unsigned char	res_18_30	: 4;	/* Reserved			*/
	unsigned char	rerase_pwr_ratio: 3;	/* Recommended erase/write power*/
	unsigned char	res18_7		: 1;	/* Reserved (must be "1")	*/
	unsigned char	res19;			/* Reserved			*/
	unsigned char	a2[3];			/* A 2 Values			*/
	unsigned char	res23;			/* Reserved			*/
	unsigned char	a3[3];			/* A 3 Vaules			*/
	unsigned char	res27;			/* Reserved			*/
};

#else				/* Motorola bitorder */

struct atipdesc {
	unsigned char	res4_7		: 1;	/* Reserved (must be "1")	*/
	unsigned char	ind_wr_power	: 3;	/* Indicative tgt writing power	*/
	unsigned char	res4_3		: 1;	/* Reserved			*/
	unsigned char	ref_speed	: 3;	/* Reference speed		*/
	unsigned char	res5_7		: 1;	/* Reserved (must be "0")	*/
	unsigned char	uru		: 1;	/* Disk is for unrestricted use	*/
	unsigned char	res5_05		: 6;	/* Reserved			*/
	unsigned char	res6_7		: 1;	/* Reserved (must be "1")	*/
	unsigned char	erasable	: 1;	/* Disk is erasable		*/
	unsigned char	sub_type	: 3;	/* Disc sub type		*/
	unsigned char	a1_v		: 1;	/* A 1 Values valid		*/
	unsigned char	a2_v		: 1;	/* A 2 Values valid		*/
	unsigned char	a3_v		: 1;	/* A 3 Values valid		*/
	unsigned char	lead_in[4];		/* Lead in time			*/
	unsigned char	lead_out[4];		/* Lead out time		*/
	unsigned char	res15;			/* Reserved			*/
	unsigned char	res16_7		: 1;	/* Reserved (must be "0")	*/
	unsigned char	clv_low		: 3;	/* Lowest usable CLV recording speed */
	unsigned char	clv_high	: 4;	/* Highes usable CLV recording speed */
	unsigned char	res17_7		: 1;	/* Reserved (must be "0")	*/
	unsigned char	power_mult	: 3;	/* Power multiplication factor	*/
	unsigned char	tgt_y_pow	: 3;	/* Tgt y val of the power mod fun */
	unsigned char	res17_0		: 1;	/* Reserved			*/
	unsigned char	res18_7		: 1;	/* Reserved (must be "1")	*/
	unsigned char	rerase_pwr_ratio: 3;	/* Recommended erase/write power*/
	unsigned char	res_18_30	: 4;	/* Reserved			*/
	unsigned char	res19;			/* Reserved			*/
	unsigned char	a2[3];			/* A 2 Values			*/
	unsigned char	res23;			/* Reserved			*/
	unsigned char	a3[3];			/* A 3 Vaules			*/
	unsigned char	res27;			/* Reserved			*/
};

#endif

struct atipinfo {
	struct tocheader	hd;
	struct atipdesc		desc;
};

/*
 * XXX Check how we may merge Track_info & Rzone_info
 */
#if defined(_BIT_FIELDS_LTOH)	/* Intel bitorder */

struct track_info {
	unsigned char	data_len[2];		/* Data len without this info	*/
	unsigned char	track_number;		/* Track number for this info	*/
	unsigned char	session_number;		/* Session number for this info	*/
	unsigned char	res4;			/* Reserved			*/
	unsigned char	track_mode	: 4;	/* Track mode (Q-sub control)	*/
	unsigned char	copy		: 1;	/* This track is a higher copy	*/
	unsigned char	damage		: 1;	/* if 1 & nwa_valid 0: inc track*/
	unsigned char	res5_67		: 2;	/* Reserved			*/
	unsigned char	data_mode	: 4;	/* Data mode of this track	*/
	unsigned char	fp		: 1;	/* This is a fixed packet track	*/
	unsigned char	packet		: 1;	/* This track is in packet mode	*/
	unsigned char	blank		: 1;	/* This is an invisible track	*/
	unsigned char	rt		: 1;	/* This is a reserved track	*/
	unsigned char	nwa_valid	: 1;	/* Next writable addr valid	*/
	unsigned char	res7_17		: 7;	/* Reserved			*/
	unsigned char	track_start[4];		/* Track start address		*/
	unsigned char	next_writable_addr[4];	/* Next writable address	*/
	unsigned char	free_blocks[4];		/* Free usr blocks in this track*/
	unsigned char	packet_size[4];		/* Packet size if in fixed mode	*/
	unsigned char	track_size[4];		/* # of user data blocks in trk	*/
};

#else				/* Motorola bitorder */

struct track_info {
	unsigned char	data_len[2];		/* Data len without this info	*/
	unsigned char	track_number;		/* Track number for this info	*/
	unsigned char	session_number;		/* Session number for this info	*/
	unsigned char	res4;			/* Reserved			*/
	unsigned char	res5_67		: 2;	/* Reserved			*/
	unsigned char	damage		: 1;	/* if 1 & nwa_valid 0: inc track*/
	unsigned char	copy		: 1;	/* This track is a higher copy	*/
	unsigned char	track_mode	: 4;	/* Track mode (Q-sub control)	*/
	unsigned char	rt		: 1;	/* This is a reserved track	*/
	unsigned char	blank		: 1;	/* This is an invisible track	*/
	unsigned char	packet		: 1;	/* This track is in packet mode	*/
	unsigned char	fp		: 1;	/* This is a fixed packet track	*/
	unsigned char	data_mode	: 4;	/* Data mode of this track	*/
	unsigned char	res7_17		: 7;	/* Reserved			*/
	unsigned char	nwa_valid	: 1;	/* Next writable addr valid	*/
	unsigned char	track_start[4];		/* Track start address		*/
	unsigned char	next_writable_addr[4];	/* Next writable address	*/
	unsigned char	free_blocks[4];		/* Free usr blocks in this track*/
	unsigned char	packet_size[4];		/* Packet size if in fixed mode	*/
	unsigned char	track_size[4];		/* # of user data blocks in trk	*/
};

#endif

/*
 * XXX Check how we may merge Track_info & Rzone_info
 */
#if defined(_BIT_FIELDS_LTOH)	/* Intel bitorder */

struct rzone_info {
	unsigned char	data_len[2];		/* Data len without this info	*/
	unsigned char	rzone_num_lsb;		/* RZone number LSB		*/
	unsigned char	border_num_lsb;		/* Border number LSB		*/
	unsigned char	res_4;			/* Reserved			*/
	unsigned char	trackmode	: 4;	/* Track mode			*/
	unsigned char	copy		: 1;	/* Higher generation CD copy	*/
	unsigned char	damage		: 1;	/* Damaged RZone		*/
	unsigned char	ljrs		: 2;	/* Layer jump recording status	*/
	unsigned char	datamode	: 4;	/* Data mode			*/
	unsigned char	fp		: 1;	/* Fixed packet			*/
	unsigned char	incremental	: 1;	/* RZone is to be written incremental */
	unsigned char	blank		: 1;	/* RZone is blank		*/
	unsigned char	rt		: 1;	/* RZone is reserved		*/
	unsigned char	nwa_v		: 1;	/* Next WR address is valid	*/
	unsigned char	lra_v		: 1;	/* Last rec address is valid	*/
	unsigned char	res7_27		: 6;	/* Reserved			*/
	unsigned char	rzone_start[4];		/* RZone start address		*/
	unsigned char	next_recordable_addr[4]; /* Next recordable address	*/
	unsigned char	free_blocks[4];		/* Free blocks in RZone		*/
	unsigned char	block_factor[4];	/* # of sectors of disc acc unit */
	unsigned char	rzone_size[4];		/* RZone size			*/
	unsigned char	last_recorded_addr[4];	/* Last Recorded addr in RZone	*/
	unsigned char	rzone_num_msb;		/* RZone number MSB		*/
	unsigned char	border_num_msb;		/* Border number MSB		*/
	unsigned char	res_34_35[2];		/* Reserved			*/
	unsigned char	read_compat_lba[4];	/* Read Compatibilty LBA	*/
	unsigned char	next_layer_jump[4];	/* Next layer jump address	*/
	unsigned char	last_layer_jump[4];	/* Last layer jump address	*/
};

#else				/* Motorola bitorder */

struct rzone_info {
	unsigned char	data_len[2];		/* Data len without this info	*/
	unsigned char	rzone_num_lsb;		/* RZone number LSB		*/
	unsigned char	border_num_lsb;		/* Border number LSB		*/
	unsigned char	res_4;			/* Reserved			*/
	unsigned char	ljrs		: 2;	/* Layer jump recording status	*/
	unsigned char	damage		: 1;	/* Damaged RZone		*/
	unsigned char	copy		: 1;	/* Higher generation CD copy	*/
	unsigned char	trackmode	: 4;	/* Track mode			*/
	unsigned char	rt		: 1;	/* RZone is reserved		*/
	unsigned char	blank		: 1;	/* RZone is blank		*/
	unsigned char	incremental	: 1;	/* RZone is to be written incremental */
	unsigned char	fp		: 1;	/* Fixed packet			*/
	unsigned char	datamode	: 4;	/* Data mode			*/
	unsigned char	res7_27		: 6;	/* Reserved			*/
	unsigned char	lra_v		: 1;	/* Last rec address is valid	*/
	unsigned char	nwa_v		: 1;	/* Next WR address is valid	*/
	unsigned char	rzone_start[4];		/* RZone start address		*/
	unsigned char	next_recordable_addr[4]; /* Next recordable address	*/
	unsigned char	free_blocks[4];		/* Free blocks in RZone		*/
	unsigned char	block_factor[4];	/* # of sectors of disc acc unit */
	unsigned char	rzone_size[4];		/* RZone size			*/
	unsigned char	last_recorded_addr[4];	/* Last Recorded addr in RZone	*/
	unsigned char	rzone_num_msb;		/* RZone number MSB		*/
	unsigned char	border_num_msb;		/* Border number MSB		*/
	unsigned char	res_34_35[2];		/* Reserved			*/
	unsigned char	read_compat_lba[4];	/* Read Compatibilty LBA	*/
	unsigned char	next_layer_jump[4];	/* Next layer jump address	*/
	unsigned char	last_layer_jump[4];	/* Last layer jump address	*/
};

#endif

/*
 * The lrjs values:
 */
#define	LRJS_NONE	0		/* DAO/Incremental/Blank	*/
#define	LRJS_UNSPEC	1		/* WT == LJ but layerjump not set */
#define	LRJS_MANUAL	2		/* Manual layer jump set	*/
#define	LRJS_INTERVAL	3		/* Jump interval size set	*/

#if defined(_BIT_FIELDS_LTOH)	/* Intel bitorder */

struct dvd_structure_00 {
	unsigned char	data_len[2];		/* Data len without this info	*/
	unsigned char	res23[2];		/* Reserved			*/
	unsigned char	book_version	: 4;	/* DVD Book version		*/
	unsigned char	book_type	: 4;	/* DVD Book type		*/
	unsigned char	maximum_rate	: 4;	/* Maximum data rate (coded)	*/
	unsigned char	disc_size	: 4;	/* Disc size (coded)		*/
	unsigned char	layer_type	: 4;	/* Layer type			*/
	unsigned char	track_path	: 1;	/* 0 = parallel, 1 = opposit dir*/
	unsigned char	numlayers	: 2;	/* Number of Layers (0 == 1)	*/
	unsigned char	res2_7		: 1;	/* Reserved			*/
	unsigned char	track_density	: 4;	/* Track density (coded)	*/
	unsigned char	linear_density	: 4;	/* Linear data density (coded)	*/
	unsigned char	res8;			/* Reserved			*/
	unsigned char	phys_start[3];		/* Starting Physical sector #	*/
	unsigned char	res12;			/* Reserved			*/
	unsigned char	phys_end[3];		/* End physical data sector #	*/
	unsigned char	res16;			/* Reserved			*/
	unsigned char	end_layer0[3];		/* End sector # in layer	*/
	unsigned char	res20		: 7;	/* Reserved			*/
	unsigned char	bca		: 1;	/* BCA flag bit			*/
};

#else				/* Motorola bitorder */

struct dvd_structure_00 {
	unsigned char	data_len[2];		/* Data len without this info	*/
	unsigned char	res23[2];		/* Reserved			*/
	unsigned char	book_type	: 4;	/* DVD Book type		*/
	unsigned char	book_version	: 4;	/* DVD Book version		*/
	unsigned char	disc_size	: 4;	/* Disc size (coded)		*/
	unsigned char	maximum_rate	: 4;	/* Maximum data rate (coded)	*/
	unsigned char	res2_7		: 1;	/* Reserved			*/
	unsigned char	numlayers	: 2;	/* Number of Layers (0 == 1)	*/
	unsigned char	track_path	: 1;	/* 0 = parallel, 1 = opposit dir*/
	unsigned char	layer_type	: 4;	/* Layer type			*/
	unsigned char	linear_density	: 4;	/* Linear data density (coded)	*/
	unsigned char	track_density	: 4;	/* Track density (coded)	*/
	unsigned char	res8;			/* Reserved			*/
	unsigned char	phys_start[3];		/* Starting Physical sector #	*/
	unsigned char	res12;			/* Reserved			*/
	unsigned char	phys_end[3];		/* End physical data sector #	*/
	unsigned char	res16;			/* Reserved			*/
	unsigned char	end_layer0[3];		/* End sector # in layer	*/
	unsigned char	bca		: 1;	/* BCA flag bit			*/
	unsigned char	res20		: 7;	/* Reserved			*/
};

#endif

struct dvd_structure_01 {
	unsigned char	data_len[2];		/* Data len without this info	*/
	unsigned char	res23[2];		/* Reserved			*/
	unsigned char	copyr_prot_type;	/* Copyright prot system type	*/
	unsigned char	region_mgt_info;	/* Region management info	*/
	unsigned char	res67[2];		/* Reserved			*/
};

struct dvd_structure_02 {
	unsigned char	data_len[2];		/* Data len without this info	*/
	unsigned char	res23[2];		/* Reserved			*/
	unsigned char	key_data[2048];		/* Disc Key data		*/
};

struct dvd_structure_03 {
	unsigned char	data_len[2];		/* Data len without this info	*/
	unsigned char	res23[2];		/* Reserved			*/
	unsigned char	bca_info[1];		/* BCA information (12-188 bytes)*/
};

struct dvd_structure_04 {
	unsigned char	data_len[2];		/* Data len without this info	*/
	unsigned char	res23[2];		/* Reserved			*/
	unsigned char	man_info[2048];		/* Disc manufacturing info	*/
};

#if defined(_BIT_FIELDS_LTOH)	/* Intel bitorder */

struct dvd_structure_05 {
	unsigned char	data_len[2];		/* Data len without this info	*/
	unsigned char	res23[2];		/* Reserved			*/
	unsigned char	res4_03		: 4;	/* Reserved			*/
	unsigned char	cgms		: 2;	/* CGMS (see below)		*/
	unsigned char	res4_6		: 1;	/* Reserved			*/
	unsigned char	cpm		: 1;	/* This is copyrighted material	*/
	unsigned char	res57[3];		/* Reserved			*/
};

#else				/* Motorola bitorder */

struct dvd_structure_05 {
	unsigned char	data_len[2];		/* Data len without this info	*/
	unsigned char	res23[2];		/* Reserved			*/
	unsigned char	cpm		: 1;	/* This is copyrighted material	*/
	unsigned char	res4_6		: 1;	/* Reserved			*/
	unsigned char	cgms		: 2;	/* CGMS (see below)		*/
	unsigned char	res4_03		: 4;	/* Reserved			*/
	unsigned char	res57[3];		/* Reserved			*/
};

#endif

#define	CGMS_PERMITTED		0	/* Unlimited copy permitted	*/
#define	CGMS_RES		1	/* Reserved			*/
#define	CGMS_ONE_COPY		2	/* One copy permitted		*/
#define	CGMS_NO_COPY		3	/* No copy permitted		*/

struct dvd_structure_0D {
	unsigned char	data_len[2];		/* Data len without this info	*/
	unsigned char	res23[2];		/* Reserved			*/
	unsigned char	last_rma_sector[2];	/* Last recorded RMA sector #	*/
	unsigned char	rmd_bytes[1];		/* Content of Record man area	*/
};

struct dvd_structure_0E {
	unsigned char	data_len[2];		/* Data len without this info	*/
	unsigned char	res23[2];		/* Reserved			*/
	unsigned char	field_id;		/* Field ID (1)			*/
	unsigned char	application_code;	/* Disc Application code	*/
	unsigned char	phys_data;		/* Disc Phisical Data		*/
	unsigned char	last_recordable_addr[3]; /* Last addr of recordable area */
	unsigned char	res_a[2];		/* Reserved			*/
	unsigned char	field_id_2;		/* Field ID (2)			*/
	unsigned char	ind_wr_power;		/* Recommended writing power	*/
	unsigned char	ind_wavelength;		/* Wavelength for ind_wr_power	*/
	unsigned char	opt_wr_strategy[4];	/* Optimum write Strategy	*/
	unsigned char	res_b[1];		/* Reserved			*/
	unsigned char	field_id_3;		/* Field ID (3)			*/
	unsigned char	man_id[6];		/* Manufacturer ID		*/
	unsigned char	res_m1;			/* Reserved			*/
	unsigned char	field_id_4;		/* Field ID (4)			*/
	unsigned char	man_id2[6];		/* Manufacturer ID		*/
	unsigned char	res_m2;			/* Reserved			*/
};

struct dvd_structure_0F {
	unsigned char	data_len[2];		/* Data len without this info	*/
	unsigned char	res23[2];		/* Reserved			*/
	unsigned char	res45[2];		/* Reserved			*/
	unsigned char	random[2];		/* Random number		*/
	unsigned char	year[4];		/* Year (ascii)			*/
	unsigned char	month[2];		/* Month (ascii)		*/
	unsigned char	day[2];			/* Day (ascii)			*/
	unsigned char	hour[2];		/* Hour (ascii)			*/
	unsigned char	minute[2];		/* Minute (ascii)		*/
	unsigned char	second[2];		/* Second (ascii)		*/
};

struct dvd_structure_0F_w {
	unsigned char	data_len[2];		/* Data len without this info	*/
	unsigned char	res23[2];		/* Reserved			*/
	unsigned char	res45[2];		/* Reserved			*/
	unsigned char	year[4];		/* Year (ascii)			*/
	unsigned char	month[2];		/* Month (ascii)		*/
	unsigned char	day[2];			/* Day (ascii)			*/
	unsigned char	hour[2];		/* Hour (ascii)			*/
	unsigned char	minute[2];		/* Minute (ascii)		*/
	unsigned char	second[2];		/* Second (ascii)		*/
};

struct dvd_structure_20 {
	unsigned char	data_len[2];		/* Data len without this info	*/
	unsigned char	res23[2];		/* Reserved			*/
	unsigned char	res47[4];		/* Reserved			*/
	unsigned char	l0_area_cap[4];		/* Layer 0 area capacity	*/
};

struct dvd_structure_22 {
	unsigned char	data_len[2];		/* Data len without this info	*/
	unsigned char	res23[2];		/* Reserved			*/
	unsigned char	res47[4];		/* Reserved			*/
	unsigned char	jump_interval_size[4];	/* Jump interval size		*/
};

struct dvd_structure_23 {
	unsigned char	data_len[2];		/* Data len without this info	*/
	unsigned char	res23[2];		/* Reserved			*/
	unsigned char	res47[4];		/* Reserved			*/
	unsigned char	jump_lba[4];		/* Jump logical block address	*/
};

struct mmc_cue {
	unsigned char	cs_ctladr;		/* CTL/ADR for this track	*/
	unsigned char	cs_tno;			/* This track number		*/
	unsigned char	cs_index;		/* Index within this track	*/
	unsigned char	cs_dataform;		/* Data form 			*/
	unsigned char	cs_scms;		/* Serial copy management	*/
	unsigned char	cs_min;			/* Absolute time minutes	*/
	unsigned char	cs_sec;			/* Absolute time seconds	*/
	unsigned char	cs_frame;		/* Absolute time frames		*/
};

struct mmc_performance_header {
	unsigned char	p_datalen[4];		/* Performance Data length	*/
#if defined(_BIT_FIELDS_LTOH)	/* Intel bitorder */
	unsigned char	p_exept		:1;	/* Nominal vs. Exept. conditions*/
	unsigned char	p_write		:1;	/* Write vs. Read performance	*/
	unsigned char	p_res_4		:6;	/* Reserved bits...		*/
#else				/* Motorola bitorder */
	unsigned char	p_res_4		:6;	/* Reserved bits...		*/
	unsigned char	p_write		:1;	/* Write vs. Read performance	*/
	unsigned char	p_exept		:1;	/* Nominal vs. Exept. conditions*/
#endif
	unsigned char	p_res[3];		/* Reserved bytes		*/
};


struct mmc_performance {		/* Type == 00 (nominal)		*/
	unsigned char	start_lba[4];		/* Starting LBA			*/
	unsigned char	start_perf[4];		/* Start Performance		*/
	unsigned char	end_lba[4];		/* Ending LBA			*/
	unsigned char	end_perf[4];		/* Ending Performance		*/
};

struct mmc_exceptions {			/* Type == 00 (exceptions)	*/
	unsigned char	lba[4];			/* LBA				*/
	unsigned char	time[2];		/* Time				*/
};

struct mmc_write_speed {		/* Type == 00 (write speed)	*/
#if defined(_BIT_FIELDS_LTOH)	/* Intel bitorder */
	unsigned char	p_mrw		:1;	/* Suitable for mixed read/write*/
	unsigned char	p_exact		:1;	/* Speed count for whole media	*/
	unsigned char	p_rdd		:1;	/* Media rotational control	*/
	unsigned char	p_wrc		:2;	/* Write rotational control	*/
	unsigned char	p_res		:3;	/* Reserved bits...		*/
#else				/* Motorola bitorder */
	unsigned char	p_res		:3;	/* Reserved bits...		*/
	unsigned char	p_wrc		:2;	/* Write rotational control	*/
	unsigned char	p_rdd		:1;	/* Media rotational control	*/
	unsigned char	p_exact		:1;	/* Speed count for whole media	*/
	unsigned char	p_mrw		:1;	/* Suitable for mixed read/write*/
#endif
	unsigned char	res[3];			/* Reserved Bytes		*/
	unsigned char	end_lba[4];		/* Ending LBA			*/
	unsigned char	read_speed[4];		/* Read Speed			*/
	unsigned char	write_speed[4];		/* Write Speed			*/
};

#define	WRC_DEF_RC	0		/* Media default rotational control */
#define	WRC_CAV		1		/* CAV				    */


struct mmc_streaming {			/* Performance for set streaming*/
#if defined(_BIT_FIELDS_LTOH)	/* Intel bitorder */
	unsigned char	p_ra		:1;	/* Random Acess			*/
	unsigned char	p_exact		:1;	/* Set values exactly		*/
	unsigned char	p_rdd		:1;	/* Restore unit defaults	*/
	unsigned char	p_wrc		:2;	/* Write rotational control	*/
	unsigned char	p_res		:3;	/* Reserved bits...		*/
#else				/* Motorola bitorder */
	unsigned char	p_res		:3;	/* Reserved bits...		*/
	unsigned char	p_wrc		:2;	/* Write rotational control	*/
	unsigned char	p_rdd		:1;	/* Restore unit defaults	*/
	unsigned char	p_exact		:1;	/* Set values exactly		*/
	unsigned char	p_ra		:1;	/* Random Acess			*/
#endif
	unsigned char	res[3];			/* Reserved Bytes		*/
	unsigned char	start_lba[4];		/* Starting LBA			*/
	unsigned char	end_lba[4];		/* Ending LBA			*/
	unsigned char	read_size[4];		/* Read Size			*/
	unsigned char	read_time[4];		/* Read Time			*/
	unsigned char	write_size[4];		/* Write Size			*/
	unsigned char	write_time[4];		/* Write Time			*/
};

#endif	/* _SCSIMMC_H */
