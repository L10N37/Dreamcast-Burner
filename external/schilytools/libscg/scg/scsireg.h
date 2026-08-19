/* @(#)scsireg.h	1.35 12/03/16 Copyright 1987-2011 J. Schilling */
/*
 *	usefull definitions for dealing with CCS SCSI - devices
 *
 *	Copyright (c) 1987-2012 J. Schilling
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

#ifndef	_SCG_SCSIREG_H
#define	_SCG_SCSIREG_H

#include <stdint.h>
#include <schily/utypes.h>
#include <schily/btorder.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct	scsi_inquiry {
	unsigned char	type		: 5;	/*  0 */
	unsigned char	qualifier	: 3;	/*  0 */

	unsigned char	type_modifier	: 7;	/*  1 */
	unsigned char	removable	: 1;	/*  1 */

	unsigned char	ansi_version	: 3;	/*  2 */
	unsigned char	ecma_version	: 3;	/*  2 */
	unsigned char	iso_version	: 2;	/*  2 */

	unsigned char	data_format	: 4;	/*  3 */
	unsigned char	res3_54		: 2;	/*  3 */
	unsigned char	termiop		: 1;	/*  3 */
	unsigned char	aenc		: 1;	/*  3 */

	unsigned char	add_len		: 8;	/*  4 */
	unsigned char	sense_len	: 8;	/*  5 */ /* only Emulex ??? */
	unsigned char	res2		: 8;	/*  6 */

	unsigned char	softreset	: 1;	/*  7 */
	unsigned char	cmdque		: 1;
	unsigned char	res7_2		: 1;
	unsigned char	linked		: 1;
	unsigned char	sync		: 1;
	unsigned char	wbus16		: 1;
	unsigned char	wbus32		: 1;
	unsigned char	reladr		: 1;	/*  7 */

	union {

		struct {
		char	vendor_info[8];		/*  8 */
		char	prod_ident[16];		/* 16 */
		char	prod_revision[4];	/* 32 */
#ifdef	comment
		char	vendor_uniq[20];	/* 36 */
		char	reserved[40];		/* 56 */
#endif
		} vi;
		char	vi_space[8+16+4];
	} vu;
};					/* 96 */

#else					/* Motorola byteorder */

struct	scsi_inquiry {
	unsigned char	qualifier	: 3;	/*  0 */
	unsigned char	type		: 5;	/*  0 */

	unsigned char	removable	: 1;	/*  1 */
	unsigned char	type_modifier	: 7;	/*  1 */

	unsigned char	iso_version	: 2;	/*  2 */
	unsigned char	ecma_version	: 3;
	unsigned char	ansi_version	: 3;	/*  2 */

	unsigned char	aenc		: 1;	/*  3 */
	unsigned char	termiop		: 1;
	unsigned char	res3_54		: 2;
	unsigned char	data_format	: 4;	/*  3 */

	unsigned char	add_len		: 8;	/*  4 */
	unsigned char	sense_len	: 8;	/*  5 */ /* only Emulex ??? */
	unsigned char	res2		: 8;	/*  6 */
	unsigned char	reladr		: 1;	/*  7 */
	unsigned char	wbus32		: 1;
	unsigned char	wbus16		: 1;
	unsigned char	sync		: 1;
	unsigned char	linked		: 1;
	unsigned char	res7_2		: 1;
	unsigned char	cmdque		: 1;
	unsigned char	softreset	: 1;

	union {

		struct {
		char	vendor_info[8];		/*  8 */
		char	prod_ident[16];		/* 16 */
		char	prod_revision[4];	/* 32 */
#ifdef	comment
		char	vendor_uniq[20];	/* 36 */
		char	reserved[40];		/* 56 */
#endif
		} vi;
		char	vi_space[8+16+4];
	} vu;
};					/* 96 */
#endif

#ifdef	__SCG_COMPAT__
#define	info		inq_vendor_info
#define	ident		inq_prod_ident
#define	revision	inq_prod_revision
#endif

#define	inq_vendor_info		vu.vi.vendor_info
#define	inq_prod_ident		vu.vi.prod_ident
#define	inq_prod_revision	vu.vi.prod_revision

#define	inq_info_space		vu.vi_space

/* Peripheral Device Qualifier */

#define	INQ_DEV_PRESENT	0x00		/* Physical device present */
#define	INQ_DEV_NOTPR	0x01		/* Physical device not present */
#define	INQ_DEV_RES	0x02		/* Reserved */
#define	INQ_DEV_NOTSUP	0x03		/* Logical unit not supported */

/* Peripheral Device Type */

#define	INQ_DASD	0x00		/* Direct-access device (disk) */
#define	INQ_SEQD	0x01		/* Sequential-access device (tape) */
#define	INQ_PRTD	0x02 		/* Printer device */
#define	INQ_PROCD	0x03 		/* Processor device */
#define	INQ_OPTD	0x04		/* Write once device (optical disk) */
#define	INQ_WORM	0x04		/* Write once device (optical disk) */
#define	INQ_ROMD	0x05		/* CD-ROM device */
#define	INQ_SCAN	0x06		/* Scanner device */
#define	INQ_OMEM	0x07		/* Optical Memory device */
#define	INQ_JUKE	0x08		/* Medium Changer device (jukebox) */
#define	INQ_COMM	0x09		/* Communications device */
#define	INQ_IT8_1	0x0A		/* IT8 */
#define	INQ_IT8_2	0x0B		/* IT8 */
#define	INQ_STARR	0x0C		/* Storage array device */
#define	INQ_ENCL	0x0D		/* Enclosure services device */
#define	INQ_SDAD	0x0E		/* Simplyfied direct-access device */
#define	INQ_OCRW	0x0F		/* Optical card reader/writer device */
#define	INQ_BRIDGE	0x10		/* Bridging expander device */
#define	INQ_OSD		0x11		/* Object based storage device */
#define	INQ_ADC		0x12		/* Automation/Drive interface */
#define	INQ_WELLKNOWN	0x1E		/* Well known logical unit */
#define	INQ_NODEV	0x1F		/* Unknown or no device */
#define	INQ_NOTPR	0x1F		/* Logical unit not present (SCSI-1) */

#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct scsi_mode_header {
	unsigned char	sense_data_len	: 8;
	unsigned char	medium_type;
	unsigned char	res2		: 4;
	unsigned char	cache		: 1;
	unsigned char	res		: 2;
	unsigned char	write_prot	: 1;
	unsigned char	blockdesc_len;
};

#else					/* Motorola byteorder */

