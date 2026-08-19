/* @(#)adip.h	1.2 06/09/13 Copyright 2004 J. Schilling */

#ifndef	ADIP_H
#define	ADIP_H

#include <schily/utypes.h>

typedef struct adip {
	unsigned char	cat_vers;		/*  0	*/
	unsigned char	disk_size;		/*  1	*/
	unsigned char	disk_struct;		/*  2	*/
	unsigned char	density;		/*  3	*/
	unsigned char	data_zone_alloc[12];	/*  4	*/
	unsigned char	mbz_16;			/* 16	*/
	unsigned char	res_17[2];		/* 17	*/
	unsigned char	man_id[8];		/* 19	*/
	unsigned char	media_id[3];		/* 27	*/
	unsigned char	prod_revision;		/* 30	*/
	unsigned char	adip_numbytes;		/* 31	*/
	unsigned char	ref_speed;		/* 32	*/
	unsigned char	max_speed;		/* 33	*/
	unsigned char	wavelength;		/* 34	*/
	unsigned char	norm_write_power;	/* 35	*/
	unsigned char	max_read_power_ref;	/* 36	*/
	unsigned char	pind_ref;		/* 37	*/
	unsigned char	beta_ref;		/* 38	*/
	unsigned char	max_read_power_max;	/* 39	*/
	unsigned char	pind_max;		/* 40	*/
	unsigned char	beta_max;		/* 41	*/
	unsigned char	pulse[14];		/* 42	*/
	unsigned char	res_56[192];		/* 56	*/
	unsigned char	res_controldat[8];	/* 248	*/
} adip_t;



#endif	/* ADIP_H */