struct scsi_mode_header {
	unsigned char	sense_data_len	: 8;
	unsigned char	medium_type;
	unsigned char	write_prot	: 1;
	unsigned char	res		: 2;
	unsigned char	cache		: 1;
	unsigned char	res2		: 4;
	unsigned char	blockdesc_len;
};
#endif

struct scsi_modesel_header {
	unsigned char	sense_data_len	: 8;
	unsigned char	medium_type;
	unsigned char	res2		: 8;
	unsigned char	blockdesc_len;
};

struct scsi_mode_blockdesc {
	unsigned char	density;
	unsigned char	nlblock[3];
	unsigned char	res		: 8;
	unsigned char	lblen[3];
};

#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct acb_mode_data {
	unsigned char	listformat;
	unsigned char	ncyl[2];
	unsigned char	nhead;
	unsigned char	start_red_wcurrent[2];
	unsigned char	start_precomp[2];
	unsigned char	landing_zone;
	unsigned char	step_rate;
	unsigned char			: 2;
	unsigned char	hard_sec	: 1;
	unsigned char	fixed_media	: 1;
	unsigned char			: 4;
	unsigned char	sect_per_trk;
};

#else					/* Motorola byteorder */

struct acb_mode_data {
	unsigned char	listformat;
	unsigned char	ncyl[2];
	unsigned char	nhead;
	unsigned char	start_red_wcurrent[2];
	unsigned char	start_precomp[2];
	unsigned char	landing_zone;
	unsigned char	step_rate;
	unsigned char			: 4;
	unsigned char	fixed_media	: 1;
	unsigned char	hard_sec	: 1;
	unsigned char			: 2;
	unsigned char	sect_per_trk;
};
#endif

#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct scsi_mode_page_header {
	unsigned char	p_code		: 6;
	unsigned char	res		: 1;
	unsigned char	parsave		: 1;
	unsigned char	p_len;
};

/*
 * This is a hack that allows mode pages without
 * any further bitfileds to be defined bitorder independent.
 */
#define	MP_P_CODE			\
	unsigned char	p_code		: 6;	\
	unsigned char	p_res		: 1;	\
	unsigned char	parsave		: 1

#else					/* Motorola byteorder */

struct scsi_mode_page_header {
	unsigned char	parsave		: 1;
	unsigned char	res		: 1;
	unsigned char	p_code		: 6;
	unsigned char	p_len;
};

/*
 * This is a hack that allows mode pages without
 * any further bitfileds to be defined bitorder independent.
 */
#define	MP_P_CODE			\
	unsigned char	parsave		: 1;	\
	unsigned char	p_res		: 1;	\
	unsigned char	p_code		: 6

#endif

#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct scsi_mode_page_01 {		/* Error recovery Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x0A = 12 Bytes */
	unsigned char	disa_correction	: 1;	/* Byte 2 */
	unsigned char	term_on_rec_err	: 1;
	unsigned char	report_rec_err	: 1;
	unsigned char	en_early_corr	: 1;
	unsigned char	read_continuous	: 1;
	unsigned char	tranfer_block	: 1;
	unsigned char	en_auto_reall_r	: 1;
	unsigned char	en_auto_reall_w	: 1;	/* Byte 2 */
	unsigned char	rd_retry_count;		/* Byte 3 */
	unsigned char	correction_span;
	char	head_offset_count;
	char	data_strobe_offset;
	unsigned char	res;
	unsigned char	wr_retry_count;
	unsigned char	res_tape[2];
	unsigned char	recov_timelim[2];
};

#else					/* Motorola byteorder */

struct scsi_mode_page_01 {		/* Error recovery Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x0A = 12 Bytes */
	unsigned char	en_auto_reall_w	: 1;	/* Byte 2 */
	unsigned char	en_auto_reall_r	: 1;
	unsigned char	tranfer_block	: 1;
	unsigned char	read_continuous	: 1;
	unsigned char	en_early_corr	: 1;
	unsigned char	report_rec_err	: 1;
	unsigned char	term_on_rec_err	: 1;
	unsigned char	disa_correction	: 1;	/* Byte 2 */
	unsigned char	rd_retry_count;		/* Byte 3 */
	unsigned char	correction_span;
	char	head_offset_count;
	char	data_strobe_offset;
	unsigned char	res;
	unsigned char	wr_retry_count;
	unsigned char	res_tape[2];
	unsigned char	recov_timelim[2];
};
#endif


#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct scsi_mode_page_02 {		/* Device dis/re connect Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x0E = 16 Bytes */
	unsigned char	buf_full_ratio;
	unsigned char	buf_empt_ratio;
	unsigned char	bus_inact_limit[2];
	unsigned char	disc_time_limit[2];
	unsigned char	conn_time_limit[2];
	unsigned char	max_burst_size[2];	/* Start SCSI-2 */
	unsigned char	data_tr_dis_ctl	: 2;
	unsigned char			: 6;
	unsigned char	res[3];
};

#else					/* Motorola byteorder */

struct scsi_mode_page_02 {		/* Device dis/re connect Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x0E = 16 Bytes */
	unsigned char	buf_full_ratio;
	unsigned char	buf_empt_ratio;
	unsigned char	bus_inact_limit[2];
	unsigned char	disc_time_limit[2];
	unsigned char	conn_time_limit[2];
	unsigned char	max_burst_size[2];	/* Start SCSI-2 */
	unsigned char			: 6;
	unsigned char	data_tr_dis_ctl	: 2;
	unsigned char	res[3];
};
#endif

#define	DTDC_DATADONE	0x01		/*
					 * Target may not disconnect once
					 * data transfer is started until
					 * all data successfully transferred.
					 */

#define	DTDC_CMDDONE	0x03		/*
					 * Target may not disconnect once
					 * data transfer is started until
					 * command completed.
					 */


#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct scsi_mode_page_03 {		/* Direct access format Paramters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x16 = 24 Bytes */
	unsigned char	trk_per_zone[2];
	unsigned char	alt_sec_per_zone[2];
	unsigned char	alt_trk_per_zone[2];
	unsigned char	alt_trk_per_vol[2];
	unsigned char	sect_per_trk[2];
	unsigned char	bytes_per_phys_sect[2];
	unsigned char	interleave[2];
	unsigned char	trk_skew[2];
	unsigned char	cyl_skew[2];
	unsigned char			: 3;
	unsigned char	inhibit_save	: 1;
	unsigned char	fmt_by_surface	: 1;
	unsigned char	removable	: 1;
	unsigned char	hard_sec	: 1;
	unsigned char	soft_sec	: 1;
	unsigned char	res[3];
};

#else					/* Motorola byteorder */

struct scsi_mode_page_03 {		/* Direct access format Paramters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x16 = 24 Bytes */
	unsigned char	trk_per_zone[2];
	unsigned char	alt_sec_per_zone[2];
	unsigned char	alt_trk_per_zone[2];
	unsigned char	alt_trk_per_vol[2];
	unsigned char	sect_per_trk[2];
	unsigned char	bytes_per_phys_sect[2];
	unsigned char	interleave[2];
	unsigned char	trk_skew[2];
	unsigned char	cyl_skew[2];
	unsigned char	soft_sec	: 1;
	unsigned char	hard_sec	: 1;
	unsigned char	removable	: 1;
	unsigned char	fmt_by_surface	: 1;
	unsigned char	inhibit_save	: 1;
	unsigned char			: 3;
	unsigned char	res[3];
};
#endif

#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct scsi_mode_page_04 {		/* Rigid disk Geometry Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x16 = 24 Bytes */
	unsigned char	ncyl[3];
	unsigned char	nhead;
	unsigned char	start_precomp[3];
	unsigned char	start_red_wcurrent[3];
	unsigned char	step_rate[2];
	unsigned char	landing_zone[3];
	unsigned char	rot_pos_locking	: 2;	/* Start SCSI-2 */
	unsigned char			: 6;	/* Start SCSI-2 */
	unsigned char	rotational_off;
	unsigned char	res1;
	unsigned char	rotation_rate[2];
	unsigned char	res2[2];
};

#else					/* Motorola byteorder */

struct scsi_mode_page_04 {		/* Rigid disk Geometry Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x16 = 24 Bytes */
	unsigned char	ncyl[3];
	unsigned char	nhead;
	unsigned char	start_precomp[3];
	unsigned char	start_red_wcurrent[3];
	unsigned char	step_rate[2];
	unsigned char	landing_zone[3];
	unsigned char			: 6;	/* Start SCSI-2 */
	unsigned char	rot_pos_locking	: 2;	/* Start SCSI-2 */
	unsigned char	rotational_off;
	unsigned char	res1;
	unsigned char	rotation_rate[2];
	unsigned char	res2[2];
};
#endif

#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct scsi_mode_page_05 {		/* Flexible disk Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x1E = 32 Bytes */
	unsigned char	transfer_rate[2];
	unsigned char	nhead;
	unsigned char	sect_per_trk;
	unsigned char	bytes_per_phys_sect[2];
	unsigned char	ncyl[2];
	unsigned char	start_precomp[2];
	unsigned char	start_red_wcurrent[2];
	unsigned char	step_rate[2];
	unsigned char	step_pulse_width;
	unsigned char	head_settle_delay[2];
	unsigned char	motor_on_delay;
	unsigned char	motor_off_delay;
	unsigned char	spc		: 4;
	unsigned char			: 4;
	unsigned char			: 5;
	unsigned char	mo		: 1;
	unsigned char	ssn		: 1;
	unsigned char	trdy		: 1;
	unsigned char	write_compensation;
	unsigned char	head_load_delay;
	unsigned char	head_unload_delay;
	unsigned char	pin_2_use	: 4;
	unsigned char	pin_34_use	: 4;
	unsigned char	pin_1_use	: 4;
	unsigned char	pin_4_use	: 4;
	unsigned char	rotation_rate[2];
	unsigned char	res[2];
};

#else					/* Motorola byteorder */

struct scsi_mode_page_05 {		/* Flexible disk Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x1E = 32 Bytes */
	unsigned char	transfer_rate[2];
	unsigned char	nhead;
	unsigned char	sect_per_trk;
	unsigned char	bytes_per_phys_sect[2];
	unsigned char	ncyl[2];
	unsigned char	start_precomp[2];
	unsigned char	start_red_wcurrent[2];
	unsigned char	step_rate[2];
	unsigned char	step_pulse_width;
	unsigned char	head_settle_delay[2];
	unsigned char	motor_on_delay;
	unsigned char	motor_off_delay;
	unsigned char	trdy		: 1;
	unsigned char	ssn		: 1;
	unsigned char	mo		: 1;
	unsigned char			: 5;
	unsigned char			: 4;
	unsigned char	spc		: 4;
	unsigned char	write_compensation;
	unsigned char	head_load_delay;
	unsigned char	head_unload_delay;
	unsigned char	pin_34_use	: 4;
	unsigned char	pin_2_use	: 4;
	unsigned char	pin_4_use	: 4;
	unsigned char	pin_1_use	: 4;
	unsigned char	rotation_rate[2];
	unsigned char	res[2];
};
#endif

#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct scsi_mode_page_07 {		/* Verify Error recovery */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x0A = 12 Bytes */
	unsigned char	disa_correction	: 1;	/* Byte 2 */
	unsigned char	term_on_rec_err	: 1;
	unsigned char	report_rec_err	: 1;
	unsigned char	en_early_corr	: 1;
	unsigned char	res		: 4;	/* Byte 2 */
	unsigned char	ve_retry_count;		/* Byte 3 */
	unsigned char	ve_correction_span;
	char	res2[5];		/* Byte 5 */
	unsigned char	ve_recov_timelim[2];	/* Byte 10 */
};

#else					/* Motorola byteorder */

struct scsi_mode_page_07 {		/* Verify Error recovery */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x0A = 12 Bytes */
	unsigned char	res		: 4;	/* Byte 2 */
	unsigned char	en_early_corr	: 1;
	unsigned char	report_rec_err	: 1;
	unsigned char	term_on_rec_err	: 1;
	unsigned char	disa_correction	: 1;	/* Byte 2 */
	unsigned char	ve_retry_count;		/* Byte 3 */
	unsigned char	ve_correction_span;
	char	res2[5];		/* Byte 5 */
	unsigned char	ve_recov_timelim[2];	/* Byte 10 */
};
#endif

#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct scsi_mode_page_08 {		/* Caching Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x0A = 12 Bytes */
	unsigned char	disa_rd_cache	: 1;	/* Byte 2 */
	unsigned char	muliple_fact	: 1;
	unsigned char	en_wt_cache	: 1;
	unsigned char	res		: 5;	/* Byte 2 */
	unsigned char	wt_ret_pri	: 4;	/* Byte 3 */
	unsigned char	demand_rd_ret_pri: 4;	/* Byte 3 */
	unsigned char	disa_pref_tr_len[2];	/* Byte 4 */
	unsigned char	min_pref[2];		/* Byte 6 */
	unsigned char	max_pref[2];		/* Byte 8 */
	unsigned char	max_pref_ceiling[2];	/* Byte 10 */
};

#else					/* Motorola byteorder */

struct scsi_mode_page_08 {		/* Caching Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x0A = 12 Bytes */
	unsigned char	res		: 5;	/* Byte 2 */
	unsigned char	en_wt_cache	: 1;
	unsigned char	muliple_fact	: 1;
	unsigned char	disa_rd_cache	: 1;	/* Byte 2 */
	unsigned char	demand_rd_ret_pri: 4;	/* Byte 3 */
	unsigned char	wt_ret_pri	: 4;
	unsigned char	disa_pref_tr_len[2];	/* Byte 4 */
	unsigned char	min_pref[2];		/* Byte 6 */
	unsigned char	max_pref[2];		/* Byte 8 */
	unsigned char	max_pref_ceiling[2];	/* Byte 10 */
};
#endif

struct scsi_mode_page_09 {		/* Peripheral device Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* >= 0x06 = 8 Bytes */
	unsigned char	interface_id[2];	/* Byte 2 */
	unsigned char	res[4];			/* Byte 4 */
	unsigned char	vendor_specific[1];	/* Byte 8 */
};

#define	PDEV_SCSI	0x0000		/* scsi interface */
#define	PDEV_SMD	0x0001		/* SMD interface */
#define	PDEV_ESDI	0x0002		/* ESDI interface */
#define	PDEV_IPI2	0x0003		/* IPI-2 interface */
#define	PDEV_IPI3	0x0004		/* IPI-3 interface */

#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct scsi_mode_page_0A {		/* Common device Control Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x06 = 8 Bytes */
	unsigned char	rep_log_exeption: 1;	/* Byte 2 */
	unsigned char	res		: 7;	/* Byte 2 */
	unsigned char	dis_queuing	: 1;	/* Byte 3 */
	unsigned char	queuing_err_man	: 1;
	unsigned char	res2		: 2;
	unsigned char	queue_alg_mod	: 4;	/* Byte 3 */
	unsigned char	EAENP		: 1;	/* Byte 4 */
	unsigned char	UAENP		: 1;
	unsigned char	RAENP		: 1;
	unsigned char	res3		: 4;
	unsigned char	en_ext_cont_all	: 1;	/* Byte 4 */
	unsigned char	res4		: 8;
	unsigned char	ready_aen_hold_per[2];	/* Byte 6 */
};

#else					/* Motorola byteorder */

struct scsi_mode_page_0A {		/* Common device Control Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x06 = 8 Bytes */
	unsigned char	res		: 7;	/* Byte 2 */
	unsigned char	rep_log_exeption: 1;	/* Byte 2 */
	unsigned char	queue_alg_mod	: 4;	/* Byte 3 */
	unsigned char	res2		: 2;
	unsigned char	queuing_err_man	: 1;
	unsigned char	dis_queuing	: 1;	/* Byte 3 */
	unsigned char	en_ext_cont_all	: 1;	/* Byte 4 */
	unsigned char	res3		: 4;
	unsigned char	RAENP		: 1;
	unsigned char	UAENP		: 1;
	unsigned char	EAENP		: 1;	/* Byte 4 */
	unsigned char	res4		: 8;
	unsigned char	ready_aen_hold_per[2];	/* Byte 6 */
};
#endif

#define	CTRL_QMOD_RESTRICT	0x0
#define	CTRL_QMOD_UNRESTRICT	0x1


struct scsi_mode_page_0B {		/* Medium Types Supported Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x06 = 8 Bytes */
	unsigned char	res[2];			/* Byte 2 */
	unsigned char	medium_one_supp;	/* Byte 4 */
	unsigned char	medium_two_supp;	/* Byte 5 */
	unsigned char	medium_three_supp;	/* Byte 6 */
	unsigned char	medium_four_supp;	/* Byte 7 */
};

#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct scsi_mode_page_0C {		/* Notch & Partition Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x16 = 24 Bytes */
	unsigned char	res		: 6;	/* Byte 2 */
	unsigned char	logical_notch	: 1;
	unsigned char	notched_drive	: 1;	/* Byte 2 */
	unsigned char	res2;			/* Byte 3 */
	unsigned char	max_notches[2];		/* Byte 4  */
	unsigned char	active_notch[2];	/* Byte 6  */
	unsigned char	starting_boundary[4];	/* Byte 8  */
	unsigned char	ending_boundary[4];	/* Byte 12 */
	unsigned char	pages_notched[8];	/* Byte 16 */
};

#else					/* Motorola byteorder */

struct scsi_mode_page_0C {		/* Notch & Partition Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x16 = 24 Bytes */
	unsigned char	notched_drive	: 1;	/* Byte 2 */
	unsigned char	logical_notch	: 1;
	unsigned char	res		: 6;	/* Byte 2 */
	unsigned char	res2;			/* Byte 3 */
	unsigned char	max_notches[2];		/* Byte 4  */
	unsigned char	active_notch[2];	/* Byte 6  */
	unsigned char	starting_boundary[4];	/* Byte 8  */
	unsigned char	ending_boundary[4];	/* Byte 12 */
	unsigned char	pages_notched[8];	/* Byte 16 */
};
#endif

#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct scsi_mode_page_0D {		/* CD-ROM Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x06 = 8 Bytes */
	unsigned char	res;			/* Byte 2 */
	unsigned char	inact_timer_mult: 4;	/* Byte 3 */
	unsigned char	res2		: 4;	/* Byte 3 */
	unsigned char	s_un_per_m_un[2];	/* Byte 4  */
	unsigned char	f_un_per_s_un[2];	/* Byte 6  */
};

#else					/* Motorola byteorder */

struct scsi_mode_page_0D {		/* CD-ROM Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x06 = 8 Bytes */
	unsigned char	res;			/* Byte 2 */
	unsigned char	res2		: 4;	/* Byte 3 */
	unsigned char	inact_timer_mult: 4;	/* Byte 3 */
	unsigned char	s_un_per_m_un[2];	/* Byte 4  */
	unsigned char	f_un_per_s_un[2];	/* Byte 6  */
};
#endif

struct sony_mode_page_20 {		/* Sony Format Mode Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x0A = 12 Bytes */
	unsigned char	format_mode;
	unsigned char	format_type;
#define	num_bands	user_band_size	/* Gilt bei Type 1 */
	unsigned char	user_band_size[4];	/* Gilt bei Type 0 */
	unsigned char	spare_band_size[2];
	unsigned char	res[2];
};

#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct toshiba_mode_page_20 {		/* Toshiba Speed Control Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x01 = 3 Bytes */
	unsigned char	speed		: 1;
	unsigned char	res		: 7;
};

#else					/* Motorola byteorder */

struct toshiba_mode_page_20 {		/* Toshiba Speed Control Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x01 = 3 Bytes */
	unsigned char	res		: 7;
	unsigned char	speed		: 1;
};
#endif

#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct ccs_mode_page_38 {		/* CCS Caching Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x0E = 14 Bytes */

	unsigned char	cache_table_size: 4;	/* Byte 3 */
	unsigned char	cache_en	: 1;
	unsigned char	res2		: 1;
	unsigned char	wr_index_en	: 1;
	unsigned char	res		: 1;	/* Byte 3 */
	unsigned char	threshold;		/* Byte 4 Prefetch threshold */
	unsigned char	max_prefetch;		/* Byte 5 Max. prefetch */
	unsigned char	max_multiplier;		/* Byte 6 Max. prefetch multiplier */
	unsigned char	min_prefetch;		/* Byte 7 Min. prefetch */
	unsigned char	min_multiplier;		/* Byte 8 Min. prefetch multiplier */
	unsigned char	res3[8];		/* Byte 9 */
};

#else					/* Motorola byteorder */

struct ccs_mode_page_38 {		/* CCS Caching Parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x0E = 14 Bytes */

	unsigned char	res		: 1;	/* Byte 3 */
	unsigned char	wr_index_en	: 1;
	unsigned char	res2		: 1;
	unsigned char	cache_en	: 1;
	unsigned char	cache_table_size: 4;	/* Byte 3 */
	unsigned char	threshold;		/* Byte 4 Prefetch threshold */
	unsigned char	max_prefetch;		/* Byte 5 Max. prefetch */
	unsigned char	max_multiplier;		/* Byte 6 Max. prefetch multiplier */
	unsigned char	min_prefetch;		/* Byte 7 Min. prefetch */
	unsigned char	min_multiplier;		/* Byte 8 Min. prefetch multiplier */
	unsigned char	res3[8];		/* Byte 9 */
};
#endif

#if defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct cd_mode_page_05 {		/* write parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x32 = 50 Bytes */
	unsigned char	write_type	: 4;	/* Session write type (PACKET/TAO...)*/
	unsigned char	test_write	: 1;	/* Do not actually write data	    */
	unsigned char	LS_V		: 1;	/* Link size valid		    */
	unsigned char	BUFE		: 1;	/* Enable Bufunderrun free rec.	    */
	unsigned char	res_2_7		: 1;
	unsigned char	track_mode	: 4;	/* Track mode (Q-sub control nibble) */
	unsigned char	copy		: 1;	/* 1st higher gen of copy prot track ~*/
	unsigned char	fp		: 1;	/* Fixed packed (if in packet mode) */
	unsigned char	multi_session	: 2;	/* Multi session write type	    */
	unsigned char	dbtype		: 4;	/* Data block type		    */
	unsigned char	res_4		: 4;	/* Reserved			    */
	unsigned char	link_size;		/* Link Size (default is 7)	    */
	unsigned char	res_6;			/* Reserved			    */
	unsigned char	host_appl_code	: 6;	/* Host application code of disk    */
	unsigned char	res_7		: 2;	/* Reserved			    */
	unsigned char	session_format;		/* Session format (DA/CDI/XA)	    */
	unsigned char	res_9;			/* Reserved			    */
	unsigned char	packet_size[4];		/* # of user datablocks/fixed packet */
	unsigned char	audio_pause_len[2];	/* # of blocks where index is zero  */
	unsigned char	media_cat_number[16];	/* Media catalog Number (MCN)	    */
	unsigned char	ISRC[14];		/* ISRC for this track		    */
	unsigned char	sub_header[4];
	unsigned char	vendor_uniq[4];
};

#else				/* Motorola byteorder */

struct cd_mode_page_05 {		/* write parameters */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x32 = 50 Bytes */
	unsigned char	res_2_7		: 1;
	unsigned char	BUFE		: 1;	/* Enable Bufunderrun free rec.	    */
	unsigned char	LS_V		: 1;	/* Link size valid		    */
	unsigned char	test_write	: 1;	/* Do not actually write data	    */
	unsigned char	write_type	: 4;	/* Session write type (PACKET/TAO...)*/
	unsigned char	multi_session	: 2;	/* Multi session write type	    */
	unsigned char	fp		: 1;	/* Fixed packed (if in packet mode) */
	unsigned char	copy		: 1;	/* 1st higher gen of copy prot track */
	unsigned char	track_mode	: 4;	/* Track mode (Q-sub control nibble) */
	unsigned char	res_4		: 4;	/* Reserved			    */
	unsigned char	dbtype		: 4;	/* Data block type		    */
	unsigned char	link_size;		/* Link Size (default is 7)	    */
	unsigned char	res_6;			/* Reserved			    */
	unsigned char	res_7		: 2;	/* Reserved			    */
	unsigned char	host_appl_code	: 6;	/* Host application code of disk    */
	unsigned char	session_format;		/* Session format (DA/CDI/XA)	    */
	unsigned char	res_9;			/* Reserved			    */
	unsigned char	packet_size[4];		/* # of user datablocks/fixed packet */
	unsigned char	audio_pause_len[2];	/* # of blocks where index is zero  */
	unsigned char	media_cat_number[16];	/* Media catalog Number (MCN)	    */
	unsigned char	ISRC[14];		/* ISRC for this track		    */
	unsigned char	sub_header[4];
	unsigned char	vendor_uniq[4];
};

#endif

#if defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct cd_wr_speed_performance {
	unsigned char	res0;			/* Reserved			    */
	unsigned char	rot_ctl_sel	: 2;	/* Rotational control selected	    */
	unsigned char	res_1_27	: 6;	/* Reserved			    */
	unsigned char	wr_speed_supp[2];	/* Supported write speed	    */
};

struct cd_mode_page_2A {		/* CD Cap / mech status */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x14 = 20 Bytes (MMC) */
					/* 0x18 = 24 Bytes (MMC-2) */
					/* 0x1C >= 28 Bytes (MMC-3) */
	unsigned char	cd_r_read	: 1;	/* Reads CD-R  media		    */
	unsigned char	cd_rw_read	: 1;	/* Reads CD-RW media		    */
	unsigned char	method2		: 1;	/* Reads fixed packet method2 media */
	unsigned char	dvd_rom_read	: 1;	/* Reads DVD ROM media		    */
	unsigned char	dvd_r_read	: 1;	/* Reads DVD-R media		    */
	unsigned char	dvd_ram_read	: 1;	/* Reads DVD-RAM media		    */
	unsigned char	res_2_67	: 2;	/* Reserved			    */
	unsigned char	cd_r_write	: 1;	/* Supports writing CD-R  media	    */
	unsigned char	cd_rw_write	: 1;	/* Supports writing CD-RW media	    */
	unsigned char	test_write	: 1;	/* Supports emulation write	    */
	unsigned char	res_3_3		: 1;	/* Reserved			    */
	unsigned char	dvd_r_write	: 1;	/* Supports writing DVD-R media	    */
	unsigned char	dvd_ram_write	: 1;	/* Supports writing DVD-RAM media   */
	unsigned char	res_3_67	: 2;	/* Reserved			    */
	unsigned char	audio_play	: 1;	/* Supports Audio play operation    */
	unsigned char	composite	: 1;	/* Deliveres composite A/V stream   */
	unsigned char	digital_port_2	: 1;	/* Supports digital output on port 2 */
	unsigned char	digital_port_1	: 1;	/* Supports digital output on port 1 */
	unsigned char	mode_2_form_1	: 1;	/* Reads Mode-2 form 1 media (XA)   */
	unsigned char	mode_2_form_2	: 1;	/* Reads Mode-2 form 2 media	    */
	unsigned char	multi_session	: 1;	/* Reads multi-session media	    */
	unsigned char	BUF		: 1;	/* Supports Buffer under. free rec. */
	unsigned char	cd_da_supported	: 1;	/* Reads audio data with READ CD cmd */
	unsigned char	cd_da_accurate	: 1;	/* READ CD data stream is accurate   */
	unsigned char	rw_supported	: 1;	/* Reads R-W sub channel information */
	unsigned char	rw_deint_corr	: 1;	/* Reads de-interleved R-W sub chan  */
	unsigned char	c2_pointers	: 1;	/* Supports C2 error pointers	    */
	unsigned char	ISRC		: 1;	/* Reads ISRC information	    */
	unsigned char	UPC		: 1;	/* Reads media catalog number (UPC) */
	unsigned char	read_bar_code	: 1;	/* Supports reading bar codes	    */
	unsigned char	lock		: 1;	/* PREVENT/ALLOW may lock media	    */
	unsigned char	lock_state	: 1;	/* Lock state 0=unlocked 1=locked   */
	unsigned char	prevent_jumper	: 1;	/* State of prev/allow jumper 0=pres */
	unsigned char	eject		: 1;	/* Ejects disc/cartr with STOP LoEj  */
	unsigned char	res_6_4		: 1;	/* Reserved			    */
	unsigned char	loading_type	: 3;	/* Loading mechanism type	    */
	unsigned char	sep_chan_vol	: 1;	/* Vol controls each channel separat */
	unsigned char	sep_chan_mute	: 1;	/* Mute controls each channel separat*/
	unsigned char	disk_present_rep: 1;	/* Changer supports disk present rep */
	unsigned char	sw_slot_sel	: 1;	/* Load empty slot in changer	    */
	unsigned char	side_change	: 1;	/* Side change capable		    */
	unsigned char	pw_in_lead_in	: 1;	/* Reads raw P-W sucode from lead in */
	unsigned char	res_7		: 2;	/* Reserved			    */
	unsigned char	max_read_speed[2];	/* Max. read speed in KB/s	    */
	unsigned char	num_vol_levels[2];	/* # of supported volume levels	    */
	unsigned char	buffer_size[2];		/* Buffer size for the data in KB   */
	unsigned char	cur_read_speed[2];	/* Current read speed in KB/s	    */
	unsigned char	res_16;			/* Reserved			    */
	unsigned char	res_17_0	: 1;	/* Reserved			    */
	unsigned char	BCK		: 1;	/* Data valid on falling edge of BCK */
	unsigned char	RCK		: 1;	/* Set: HIGH high LRCK=left channel  */
	unsigned char	LSBF		: 1;	/* Set: LSB first Clear: MSB first  */
	unsigned char	length		: 2;	/* 0=32BCKs 1=16BCKs 2=24BCKs 3=24I2c*/
	unsigned char	res_17		: 2;	/* Reserved			    */
	unsigned char	max_write_speed[2];	/* Max. write speed supported in KB/s*/
	unsigned char	cur_write_speed[2];	/* Current write speed in KB/s	    */

					/* Byte 22 ... Only in MMC-2	    */
	unsigned char	copy_man_rev[2];	/* Copy management revision supported*/
	unsigned char	res_24;			/* Reserved			    */
	unsigned char	res_25;			/* Reserved			    */

					/* Byte 26 ... Only in MMC-3	    */
	unsigned char	res_26;			/* Reserved			    */
	unsigned char	res_27_27	: 6;	/* Reserved			    */
	unsigned char	rot_ctl_sel	: 2;	/* Rotational control selected	    */
	unsigned char	v3_cur_write_speed[2];	/* Current write speed in KB/s	    */
	unsigned char	num_wr_speed_des[2];	/* # of wr speed perf descr. tables */
	struct cd_wr_speed_performance
		wr_speed_des[1];	/* wr speed performance descriptor  */
					/* Actually more (num_wr_speed_des) */
};

#else				/* Motorola byteorder */

struct cd_wr_speed_performance {
	unsigned char	res0;			/* Reserved			    */
	unsigned char	res_1_27	: 6;	/* Reserved			    */
	unsigned char	rot_ctl_sel	: 2;	/* Rotational control selected	    */
	unsigned char	wr_speed_supp[2];	/* Supported write speed	    */
};

struct cd_mode_page_2A {		/* CD Cap / mech status */
		MP_P_CODE;		/* parsave & pagecode */
	unsigned char	p_len;			/* 0x14 = 20 Bytes (MMC) */
					/* 0x18 = 24 Bytes (MMC-2) */
					/* 0x1C >= 28 Bytes (MMC-3) */
	unsigned char	res_2_67	: 2;	/* Reserved			    */
	unsigned char	dvd_ram_read	: 1;	/* Reads DVD-RAM media		    */
	unsigned char	dvd_r_read	: 1;	/* Reads DVD-R media		    */
	unsigned char	dvd_rom_read	: 1;	/* Reads DVD ROM media		    */
	unsigned char	method2		: 1;	/* Reads fixed packet method2 media */
	unsigned char	cd_rw_read	: 1;	/* Reads CD-RW media		    */
	unsigned char	cd_r_read	: 1;	/* Reads CD-R  media		    */
	unsigned char	res_3_67	: 2;	/* Reserved			    */
	unsigned char	dvd_ram_write	: 1;	/* Supports writing DVD-RAM media   */
	unsigned char	dvd_r_write	: 1;	/* Supports writing DVD-R media	    */
	unsigned char	res_3_3		: 1;	/* Reserved			    */
	unsigned char	test_write	: 1;	/* Supports emulation write	    */
	unsigned char	cd_rw_write	: 1;	/* Supports writing CD-RW media	    */
	unsigned char	cd_r_write	: 1;	/* Supports writing CD-R  media	    */
	unsigned char	BUF		: 1;	/* Supports Buffer under. free rec. */
	unsigned char	multi_session	: 1;	/* Reads multi-session media	    */
	unsigned char	mode_2_form_2	: 1;	/* Reads Mode-2 form 2 media	    */
	unsigned char	mode_2_form_1	: 1;	/* Reads Mode-2 form 1 media (XA)   */
	unsigned char	digital_port_1	: 1;	/* Supports digital output on port 1 */
	unsigned char	digital_port_2	: 1;	/* Supports digital output on port 2 */
	unsigned char	composite	: 1;	/* Deliveres composite A/V stream   */
	unsigned char	audio_play	: 1;	/* Supports Audio play operation    */
	unsigned char	read_bar_code	: 1;	/* Supports reading bar codes	    */
	unsigned char	UPC		: 1;	/* Reads media catalog number (UPC) */
	unsigned char	ISRC		: 1;	/* Reads ISRC information	    */
	unsigned char	c2_pointers	: 1;	/* Supports C2 error pointers	    */
	unsigned char	rw_deint_corr	: 1;	/* Reads de-interleved R-W sub chan */
	unsigned char	rw_supported	: 1;	/* Reads R-W sub channel information */
	unsigned char	cd_da_accurate	: 1;	/* READ CD data stream is accurate   */
	unsigned char	cd_da_supported	: 1;	/* Reads audio data with READ CD cmd */
	unsigned char	loading_type	: 3;	/* Loading mechanism type	    */
	unsigned char	res_6_4		: 1;	/* Reserved			    */
	unsigned char	eject		: 1;	/* Ejects disc/cartr with STOP LoEj */
	unsigned char	prevent_jumper	: 1;	/* State of prev/allow jumper 0=pres */
	unsigned char	lock_state	: 1;	/* Lock state 0=unlocked 1=locked   */
	unsigned char	lock		: 1;	/* PREVENT/ALLOW may lock media	    */
	unsigned char	res_7		: 2;	/* Reserved			    */
	unsigned char	pw_in_lead_in	: 1;	/* Reads raw P-W sucode from lead in */
	unsigned char	side_change	: 1;	/* Side change capable		    */
	unsigned char	sw_slot_sel	: 1;	/* Load empty slot in changer	    */
	unsigned char	disk_present_rep: 1;	/* Changer supports disk present rep */
	unsigned char	sep_chan_mute	: 1;	/* Mute controls each channel separat*/
	unsigned char	sep_chan_vol	: 1;	/* Vol controls each channel separat */
	unsigned char	max_read_speed[2];	/* Max. read speed in KB/s	    */
	unsigned char	num_vol_levels[2];	/* # of supported volume levels	    */
	unsigned char	buffer_size[2];		/* Buffer size for the data in KB   */
	unsigned char	cur_read_speed[2];	/* Current read speed in KB/s	    */
	unsigned char	res_16;			/* Reserved			    */
	unsigned char	res_17		: 2;	/* Reserved			    */
	unsigned char	length		: 2;	/* 0=32BCKs 1=16BCKs 2=24BCKs 3=24I2c*/
	unsigned char	LSBF		: 1;	/* Set: LSB first Clear: MSB first  */
	unsigned char	RCK		: 1;	/* Set: HIGH high LRCK=left channel */
	unsigned char	BCK		: 1;	/* Data valid on falling edge of BCK */
	unsigned char	res_17_0	: 1;	/* Reserved			    */
	unsigned char	max_write_speed[2];	/* Max. write speed supported in KB/s*/
	unsigned char	cur_write_speed[2];	/* Current write speed in KB/s	    */

					/* Byte 22 ... Only in MMC-2	    */
	unsigned char	copy_man_rev[2];	/* Copy management revision supported*/
	unsigned char	res_24;			/* Reserved			    */
	unsigned char	res_25;			/* Reserved			    */

					/* Byte 26 ... Only in MMC-3	    */
	unsigned char	res_26;			/* Reserved			    */
	unsigned char	res_27_27	: 6;	/* Reserved			    */
	unsigned char	rot_ctl_sel	: 2;	/* Rotational control selected	    */
	unsigned char	v3_cur_write_speed[2];	/* Current write speed in KB/s	    */
	unsigned char	num_wr_speed_des[2];	/* # of wr speed perf descr. tables */
	struct cd_wr_speed_performance
		wr_speed_des[1];	/* wr speed performance descriptor  */
					/* Actually more (num_wr_speed_des) */
};

#endif

#define	LT_CADDY	0
#define	LT_TRAY		1
#define	LT_POP_UP	2
#define	LT_RES3		3
#define	LT_CHANGER_IND	4
#define	LT_CHANGER_CART	5
#define	LT_RES6		6
#define	LT_RES7		7


struct scsi_mode_data {
	struct scsi_mode_header		header;
	struct scsi_mode_blockdesc	blockdesc;
	union	pagex	{
		struct acb_mode_data		acb;
		struct scsi_mode_page_01	page1;
		struct scsi_mode_page_02	page2;
		struct scsi_mode_page_03	page3;
		struct scsi_mode_page_04	page4;
		struct scsi_mode_page_05	page5;
		struct scsi_mode_page_07	page7;
		struct scsi_mode_page_08	page8;
		struct scsi_mode_page_09	page9;
		struct scsi_mode_page_0A	pageA;
		struct scsi_mode_page_0B	pageB;
		struct scsi_mode_page_0C	pageC;
		struct scsi_mode_page_0D	pageD;
		struct sony_mode_page_20	sony20;
		struct toshiba_mode_page_20	toshiba20;
		struct ccs_mode_page_38		ccs38;
	} pagex;
};

struct scsi_capacity {
	int32_t	c_baddr;		/* must convert byteorder!! */
	int32_t	c_bsize;		/* must convert byteorder!! */
};

#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct scsi_def_header {
	unsigned char		: 8;
	unsigned char	format	: 3;
	unsigned char	gdl	: 1;
	unsigned char	mdl	: 1;
	unsigned char		: 3;
	unsigned char	length[2];
};

#else					/* Motorola byteorder */

struct scsi_def_header {
	unsigned char		: 8;
	unsigned char		: 3;
	unsigned char	mdl	: 1;
	unsigned char	gdl	: 1;
	unsigned char	format	: 3;
	unsigned char	length[2];
};
#endif


#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct scsi_format_header {
	unsigned char	res		: 8;	/* Adaptec 5500: 1 --> format track */
	unsigned char	vu		: 1;	/* Vendor Unique		    */
	unsigned char	immed		: 1;	/* Return Immediately from Format   */
	unsigned char	tryout		: 1;	/* Check if format parameters OK    */
	unsigned char	ipattern	: 1;	/* Init patter descriptor present   */
	unsigned char	serr		: 1;	/* Stop on error		    */
	unsigned char	dcert		: 1;	/* Disable certification	    */
	unsigned char	dmdl		: 1;	/* Disable manufacturer defect list */
	unsigned char	enable		: 1;	/* Enable to use the next 3 bits    */
	unsigned char	length[2];		/* Length of following list in bytes*/
};

#else					/* Motorola byteorder */

struct scsi_format_header {
	unsigned char	res		: 8;	/* Adaptec 5500: 1 --> format track */
	unsigned char	enable		: 1;	/* Enable to use the next 3 bits    */
	unsigned char	dmdl		: 1;	/* Disable manufacturer defect list */
	unsigned char	dcert		: 1;	/* Disable certification	    */
	unsigned char	serr		: 1;	/* Stop on error		    */
	unsigned char	ipattern	: 1;	/* Init patter descriptor present   */
	unsigned char	tryout		: 1;	/* Check if format parameters OK    */
	unsigned char	immed		: 1;	/* Return Immediately from Format   */
	unsigned char	vu		: 1;	/* Vendor Unique		    */
	unsigned char	length[2];		/* Length of following list in bytes*/
};
#endif

struct	scsi_def_bfi {
	unsigned char	cyl[3];
	unsigned char	head;
	unsigned char	bfi[4];
};

struct	scsi_def_phys {
	unsigned char	cyl[3];
	unsigned char	head;
	unsigned char	sec[4];
};

struct	scsi_def_list {
	struct	scsi_def_header	hd;
	union {
			unsigned char		list_block[1][4];
		struct	scsi_def_bfi	list_bfi[1];
		struct	scsi_def_phys	list_phys[1];
	} def_list;
};

struct	scsi_format_data {
	struct scsi_format_header hd;
	union {
			unsigned char		list_block[1][4];
		struct	scsi_def_bfi	list_bfi[1];
		struct	scsi_def_phys	list_phys[1];
	} def_list;
};

#define	def_block	def_list.list_block
#define	def_bfi		def_list.list_bfi
#define	def_phys	def_list.list_phys

#define	SC_DEF_BLOCK	0
#define	SC_DEF_BFI	4
#define	SC_DEF_PHYS	5
#define	SC_DEF_VU	6
#define	SC_DEF_RES	7

struct scsi_format_cap_header {
	unsigned char	res[3];			/* Reserved			*/
	unsigned char	len;			/* Len (a multiple of 8)	*/
};

#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct scsi_format_cap_desc {
	unsigned char	nblock[4];		/* Number of blocks		*/
	unsigned char	desc_type	: 2;	/* Descriptor type		*/
	unsigned char	fmt_type	: 6;	/* Format Type			*/
	unsigned char	blen[3];		/* Logical block length		*/
};

#else					/* Motorola byteorder */

struct scsi_format_cap_desc {
	unsigned char	nblock[4];		/* Number of blocks		*/
	unsigned char	fmt_type	: 6;	/* Format Type			*/
	unsigned char	desc_type	: 2;	/* Descriptor type		*/
	unsigned char	blen[3];		/* Logical block length		*/
};
#endif

/*
 * Defines for 'fmt_type'.
 */
#define	FCAP_TYPE_FULL		0x00	/* Full Format			*/
#define	FCAP_TYPE_EXPAND_SPARE	0x01	/* Spare area expansion		*/
#define	FCAP_TYPE_ZONE_REFORMAT	0x04	/* DVD-RAM Zone Reformat	*/
#define	FCAP_TYPE_ZONE_FORMAT	0x05	/* DVD-RAM Zone Format		*/
#define	FCAP_TYPE_CDRW_FULL	0x10	/* CD-RW/DVD-RW Full Format	*/
#define	FACP_TYPE_CDRW_GROW_SES	0x11	/* CD-RW/DVD-RW grow session	*/
#define	FACP_TYPE_CDRW_ADD_SES	0x12	/* CD-RW/DVD-RW add session	*/
#define	FACP_TYPE_DVDRW_QGROW	0x13	/* DVD-RW quick grow last border*/
#define	FACP_TYPE_DVDRW_QADD_SES 0x14	/* DVD-RW quick add session	*/
#define	FACP_TYPE_DVDRW_QUICK	0x15	/* DVD-RW quick interm. session	*/
#define	FCAP_TYPE_FULL_SPARE	0x20	/* Full Format with sparing	*/
#define	FCAP_TYPE_MRW_FULL	0x24	/* CD-RW/DVD+RW Full Format	*/
#define	FCAP_TYPE_DVDPLUS_BASIC	0x26	/* DVD+RW Basic Format		*/
#define	FCAP_TYPE_DVDPLUS_FULL	0x26	/* DVD+RW Full Format		*/
#define	FCAP_TYPE_BDRE_SPARE	0x30	/* BD-RE Full Format with spare	*/
#define	FCAP_TYPE_BDRE		0x31	/* BD-RE Full Format without spare*/
#define	FCAP_TYPE_BDR_SPARE	0x32	/* BD-R Full Format with spare	*/

/*
 * Defines for 'desc_type'.
 * In case of FCAP_DESC_RES, the descriptor is a formatted capacity descriptor
 * and the 'blen' field is type dependent.
 * For all other cases, this is the Current/Maximum Capacity descriptor and
 * the value of 'fmt_type' is reserved and must be zero.
 */
#define	FCAP_DESC_RES		0	/* Reserved			*/
#define	FCAP_DESC_UNFORM	1	/* Unformatted Media		*/
#define	FCAP_DESC_FORM		2	/* Formatted Media		*/
#define	FCAP_DESC_NOMEDIA	3	/* No Media			*/

struct	scsi_cap_data {
	struct scsi_format_cap_header	hd;
	struct scsi_format_cap_desc	list[1];
};


struct	scsi_send_diag_cmd {
	unsigned char	cmd;
	unsigned char	addr[4];
	unsigned char		: 8;
};

#if	defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct	scsi_sector_header {
	unsigned char	cyl[2];
	unsigned char	head;
	unsigned char	sec;
	unsigned char		: 5;
	unsigned char	rp	: 1;
	unsigned char	sp	: 1;
	unsigned char	dt	: 1;
};

#else					/* Motorola byteorder */

struct	scsi_sector_header {
	unsigned char	cyl[2];
	unsigned char	head;
	unsigned char	sec;
	unsigned char	dt	: 1;
	unsigned char	sp	: 1;
	unsigned char	rp	: 1;
	unsigned char		: 5;
};
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SCG_SCSIREG_H */
