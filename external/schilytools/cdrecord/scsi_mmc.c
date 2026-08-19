/* @(#)scsi_mmc.c	1.51 10/12/19 Copyright 2002-2010 J. Schilling */
#include <stdint.h>
#include <schily/mconfig.h>
#ifndef lint
static	UConst char sccsid[] =
	"@(#)scsi_mmc.c	1.51 10/12/19 Copyright 2002-2010 J. Schilling";
#endif
/*
 *	SCSI command functions for cdrecord
 *	covering MMC-3 level and above
 *
 *	Copyright (c) 2002-2010 J. Schilling
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

/*#define	DEBUG*/

#include <schily/mconfig.h>

#include <schily/stdio.h>
#include <schily/standard.h>
#include <schily/stdlib.h>
#include <schily/unistd.h>
#include <schily/fcntl.h>
#include <schily/errno.h>
#include <schily/string.h>
#include <schily/time.h>

#include <schily/utypes.h>
#include <schily/btorder.h>
#include <schily/intcvt.h>
#include <schily/schily.h>
#include <schily/nlsdefs.h>

#include <scg/scgcmd.h>
#include <scg/scsidefs.h>
#include <scg/scsireg.h>
#include <scg/scsitransp.h>

#include "scsimmc.h"
#include "cdrecord.h"

extern	int	xdebug;
extern	int	lverbose;
extern	char	*driveropts;

LOCAL struct features {
	uint16_t	code;
	char		*name;
} fl[] = {
	{ 0x0000,	"Profile List", },
	{ 0x0001,	"Core", },
	{ 0x0002,	"Morphing", },
	{ 0x0003,	"Removable Medium", },
	{ 0x0004,	"Write Protect", },

	{ 0x0010,	"Random Readable", },

	{ 0x001D,	"Multi Read", },
	{ 0x001E,	"CD Read", },
	{ 0x001F,	"DVD Read", },

	{ 0x0020,	"Random Writable", },
	{ 0x0021,	"Incremental Streaming Writable", },
	{ 0x0022,	"Sector Erasable", },
	{ 0x0023,	"Formattable", },
	{ 0x0024,	"Defect Management", },
	{ 0x0025,	"Write Once", },
	{ 0x0026,	"Restricted Overwrite", },
	{ 0x0027,	"CD-RW CAV Write", },
	{ 0x0028,	"MRW", },
	{ 0x0029,	"Ehanced Defect Reporting", },
	{ 0x002A,	"DVD+RW", },
	{ 0x002B,	"DVD+R", },
	{ 0x002C,	"Rigid Restricted Overwrite", },
	{ 0x002D,	"CD Track at Once", },
	{ 0x002E,	"CD Mastering", },
	{ 0x002F,	"DVD-R/-RW Write", },

	{ 0x0030,	"DDCD Read", },
	{ 0x0031,	"DDCD-R Write", },
	{ 0x0032,	"DDCD-RW Write", },

	{ 0x0033,	"Layer Jump Recording", },

	{ 0x0037,	"CD-RW Write", },
	{ 0x0038,	"BD-R Pseudo-Overwrite (POW)", },

	{ 0x003A,	"DVD+RW/DL Read", },
	{ 0x003B,	"DVD+R/DL Read", },

	{ 0x0040,	"BD Read", },
	{ 0x0041,	"BD Write", },
	{ 0x0042,	"Time Safe Recording (TSR)", },

	{ 0x0050,	"HD-DVD Read", },
	{ 0x0051,	"HD-DVD Write", },

	{ 0x0080,	"Hybrid Disk Read", },

	{ 0x0100,	"Power Management", },
	{ 0x0101,	"S.M.A.R.T.", },
	{ 0x0102,	"Embedded Changer", },
	{ 0x0103,	"CD Audio analog play", },
	{ 0x0104,	"Microcode Upgrade", },
	{ 0x0105,	"Time-out", },
	{ 0x0106,	"DVD-CSS", },
	{ 0x0107,	"Real Time Streaming", },
	{ 0x0108,	"Logical Unit Serial Number", },
	{ 0x0109,	"Media Serial Number", },
	{ 0x010A,	"Disk Control Blocks", },
	{ 0x010B,	"DVD CPRM", },
	{ 0x010C,	"Microcode Information", },
	{ 0x010D,	"AACS", },

	{ 0x0110,	"VCPS", },
};

LOCAL struct profiles {
	uint16_t	code;
	char		*name;
} pl[] = {
	{ 0x0000,	"Reserved", },
	{ 0x0001,	"Non -removable Disk", },
	{ 0x0002,	"Removable Disk", },
	{ 0x0003,	"MO Erasable", },
	{ 0x0004,	"MO Write Once", },
	{ 0x0005,	"AS-MO", },

	/* 0x06..0x07 is reserved */

	{ 0x0008,	"CD-ROM", },
	{ 0x0009,	"CD-R", },
	{ 0x000A,	"CD-RW", },

	/* 0x0B..0x0F is reserved */

	{ 0x0010,	"DVD-ROM", },
	{ 0x0011,	"DVD-R sequential recording", },
	{ 0x0012,	"DVD-RAM", },
	{ 0x0013,	"DVD-RW restricted overwrite", },
	{ 0x0014,	"DVD-RW sequential recording", },
	{ 0x0015,	"DVD-R/DL sequential recording", },
	{ 0x0016,	"DVD-R/DL layer jump recording", },
	{ 0x0017,	"DVD-RW/DL", },

	/* 0x18..0x19 is reserved */

	{ 0x001A,	"DVD+RW", },
	{ 0x001B,	"DVD+R", },

	{ 0x0020,	"DDCD-ROM", },
	{ 0x0021,	"DDCD-R", },
	{ 0x0022,	"DDCD-RW", },

	{ 0x002A,	"DVD+RW/DL", },
	{ 0x002B,	"DVD+R/DL", },

	{ 0x0040,	"BD-ROM", },
	{ 0x0041,	"BD-R sequential recording", },
	{ 0x0042,	"BD-R random recording", },
	{ 0x0043,	"BD-RE", },

	/* 0x44..0x4F is reserved */

	{ 0x0050,	"HD DVD-ROM", },
	{ 0x0051,	"HD DVD-R", },
	{ 0x0052,	"HD DVD-RAM", },
	{ 0x0053,	"HD DVD-RW", },

	/* 0x54..0x57 is reserved */

	{ 0x0058,	"HD DVD-R/DL", },

	{ 0x005A,	"HD DVD-RW/DL", },

	{ 0xFFFF,	"No standard Profile", },
};


EXPORT	int	get_configuration	__PR((SCSI *scgp, char * bp, int cnt, int st_feature, int rt));
LOCAL	int	get_conflen		__PR((SCSI *scgp, int st_feature, int rt));
EXPORT	int	get_curprofile		__PR((SCSI *scgp));
LOCAL	int	get_profiles		__PR((SCSI *scgp, char * bp, int cnt));
EXPORT	int	has_profile		__PR((SCSI *scgp, int profile));
EXPORT	int	print_profiles		__PR((SCSI *scgp));
EXPORT	int	get_proflist		__PR((SCSI *scgp, BOOL *wp, BOOL *cdp, BOOL *dvdp, BOOL *dvdplusp, BOOL *ddcdp));
EXPORT	int	get_wproflist		__PR((SCSI *scgp, BOOL *cdp, BOOL *dvdp,
							BOOL *dvdplusp, BOOL *ddcdp));
EXPORT	int	get_mediatype		__PR((SCSI *scgp));
EXPORT	int	get_singlespeed		__PR((int mt));
EXPORT	float	get_secsps		__PR((int mt));
EXPORT	char	*get_mclassname		__PR((int mt));
EXPORT	int	get_blf			__PR((int mt));

LOCAL	int	scsi_get_performance	__PR((SCSI *scgp, char * bp, int cnd, int ndesc, int type, int datatype));
EXPORT	int	scsi_get_perf_maxspeed	__PR((SCSI *scgp, unsigned long *readp, unsigned long *writep, unsigned long *endp));
EXPORT	int	scsi_get_perf_curspeed	__PR((SCSI *scgp, unsigned long *readp, unsigned long *writep, unsigned long *endp));
LOCAL	int	scsi_set_streaming	__PR((SCSI *scgp, unsigned long *readp, unsigned long *writep, unsigned long *endp, int wrc, BOOL exact, BOOL restore));
EXPORT	int	speed_select_mdvd	__PR((SCSI *scgp, int readspeed, int writespeed));
LOCAL	char	*fname			__PR((unsigned int code));
LOCAL	char	*pname			__PR((unsigned int code));
LOCAL	BOOL	fname_known		__PR((unsigned int code));
LOCAL	BOOL	pname_known		__PR((unsigned int code));
EXPORT	int	print_features		__PR((SCSI *scgp));
EXPORT	void	print_format_capacities	__PR((SCSI *scgp));
EXPORT	int	get_format_capacities	__PR((SCSI *scgp, char * bp, int cnt));
EXPORT	int	read_format_capacities	__PR((SCSI *scgp, char * bp, int cnt));

EXPORT	void	przone			__PR((struct rzone_info *rp));
EXPORT	int	get_diskinfo		__PR((SCSI *scgp, struct disk_info *dip, int cnt));
EXPORT	char	*get_ses_type		__PR((int ses_type));
EXPORT	void	print_diskinfo		__PR((struct disk_info *dip, BOOL is_cd));
EXPORT	int	prdiskstatus		__PR((SCSI *scgp, cdr_t *dp, BOOL is_cd));
EXPORT	int	sessstatus		__PR((SCSI *scgp, BOOL is_cd, long *offp, long *nwap));
EXPORT	void	print_performance_mmc	__PR((SCSI *scgp));


/*
 * Get feature codes
 */
EXPORT int
get_configuration(scgp, bp, cnt, st_feature, rt)
	SCSI	*scgp;
	char *	bp;
	int	cnt;
	int	st_feature;
	int	rt;
{
	register struct	scg_cmd	*scmd = scgp->scmd;

	fillbytes((char *)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0x46;
	scmd->cdb.g1_cdb.lun = scg_lun(scgp);
	if (rt & 1)
		scmd->cdb.g1_cdb.reladr  = 1;
	if (rt & 2)
		scmd->cdb.g1_cdb.res  = 1;

	i_to_2_byte(scmd->cdb.g1_cdb.addr, st_feature);
	g1_cdblen(&scmd->cdb.g1_cdb, cnt);

	scgp->cmdname = "get_configuration";

	return (scg_cmd(scgp));
}

/*
 * Retrieve feature code list length
 */
LOCAL int
get_conflen(scgp, st_feature, rt)
	SCSI	*scgp;
	int	st_feature;
	int	rt;
{
	unsigned char	cbuf[8];
	int	flen;
	int	i;

	fillbytes(cbuf, sizeof (cbuf), '\0');
	scgp->silent++;
	i = get_configuration(scgp, (char *)cbuf, sizeof (cbuf), st_feature, rt);
	scgp->silent--;
	if (i < 0)
		return (-1);
	i = sizeof (cbuf) - scg_getresid(scgp);
	if (i < 4)
		return (-1);

	flen = a_to_u_4_byte(cbuf);
	if (flen < 4)
		return (-1);
	return (flen);
}

EXPORT int
get_curprofile(scgp)
	SCSI	*scgp;
{
	unsigned char	cbuf[8];
	int	amt;
	int	flen;
	int	profile;
	int	i;

	fillbytes(cbuf, sizeof (cbuf), '\0');
	scgp->silent++;
	i = get_configuration(scgp, (char *)cbuf, sizeof (cbuf), 0, 0);
	scgp->silent--;
	if (i < 0)
		return (-1);

	amt = sizeof (cbuf) - scg_getresid(scgp);
	if (amt < 8)
		return (-1);
	flen = a_to_u_4_byte(cbuf);
	if (flen < 4)
		return (-1);

	profile = a_to_u_2_byte(&cbuf[6]);

	if (xdebug > 1)
		scg_prbytes(_("Features: "), cbuf, amt);

	if (xdebug > 0)
		printf(_("feature len: %d current profile 0x%04X len %d\n"),
				flen, profile, amt);

	return (profile);
}

LOCAL int
get_profiles(scgp, bp, cnt)
	SCSI	*scgp;
	char *	bp;
	int	cnt;
{
	int	amt;
	int	flen;
	int	i;

	flen = get_conflen(scgp, 0, 0);
	if (flen < 0)
		return (-1);
	if (cnt < flen)
		flen = cnt;

	fillbytes(bp, cnt, '\0');
	scgp->silent++;
	i = get_configuration(scgp, (char *)bp, flen, 0, 0);
	scgp->silent--;
	if (i < 0)
		return (-1);
	amt = flen - scg_getresid(scgp);

	flen = a_to_u_4_byte(bp);
	if ((flen+4) < amt)
		amt = flen+4;

	return (amt);
}

EXPORT int
has_profile(scgp, profile)
	SCSI	*scgp;
	int	profile;
{
	unsigned char	cbuf[1024];
	unsigned char	*p;
	int	flen;
	int	prf;
	int	i;
	int	n;

	flen = get_profiles(scgp, (char *)cbuf, sizeof (cbuf));
	if (flen < 0)
		return (-1);

	p = cbuf;
	p += 8;		/* Skip feature header	*/
	n = p[3];	/* Additional length	*/
	n /= 4;
	p += 4;

	for (i = 0; i < n; i++) {
		prf = a_to_u_2_byte(p);
		if (xdebug > 0)
			printf(_("Profile: 0x%04X "), prf);
		if (profile == prf)
			return (1);
		p += 4;
	}
	return (0);
}

EXPORT int
print_profiles(scgp)
	SCSI	*scgp;
{
	unsigned char	cbuf[1024];
	unsigned char	*p;
	int	flen;
	int	curprofile;
	int	profile;
	int	i;
	int	n;

	flen = get_profiles(scgp, (char *)cbuf, sizeof (cbuf));
	if (flen < 0)
		return (-1);

	p = cbuf;
	if (xdebug > 1)
		scg_prbytes(_("Features: "), cbuf, flen);

	curprofile = a_to_u_2_byte(&p[6]);
	if (xdebug > 0)
		printf(_("feature len: %d current profile 0x%04X\n"),
				flen, curprofile);

	if (pname_known(curprofile))
		printf(_("Current: %s\n"), curprofile == 0 ? _("none") : pname(curprofile));
	else
		printf(_("Current: 0x%04X unknown\n"), curprofile);

	p += 8;		/* Skip feature header	*/
	n = p[3];	/* Additional length	*/
	n /= 4;
	p += 4;

	for (i = 0; i < n; i++) {
		profile = a_to_u_2_byte(p);
		if (xdebug > 0)
			printf(_("Profile: 0x%04X "), profile);
		else
			printf(_("Profile: "));
		if (pname_known(profile))
			printf("%s %s\n", pname(profile), p[2] & 1 ? _("(current)"):"");
		else
			printf("0x%04X %s\n", profile, p[2] & 1 ? _("(current)"):"");
		p += 4;
	}
	return (curprofile);
}

EXPORT int
get_proflist(scgp, wp, cdp, dvdp, dvdplusp, ddcdp)
	SCSI	*scgp;
	BOOL	*wp;
	BOOL	*cdp;
	BOOL	*dvdp;
	BOOL	*dvdplusp;
	BOOL	*ddcdp;
{
	unsigned char	cbuf[1024];
	unsigned char	*p;
	int	flen;
	int	curprofile;
	int	profile;
	int	i;
	int	n;
	BOOL	wr	= FALSE;
	BOOL	cd	= FALSE;
	BOOL	dvd	= FALSE;
	BOOL	dvdplus	= FALSE;
	BOOL	ddcd	= FALSE;

	flen = get_profiles(scgp, (char *)cbuf, sizeof (cbuf));
	if (flen < 0)
		return (-1);

	p = cbuf;
	if (xdebug > 1)
		scg_prbytes(_("Features: "), cbuf, flen);

	curprofile = a_to_u_2_byte(&p[6]);
	if (xdebug > 0)
		printf(_("feature len: %d current profile 0x%04X\n"),
				flen, curprofile);

	p += 8;		/* Skip feature header	*/
	n = p[3];	/* Additional length	*/
	n /= 4;
	p += 4;

	for (i = 0; i < n; i++) {
		profile = a_to_u_2_byte(p);
		p += 4;
		if (profile >= 0x0008 && profile < 0x0010)
			cd = TRUE;
		if (profile > 0x0008 && profile < 0x0010)
			wr = TRUE;

		if (profile >= 0x0010 && profile < 0x0018)
			dvd = TRUE;
		if (profile > 0x0010 && profile < 0x0018)
			wr = TRUE;

		if (profile >= 0x0018 && profile < 0x0020)
			dvdplus = TRUE;
		if (profile > 0x0018 && profile < 0x0020)
			wr = TRUE;

		if (profile >= 0x0020 && profile < 0x0028)
			ddcd = TRUE;
		if (profile > 0x0020 && profile < 0x0028)
			wr = TRUE;
	}
	if (wp)
		*wp	= wr;
	if (cdp)
		*cdp	= cd;
	if (dvdp)
		*dvdp	= dvd;
	if (dvdplusp)
		*dvdplusp = dvdplus;
	if (ddcdp)
		*ddcdp	= ddcd;

	return (curprofile);
}

EXPORT int
get_wproflist(scgp, cdp, dvdp, dvdplusp, ddcdp)
	SCSI	*scgp;
	BOOL	*cdp;
	BOOL	*dvdp;
	BOOL	*dvdplusp;
	BOOL	*ddcdp;
{
	unsigned char	cbuf[1024];
	unsigned char	*p;
	int	flen;
	int	curprofile;
	int	profile;
	int	i;
	int	n;
	BOOL	cd	= FALSE;
	BOOL	dvd	= FALSE;
	BOOL	dvdplus	= FALSE;
	BOOL	ddcd	= FALSE;

	flen = get_profiles(scgp, (char *)cbuf, sizeof (cbuf));
	if (flen < 0)
		return (-1);
	p = cbuf;
	curprofile = a_to_u_2_byte(&p[6]);

	p += 8;		/* Skip feature header	*/
	n = p[3];	/* Additional length	*/
	n /= 4;
	p += 4;

	for (i = 0; i < n; i++) {
		profile = a_to_u_2_byte(p);
		p += 4;
		if (profile > 0x0008 && profile < 0x0010)
			cd = TRUE;
		if (profile > 0x0010 && profile < 0x0018)
			dvd = TRUE;
		if (profile > 0x0018 && profile < 0x0020)
			dvdplus = TRUE;
		if (profile > 0x0020 && profile < 0x0028)
			ddcd = TRUE;
	}
	if (cdp)
		*cdp	= cd;
	if (dvdp)
		*dvdp	= dvd;
	if (dvdplusp)
		*dvdplusp = dvdplus;
	if (ddcdp)
		*ddcdp	= ddcd;

	return (curprofile);
}

EXPORT int
get_mediatype(scgp)
	SCSI	*scgp;
{
	int	profile = get_curprofile(scgp);

	if (profile < 0x08)
		return (MT_NONE);
	if (profile >= 0x08 && profile < 0x10)
		return (MT_CD);
	if (profile >= 0x10 && profile < 0x40)
		return (MT_DVD);
	if (profile >= 0x40 && profile < 0x50)
		return (MT_BD);
	if (profile >= 0x50 && profile < 0x60)
		return (MT_HDDVD);

	return (MT_NONE);
}

EXPORT int
get_singlespeed(mt)
	int	mt;
{
	switch (mt) {

	case MT_CD:
		return (176);

	case MT_DVD:
		return (1385);

	case MT_BD:
		return (4495);

	case MT_HDDVD:
		return (4495);	/* XXX ??? */

	case MT_NONE:
	default:
		return (1);
	}
}

EXPORT float
get_secsps(mt)
	int	mt;
{
	switch (mt) {

	case MT_CD:
		return ((float)75.0);

	case MT_DVD:
		return ((float)676.27);

	case MT_BD:
		return ((float)2195.07);

	case MT_HDDVD:
		return ((float)2195.07);	/* XXX ??? */

	case MT_NONE:
	default:
		return ((float)75.0);
	}
}

EXPORT char *
get_mclassname(mt)
	int	mt;
{
	switch (mt) {

	case MT_CD:
		return ("CD");

	case MT_DVD:
		return ("DVD");

	case MT_BD:
		return ("BD");

	case MT_HDDVD:
		return ("HD-DVD");

	case MT_NONE:
	default:
		return ("NONE");
	}
}

/*
 * Guessed blocking factor based on media type
 */
EXPORT int
get_blf(mt)
	int	mt;
{
	switch (mt) {

	case MT_DVD:
		return (16);

	case MT_BD:
		return (32);

	case MT_HDDVD:
		return (32);	/* XXX ??? */

	default:
		return (1);
	}
}

LOCAL int
scsi_get_performance(scgp, bp, cnt, ndesc, type, datatype)
	SCSI	*scgp;
	char *	bp;
	int	cnt;
	int	ndesc;
	int	type;
	int	datatype;
{
	register struct	scg_cmd	*scmd = scgp->scmd;

	fillbytes((char *) scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt;
	scmd->flags = SCG_RECV_DATA | SCG_DISRE_ENA;
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0xAC;
	scmd->cdb.g1_cdb.lun = scg_lun(scgp);
	scmd->cdb.cmd_cdb[1] |= datatype & 0x1F;
	scmd->cdb.cmd_cdb[9] = ndesc;
	scmd->cdb.cmd_cdb[10] = type;

	scgp->cmdname = "get performance";

	return (scg_cmd(scgp));
}


EXPORT int
scsi_get_perf_maxspeed(scgp, readp, writep, endp)
	SCSI	*scgp;
	unsigned long	*readp;
	unsigned long	*writep;
	unsigned long	*endp;
{
	register struct	scg_cmd	*scmd = scgp->scmd;
	struct mmc_performance_header	*ph;
	struct mmc_write_speed		*wsp;
#define	MAX_AMT	100
	char buffer[8 + MAX_AMT*16];
	unsigned long	ul;
	int	amt;
	int	i;
	int	mt = 0;
	int	ssp = 1;
	char	*mname = NULL;

	if (xdebug != 0) {
		mt = get_mediatype(scgp);
		ssp = get_singlespeed(mt);
		mname = get_mclassname(mt);
	}
	fillbytes((char *) buffer, sizeof (buffer), '\0');
	ph = (struct mmc_performance_header *)buffer;
	if (scsi_get_performance(scgp, buffer, 8+16, 1, 0x03, 0) < 0)
		return (-1);

	amt = (a_to_4_byte(ph->p_datalen) -4)/sizeof (struct mmc_write_speed);
	if (amt < 1)
		amt = 1;
	if (amt > MAX_AMT)
		amt = MAX_AMT;
	if (scsi_get_performance(scgp, buffer, 8+amt*16, amt, 0x03, 0) < 0)
		return (-1);

#ifdef	XDEBUG
	error(_("Bytes: %d\n"), scmd->size - scg_getresid(scgp));
	error(_("header: %ld\n"), a_to_4_byte(buffer) + 4);
#endif

	ph = (struct mmc_performance_header *)buffer;
	wsp = (struct mmc_write_speed *)(((char *)ph) +
				sizeof (struct mmc_performance_header));

	ul = a_to_u_4_byte(wsp->end_lba);
	if (endp)
		*endp = ul;

	ul = a_to_u_4_byte(wsp->read_speed);
	if (readp)
		*readp = ul;

	ul = a_to_u_4_byte(wsp->write_speed);
	if (writep)
		*writep = ul;

	wsp = (struct mmc_write_speed *)(((char *)ph) +
				sizeof (struct mmc_performance_header));

	i = (a_to_4_byte(buffer) -4)/sizeof (struct mmc_write_speed);
	if (i > scmd->cdb.cmd_cdb[9])
		i = scmd->cdb.cmd_cdb[9];
	if (xdebug > 0)
		error(_("MaxSpeed Nperf:   %d\n"), i);
	if (xdebug != 0) for (; --i >= 0; wsp++) {
		ul = a_to_u_4_byte(wsp->end_lba);
		error(_("End LBA:     %7lu\n"), ul);
		ul = a_to_u_4_byte(wsp->read_speed);
		error(_("Read Speed:  %7lu == %lux %s\n"), ul, ul/ssp, mname);
		ul = a_to_u_4_byte(wsp->write_speed);
		error(_("Write Speed: %7lu == %lux %s\n"), ul, ul/ssp, mname);
		error("\n");
	}
#ifdef	XDEBUG
	scg_prbytes(_("Performance data:"), (unsigned char *)buffer, scmd->size - scg_getresid(scgp));
#endif

	return (0);
#undef	MAX_AMT
}

EXPORT int
scsi_get_perf_curspeed(scgp, readp, writep, endp)
	SCSI	*scgp;
	unsigned long	*readp;
	unsigned long	*writep;
	unsigned long	*endp;
{
	register struct	scg_cmd	*scmd = scgp->scmd;
	struct mmc_performance_header	*ph;
	struct mmc_performance		*perfp;
#define	MAX_AMT	100
	char buffer[8 + MAX_AMT*16];
	unsigned long	ul;
	unsigned long	end;
	unsigned long	speed;
	int	amt;
	int	i;
	int	mt = 0;
	int	ssp = 1;
	char	*mname = NULL;

	if (xdebug != 0) {
		mt = get_mediatype(scgp);
		ssp = get_singlespeed(mt);
		mname = get_mclassname(mt);
	}

	if (endp || writep) {
		fillbytes((char *) buffer, sizeof (buffer), '\0');
		scgp->silent++;
		if (scsi_get_performance(scgp, buffer, 8+16, 1, 0x00, 0x04) < 0) {
			scgp->silent--;
			goto doread;
		}
		scgp->silent--;

		ph = (struct mmc_performance_header *)buffer;
		amt = (a_to_4_byte(ph->p_datalen) -4)/sizeof (struct mmc_performance);
		if (amt < 1)
			amt = 1;
		if (amt > MAX_AMT)
			amt = MAX_AMT;

		if (scsi_get_performance(scgp, buffer, 8+16*amt, amt, 0x00, 0x04) < 0)
			return (-1);

#ifdef	XDEBUG
		error(_("Bytes: %d\n"), scmd->size - scg_getresid(scgp));
		error(_("header: %ld\n"), a_to_4_byte(buffer) + 4);
#endif

		ph = (struct mmc_performance_header *)buffer;
		perfp = (struct mmc_performance *)(((char *)ph) +
				sizeof (struct mmc_performance_header));

		i = (a_to_4_byte(buffer) -4)/sizeof (struct mmc_performance);
		if (i > amt)
			i = amt;
		end = 0;
		speed = 0;
		for (; --i >= 0; perfp++) {
			ul = a_to_u_4_byte(perfp->end_lba);
			if (ul > end) {
				end = ul;
				ul = a_to_u_4_byte(perfp->end_perf);
				speed = ul;
			}
		}

		if (endp)
			*endp = end;

		if (writep)
			*writep = speed;

		perfp = (struct mmc_performance *)(((char *)ph) +
				sizeof (struct mmc_performance_header));
		i = (a_to_4_byte(buffer) -4)/sizeof (struct mmc_performance);
		if (i > scmd->cdb.cmd_cdb[9])
			i = scmd->cdb.cmd_cdb[9];
		if (xdebug > 1)
			error(_("CurSpeed Writeperf: %d\n"), i);
		else if (xdebug < 0)
			error(_("Write Performance:\n"));
		if (xdebug != 0) for (; --i >= 0; perfp++) {
			ul = a_to_u_4_byte(perfp->start_lba);
			error(_("START LBA:   %7lu\n"), ul);
			ul = a_to_u_4_byte(perfp->end_lba);
			error(_("End LBA:     %7lu\n"), ul);
			ul = a_to_u_4_byte(perfp->start_perf);
			error(_("Start Perf:  %7lu == %lux %s\n"), ul, ul/ssp, mname);
			ul = a_to_u_4_byte(perfp->end_perf);
			error(_("END Perf:    %7lu == %lux %s\n"), ul, ul/ssp, mname);
			error("\n");
		}
#ifdef	XDEBUG
		scg_prbytes(_("Performance data:"), (unsigned char *)buffer, scmd->size - scg_getresid(scgp));
#endif
	}
doread:
	if (readp) {
		fillbytes((char *) buffer, sizeof (buffer), '\0');
		scgp->silent++;
		if (scsi_get_performance(scgp, buffer, 8+16, 1, 0x00, 0x00) < 0) {
			scgp->silent--;
			return (-1);
		}
		scgp->silent--;

		ph = (struct mmc_performance_header *)buffer;
		amt = (a_to_4_byte(ph->p_datalen) -4)/sizeof (struct mmc_performance);
		if (amt < 1)
			amt = 1;
		if (amt > MAX_AMT)
			amt = MAX_AMT;

		if (scsi_get_performance(scgp, buffer, 8+16*amt, amt, 0x00, 0x00) < 0)
			return (-1);

#ifdef	XDEBUG
		error(_("Bytes: %d\n"), scmd->size - scg_getresid(scgp));
		error(_("header: %ld\n"), a_to_4_byte(buffer) + 4);
#endif

		ph = (struct mmc_performance_header *)buffer;
		perfp = (struct mmc_performance *)(((char *)ph) +
				sizeof (struct mmc_performance_header));

		i = (a_to_4_byte(buffer) -4)/sizeof (struct mmc_performance);
		if (i > amt)
			i = amt;
		end = 0;
		speed = 0;
		for (; --i >= 0; perfp++) {
			ul = a_to_u_4_byte(perfp->end_lba);
			if (ul > end) {
				end = ul;
				ul = a_to_u_4_byte(perfp->end_perf);
				speed = ul;
			}
		}

		if (readp)
			*readp = speed;

		i = (a_to_4_byte(buffer) -4)/sizeof (struct mmc_performance);
		if (i > scmd->cdb.cmd_cdb[9])
			i = scmd->cdb.cmd_cdb[9];
		if (xdebug > 1)
			error(_("CurSpeed Readperf: %d\n"), i);
		else if (xdebug < 0)
			error(_("Read Performance:\n"));
		if (xdebug != 0) for (; --i >= 0; perfp++) {
			ul = a_to_u_4_byte(perfp->start_lba);
			error(_("START LBA:   %7lu\n"), ul);
			ul = a_to_u_4_byte(perfp->end_lba);
			error(_("End LBA:     %7lu\n"), ul);
			ul = a_to_u_4_byte(perfp->start_perf);
			error(_("Start Perf:  %7lu == %lux %s\n"), ul, ul/ssp, mname);
			ul = a_to_u_4_byte(perfp->end_perf);
			error(_("END Perf:    %7lu == %lux %s\n"), ul, ul/ssp, mname);
			error("\n");
		}
#ifdef	XDEBUG
		scg_prbytes(_("Performance data:"), (unsigned char *)buffer, scmd->size - scg_getresid(scgp));
#endif
	}

	return (0);
}

/*
 * RB_RETROBEAM_MEDIA_INTELLIGENCE_28
 *
 * Modern MMC media/firmware intelligence.
 *
 * These commands only query the drive.  They do not select a write strategy,
 * change speed, perform OPC, modify mode pages, or write media.
 *
 * GET PERFORMANCE Data Type layout for Type 00h:
 *   bits 4:3 TOLERANCE
 *   bit  2   WRITE
 *   bits 1:0 EXCEPT
 *
 * Windows optical HLK uses Data Type 14h for nominal write performance:
 *   TOLERANCE=10b, WRITE=1, EXCEPT=00b.
 * Therefore write-performance exceptions-only is 16h:
 *   TOLERANCE=10b, WRITE=1, EXCEPT=10b.
 */
#define RB_MI_MAX_SPEED_DESCRIPTORS	64
#define RB_MI_MAX_PERF_DESCRIPTORS	64
#define RB_MI_MAX_EXCEPTIONS		128

LOCAL int
rb_get_performance_info(SCSI *scgp, char *bp, int cnt,
			uint32_t starting_lba, uint16_t ndesc,
			uint8_t type, uint8_t datatype)
{
	struct scg_cmd	*scmd = scgp->scmd;

	fillbytes((char *)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt;
	scmd->flags = SCG_RECV_DATA | SCG_DISRE_ENA;
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0xAC;
	scmd->cdb.g1_cdb.lun = scg_lun(scgp);
	scmd->cdb.cmd_cdb[1] = datatype & 0x1F;
	i_to_4_byte(&scmd->cdb.cmd_cdb[2], starting_lba);
	i_to_2_byte(&scmd->cdb.cmd_cdb[8], ndesc);
	scmd->cdb.cmd_cdb[10] = type;

	scgp->cmdname = "get performance (RetroBeam media intelligence)";
	return (scg_cmd(scgp));
}

LOCAL int
rb_get_performance_optional(SCSI *scgp, char *bp, int cnt,
			    uint32_t starting_lba, uint16_t ndesc,
			    uint8_t type, uint8_t datatype)
{
	int	ret;

	scgp->silent++;
	ret = rb_get_performance_info(scgp, bp, cnt,
	    starting_lba, ndesc, type, datatype);
	scgp->silent--;
	return (ret);
}

LOCAL int
rb_performance_descriptor_count(const unsigned char *bp, int descriptor_size)
{
	uint32_t	datalen;

	if (descriptor_size <= 0)
		return (0);

	datalen = a_to_u_4_byte(bp);
	if (datalen < 4)
		return (0);

	return ((int)((datalen - 4) / (uint32_t)descriptor_size));
}

LOCAL const char *
rb_rotation_name(unsigned int wrc)
{
	switch (wrc) {
	case WRC_DEF_RC:
		return ("media/firmware default");
	case WRC_CAV:
		return ("CAV");
	default:
		return ("reserved/drive-specific");
	}
}

LOCAL void
rb_print_realtime_streaming(SCSI *scgp)
{
	unsigned char	b[32];
	int		amt;
	int		ret;
	unsigned int	feature;
	unsigned int	flags;
	unsigned int	caps;

	fillbytes((char *)b, sizeof (b), '\0');

	scgp->silent++;
	ret = get_configuration(scgp, (char *)b, sizeof (b), 0x0107, 2);
	scgp->silent--;

	if (ret < 0) {
		printf(_("  Real-time streaming feature (0x0107): unavailable/unsupported\n"));
		printf(_("  [RBMI] realtime_streaming=unavailable\n"));
		return;
	}

	amt = sizeof (b) - scg_getresid(scgp);
	if (amt < 13) {
		printf(_("  Real-time streaming feature (0x0107): short response (%d bytes)\n"),
		    amt);
		printf(_("  [RBMI] realtime_streaming=short_response bytes=%d\n"), amt);
		return;
	}

	feature = a_to_u_2_byte(&b[8]);
	if (feature != 0x0107 || b[11] < 1) {
		printf(_("  Real-time streaming feature (0x0107): not returned\n"));
		printf(_("  [RBMI] realtime_streaming=not_returned\n"));
		return;
	}

	flags = b[10];
	caps = b[12];

	printf(_("  Real-time streaming feature (0x0107): %s%s\n"),
	    (flags & 0x01) ? _("current") : _("not current"),
	    (flags & 0x02) ? _(", persistent") : "");
	printf(_("    Stream recording:              %s\n"),
	    (caps & 0x01) ? _("yes") : _("no"));
	printf(_("    Write speeds via GET PERFORMANCE: %s\n"),
	    (caps & 0x02) ? _("yes") : _("no"));
	printf(_("    Write speeds via Mode Page 2Ah: %s\n"),
	    (caps & 0x04) ? _("yes") : _("no"));
	printf(_("    SET CD SPEED:                  %s\n"),
	    (caps & 0x08) ? _("yes") : _("no"));
	printf(_("    READ BUFFER CAPACITY:          %s\n"),
	    (caps & 0x10) ? _("yes") : _("no"));

	printf(_("  [RBMI] realtime_streaming=current:%u persistent:%u stream_recording:%u get_performance_wspd:%u mode_page_2a_wspd:%u set_cd_speed:%u read_buffer_capacity:%u\n"),
	    (flags & 0x01) ? 1U : 0U,
	    (flags & 0x02) ? 1U : 0U,
	    (caps & 0x01) ? 1U : 0U,
	    (caps & 0x02) ? 1U : 0U,
	    (caps & 0x04) ? 1U : 0U,
	    (caps & 0x08) ? 1U : 0U,
	    (caps & 0x10) ? 1U : 0U);
}

LOCAL void
rb_print_write_speed_descriptors(SCSI *scgp, int mt)
{
	struct mmc_performance_header	*ph;
	struct mmc_write_speed		*wsp;
	unsigned char	buffer[8 +
	    RB_MI_MAX_SPEED_DESCRIPTORS * sizeof (struct mmc_write_speed)];
	int		count;
	int		fetch;
	int		i;
	int		ret;
	int		datatype;
	int		ssp;

	fillbytes((char *)buffer, sizeof (buffer), '\0');

	/*
	 * 14h is the current MMC/Windows-HLK form:
	 * 10% nominal tolerance + write + nominal.
	 * Some older firmware accepts the legacy zero Data Type for Type 03h,
	 * so retain that only as a query fallback.
	 */
	datatype = 0x14;
	ret = rb_get_performance_optional(scgp, (char *)buffer,
	    8 + sizeof (struct mmc_write_speed), 0, 1, 0x03, datatype);
	if (ret < 0) {
		datatype = 0x00;
		ret = rb_get_performance_optional(scgp, (char *)buffer,
		    8 + sizeof (struct mmc_write_speed), 0, 1, 0x03, datatype);
	}

	if (ret < 0) {
		printf(_("  Current-media write-speed descriptors: unavailable/unsupported\n"));
		printf(_("  [RBMI] write_speed_descriptors=unavailable\n"));
		return;
	}

	ph = (struct mmc_performance_header *)buffer;
	count = rb_performance_descriptor_count(buffer,
	    sizeof (struct mmc_write_speed));

	if (count <= 0) {
		printf(_("  Current-media write-speed descriptors: none reported\n"));
		printf(_("  [RBMI] write_speed_descriptors=0 datatype=0x%02X\n"),
		    datatype);
		return;
	}

	fetch = count;
	if (fetch > RB_MI_MAX_SPEED_DESCRIPTORS)
		fetch = RB_MI_MAX_SPEED_DESCRIPTORS;

	fillbytes((char *)buffer, sizeof (buffer), '\0');
	ret = rb_get_performance_optional(scgp, (char *)buffer,
	    8 + fetch * sizeof (struct mmc_write_speed),
	    0, (uint16_t)fetch, 0x03, (uint8_t)datatype);
	if (ret < 0) {
		printf(_("  Current-media write-speed descriptors: second query failed\n"));
		printf(_("  [RBMI] write_speed_descriptors=query_failed\n"));
		return;
	}

	ph = (struct mmc_performance_header *)buffer;
	count = rb_performance_descriptor_count(buffer,
	    sizeof (struct mmc_write_speed));
	if (count > fetch)
		count = fetch;

	printf(_("  Current-media write-speed descriptors: %d"), count);
	if (datatype == 0x00)
		printf(_(" (legacy-compatible query fallback)"));
	printf("\n");

	ssp = get_singlespeed(mt);
	wsp = (struct mmc_write_speed *)(((char *)ph) +
	    sizeof (struct mmc_performance_header));

	for (i = 0; i < count; i++, wsp++) {
		unsigned long	end_lba;
		unsigned long	read_speed;
		unsigned long	write_speed;

		end_lba = a_to_u_4_byte(wsp->end_lba);
		read_speed = a_to_u_4_byte(wsp->read_speed);
		write_speed = a_to_u_4_byte(wsp->write_speed);

		if (ssp > 0) {
			printf(_("    %2d: write %7lu kB/s (~%.1fx), read %7lu kB/s, end LBA %lu,\n"),
			    i + 1, write_speed,
			    (double)write_speed / (double)ssp,
			    read_speed, end_lba);
		} else {
			printf(_("    %2d: write %7lu kB/s, read %7lu kB/s, end LBA %lu,\n"),
			    i + 1, write_speed, read_speed, end_lba);
		}
		printf(_("        rotation=%s, exact=%s, media-rotation-defined=%s, mixed-r/w=%s\n"),
		    rb_rotation_name(wsp->p_wrc),
		    wsp->p_exact ? _("yes") : _("no"),
		    wsp->p_rdd ? _("yes") : _("no"),
		    wsp->p_mrw ? _("yes") : _("no"));

		printf(_("  [RBMI] wspd index=%d write_kbps=%lu read_kbps=%lu end_lba=%lu wrc=%u exact=%u rdd=%u mrw=%u\n"),
		    i, write_speed, read_speed, end_lba,
		    (unsigned int)wsp->p_wrc,
		    wsp->p_exact ? 1U : 0U,
		    wsp->p_rdd ? 1U : 0U,
		    wsp->p_mrw ? 1U : 0U);
	}

	if (fetch < rb_performance_descriptor_count(buffer,
	    sizeof (struct mmc_write_speed))) {
		printf(_("    ... descriptor list truncated by RetroBeam diagnostic cap\n"));
	}
}

LOCAL void
rb_print_nominal_write_performance(SCSI *scgp, int mt)
{
	struct mmc_performance_header	*ph;
	struct mmc_performance		*perfp;
	unsigned char	buffer[8 +
	    RB_MI_MAX_PERF_DESCRIPTORS * sizeof (struct mmc_performance)];
	int		count;
	int		fetch;
	int		i;
	int		ret;
	int		datatype;
	int		ssp;

	fillbytes((char *)buffer, sizeof (buffer), '\0');

	/*
	 * 14h = TOLERANCE 10b, WRITE 1, EXCEPT nominal (00b).
	 * Fall back to the historical Schilling 04h query for firmware that
	 * predates the modern tolerance form.
	 */
	datatype = 0x14;
	ret = rb_get_performance_optional(scgp, (char *)buffer,
	    8 + sizeof (struct mmc_performance), 0, 1, 0x00, datatype);
	if (ret < 0) {
		datatype = 0x04;
		ret = rb_get_performance_optional(scgp, (char *)buffer,
		    8 + sizeof (struct mmc_performance), 0, 1, 0x00, datatype);
	}

	if (ret < 0) {
		printf(_("  Nominal write-performance map: unavailable/unsupported\n"));
		printf(_("  [RBMI] nominal_write_performance=unavailable\n"));
		return;
	}

	count = rb_performance_descriptor_count(buffer,
	    sizeof (struct mmc_performance));
	if (count <= 0) {
		printf(_("  Nominal write-performance map: no descriptors\n"));
		printf(_("  [RBMI] nominal_write_performance=0 datatype=0x%02X\n"),
		    datatype);
		return;
	}

	fetch = count;
	if (fetch > RB_MI_MAX_PERF_DESCRIPTORS)
		fetch = RB_MI_MAX_PERF_DESCRIPTORS;

	fillbytes((char *)buffer, sizeof (buffer), '\0');
	ret = rb_get_performance_optional(scgp, (char *)buffer,
	    8 + fetch * sizeof (struct mmc_performance),
	    0, (uint16_t)fetch, 0x00, (uint8_t)datatype);
	if (ret < 0) {
		printf(_("  Nominal write-performance map: second query failed\n"));
		printf(_("  [RBMI] nominal_write_performance=query_failed\n"));
		return;
	}

	ph = (struct mmc_performance_header *)buffer;
	count = rb_performance_descriptor_count(buffer,
	    sizeof (struct mmc_performance));
	if (count > fetch)
		count = fetch;

	printf(_("  Nominal write-performance map: %d extent%s"),
	    count, count == 1 ? "" : "s");
	if (datatype == 0x04)
		printf(_(" (legacy-compatible tolerance fallback)"));
	printf("\n");

	ssp = get_singlespeed(mt);
	perfp = (struct mmc_performance *)(((char *)ph) +
	    sizeof (struct mmc_performance_header));

	for (i = 0; i < count; i++, perfp++) {
		unsigned long	start_lba;
		unsigned long	start_perf;
		unsigned long	end_lba;
		unsigned long	end_perf;

		start_lba = a_to_u_4_byte(perfp->start_lba);
		start_perf = a_to_u_4_byte(perfp->start_perf);
		end_lba = a_to_u_4_byte(perfp->end_lba);
		end_perf = a_to_u_4_byte(perfp->end_perf);

		if (ssp > 0) {
			printf(_("    %2d: LBA %lu .. %lu : %lu -> %lu kB/s (~%.1fx -> %.1fx)\n"),
			    i + 1, start_lba, end_lba,
			    start_perf, end_perf,
			    (double)start_perf / (double)ssp,
			    (double)end_perf / (double)ssp);
		} else {
			printf(_("    %2d: LBA %lu .. %lu : %lu -> %lu kB/s\n"),
			    i + 1, start_lba, end_lba, start_perf, end_perf);
		}

		printf(_("  [RBMI] nominal index=%d start_lba=%lu end_lba=%lu start_kbps=%lu end_kbps=%lu\n"),
		    i, start_lba, end_lba, start_perf, end_perf);
	}
}

LOCAL void
rb_print_write_performance_exceptions(SCSI *scgp)
{
	struct mmc_performance_header	*ph;
	struct mmc_exceptions		*ep;
	unsigned char	buffer[8 +
	    RB_MI_MAX_EXCEPTIONS * sizeof (struct mmc_exceptions)];
	int		count;
	int		fetch;
	int		i;
	int		ret;

	fillbytes((char *)buffer, sizeof (buffer), '\0');

	/*
	 * 16h = TOLERANCE 10b, WRITE 1, EXCEPT exceptions-only (10b).
	 * The returned six-byte descriptor is:
	 *   LBA[4], additional delay in tenths of a millisecond[2].
	 */
	ret = rb_get_performance_optional(scgp, (char *)buffer,
	    8 + sizeof (struct mmc_exceptions), 0, 1, 0x00, 0x16);

	if (ret < 0) {
		printf(_("  Write-performance exceptions: unavailable/unsupported\n"));
		printf(_("  [RBMI] write_exceptions=unavailable\n"));
		return;
	}

	ph = (struct mmc_performance_header *)buffer;
	count = rb_performance_descriptor_count(buffer,
	    sizeof (struct mmc_exceptions));

	if (count <= 0) {
		printf(_("  Write-performance exceptions: none reported\n"));
		printf(_("  [RBMI] write_exceptions=0\n"));
		return;
	}

	fetch = count;
	if (fetch > RB_MI_MAX_EXCEPTIONS)
		fetch = RB_MI_MAX_EXCEPTIONS;

	fillbytes((char *)buffer, sizeof (buffer), '\0');
	ret = rb_get_performance_optional(scgp, (char *)buffer,
	    8 + fetch * sizeof (struct mmc_exceptions),
	    0, (uint16_t)fetch, 0x00, 0x16);
	if (ret < 0) {
		printf(_("  Write-performance exceptions: second query failed\n"));
		printf(_("  [RBMI] write_exceptions=query_failed\n"));
		return;
	}

	ph = (struct mmc_performance_header *)buffer;
	count = rb_performance_descriptor_count(buffer,
	    sizeof (struct mmc_exceptions));
	if (count > fetch)
		count = fetch;

	if (!ph->p_exept) {
		printf(_("  Write-performance exceptions: drive returned nominal format unexpectedly\n"));
		printf(_("  [RBMI] write_exceptions=unexpected_nominal_format\n"));
		return;
	}

	printf(_("  Write-performance exceptions: %d location%s\n"),
	    count, count == 1 ? "" : "s");

	ep = (struct mmc_exceptions *)(((char *)ph) +
	    sizeof (struct mmc_performance_header));

	for (i = 0; i < count; i++, ep++) {
		unsigned long	lba;
		unsigned int	tenths_ms;
		double		mib;

		lba = a_to_u_4_byte(ep->lba);
		tenths_ms = a_to_u_2_byte(ep->time);
		mib = ((double)lba * 2048.0) / (1024.0 * 1024.0);

		printf(_("    %2d: LBA %lu (~%.1f MiB), additional delay %.1f ms\n"),
		    i + 1, lba, mib, (double)tenths_ms / 10.0);

		printf(_("  [RBMI] exception index=%d lba=%lu approx_mib=%.1f additional_delay_tenths_ms=%u\n"),
		    i, lba, mib, tenths_ms);
	}

	if (fetch == RB_MI_MAX_EXCEPTIONS)
		printf(_("    ... exception list capped at %d entries\n"),
		    RB_MI_MAX_EXCEPTIONS);
}

/*
 * RB_RETROBEAM_VENDOR_FINGERPRINT_32
 *
 * Query-only optical-drive fingerprinting.
 *
 * This deliberately does not issue guessed vendor-specific commands.  It
 * inventories the standard discovery surfaces that MMC/SPC optical drives
 * actually expose, preserving raw descriptors so a later vendor module can
 * be matched against documented/open-source implementations.
 */
#define RB_VF_FEATURE_BUFFER_SIZE	(32 * 1024)
#define RB_VF_MODE_BUFFER_SIZE		4096
#define RB_VF_VPD_BUFFER_SIZE		255
#define RB_VF_MAX_VPD_PAGES		64

LOCAL void
rb_vf_hex(const unsigned char *bp, int len)
{
	int	i;

	for (i = 0; i < len; i++)
		printf("%02X", (unsigned int)bp[i]);
}

LOCAL int
rb_vf_inquiry_vpd(SCSI *scgp, unsigned int page,
			unsigned char *bp, int cnt)
{
	struct scg_cmd	*scmd = scgp->scmd;

	if (cnt > 255)
		cnt = 255;

	fillbytes((char *)bp, cnt, '\0');
	fillbytes((char *)scmd, sizeof (*scmd), '\0');

	scmd->addr = (char *)bp;
	scmd->size = cnt;
	scmd->flags = SCG_RECV_DATA | SCG_DISRE_ENA;
	scmd->cdb_len = SC_G0_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.cmd_cdb[0] = 0x12;		/* INQUIRY */
	scmd->cdb.cmd_cdb[1] = 0x01;		/* EVPD */
	scmd->cdb.cmd_cdb[2] = page & 0xFF;
	scmd->cdb.cmd_cdb[4] = cnt & 0xFF;

	scgp->cmdname = "inquiry vpd (RetroBeam vendor fingerprint)";
	return (scg_cmd(scgp));
}

LOCAL int
rb_vf_mode_sense10_subpages(SCSI *scgp, unsigned char *bp, int cnt)
{
	struct scg_cmd	*scmd = scgp->scmd;

	fillbytes((char *)bp, cnt, '\0');
	fillbytes((char *)scmd, sizeof (*scmd), '\0');

	scmd->addr = (char *)bp;
	scmd->size = cnt;
	scmd->flags = SCG_RECV_DATA | SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.cmd_cdb[0] = 0x5A;		/* MODE SENSE(10) */
	scmd->cdb.cmd_cdb[2] = 0x3F;		/* all page codes, current */
	scmd->cdb.cmd_cdb[3] = 0xFF;		/* all subpages */
	i_to_2_byte(&scmd->cdb.cmd_cdb[7], cnt);

	scgp->cmdname = "mode sense all subpages (RetroBeam vendor fingerprint)";
	return (scg_cmd(scgp));
}

LOCAL void
rb_vf_print_features(SCSI *scgp)
{
	unsigned char	fbuf[RB_VF_FEATURE_BUFFER_SIZE];
	unsigned char	*p;
	unsigned char	*pend;
	int		amt;
	int		ret;
	int		declared;

	fillbytes((char *)fbuf, sizeof (fbuf), '\0');

	scgp->silent++;
	ret = get_configuration(scgp, (char *)fbuf, sizeof (fbuf), 0, 0);
	scgp->silent--;

	if (ret < 0) {
		printf(_("  Feature inventory: unavailable\n"));
		printf(_("  [RBVF] feature_inventory=unavailable\n"));
		return;
	}

	amt = sizeof (fbuf) - scg_getresid(scgp);
	if (amt < 8) {
		printf(_("  Feature inventory: short response (%d bytes)\n"), amt);
		printf(_("  [RBVF] feature_inventory=short bytes=%d\n"), amt);
		return;
	}

	declared = (int)a_to_u_4_byte(fbuf) + 4;
	if (declared < amt)
		amt = declared;

	p = fbuf + 8;
	pend = fbuf + amt;

	printf(_("  MMC feature descriptors:\n"));

	while (p + 4 <= pend) {
		unsigned int	code;
		unsigned int	flags;
		unsigned int	version;
		unsigned int	len;
		int		total;
		const char	*class_name;

		code = a_to_u_2_byte(p);
		flags = p[2];
		version = (flags >> 2) & 0x0F;
		len = p[3];
		total = 4 + (int)len;

		if (p + total > pend) {
			printf(_("    truncated descriptor at feature 0x%04X\n"), code);
			printf(_("  [RBVF] feature_truncated code=0x%04X\n"), code);
			break;
		}

		if (fname_known(code))
			class_name = "known_mmc";
		else if (code >= 0xFF00)
			class_name = "vendor_specific_range";
		else
			class_name = "unmapped_or_newer";

		printf(_("    0x%04X %-28s current=%u persistent=%u version=%u len=%u raw="),
		    code,
		    fname_known(code) ? fname(code) : "Unmapped",
		    (flags & 0x01) ? 1U : 0U,
		    (flags & 0x02) ? 1U : 0U,
		    version,
		    len);
		rb_vf_hex(p, total);
		printf("\n");

		printf(_("  [RBVF] feature code=0x%04X class=%s current=%u persistent=%u version=%u len=%u raw="),
		    code,
		    class_name,
		    (flags & 0x01) ? 1U : 0U,
		    (flags & 0x02) ? 1U : 0U,
		    version,
		    len);
		rb_vf_hex(p, total);
		printf("\n");

		p += total;
	}
}

LOCAL void
rb_vf_print_vpd(SCSI *scgp)
{
	unsigned char	list[RB_VF_VPD_BUFFER_SIZE];
	unsigned char	pagebuf[RB_VF_VPD_BUFFER_SIZE];
	int		amt;
	int		plen;
	int		count;
	int		i;
	int		ret;

	fillbytes((char *)list, sizeof (list), '\0');

	scgp->silent++;
	ret = rb_vf_inquiry_vpd(scgp, 0x00, list, sizeof (list));
	scgp->silent--;

	if (ret < 0) {
		printf(_("  INQUIRY VPD: unsupported/unavailable\n"));
		printf(_("  [RBVF] vpd_inventory=unavailable\n"));
		return;
	}

	amt = sizeof (list) - scg_getresid(scgp);
	if (amt < 4) {
		printf(_("  INQUIRY VPD: short page-00 response (%d bytes)\n"), amt);
		printf(_("  [RBVF] vpd_inventory=short bytes=%d\n"), amt);
		return;
	}

	plen = (int)a_to_u_2_byte(&list[2]);
	if (plen > amt - 4)
		plen = amt - 4;
	if (plen < 0)
		plen = 0;

	count = plen;
	if (count > RB_VF_MAX_VPD_PAGES)
		count = RB_VF_MAX_VPD_PAGES;

	printf(_("  Supported INQUIRY VPD pages (%d reported"), plen);
	if (count < plen)
		printf(_(", first %d queried"), count);
	printf("):");
	for (i = 0; i < plen; i++)
		printf(" %02X", (unsigned int)list[4 + i]);
	printf("\n");

	printf(_("  [RBVF] vpd_supported count=%d pages="), plen);
	for (i = 0; i < plen; i++)
		printf("%02X%s", (unsigned int)list[4 + i],
		    i + 1 == plen ? "" : ",");
	printf("\n");

	for (i = 0; i < count; i++) {
		unsigned int	page;
		int		pamt;
		int		pdecl;
		const char	*class_name;

		page = list[4 + i];

		fillbytes((char *)pagebuf, sizeof (pagebuf), '\0');
		scgp->silent++;
		ret = rb_vf_inquiry_vpd(scgp, page, pagebuf, sizeof (pagebuf));
		scgp->silent--;

		if (ret < 0) {
			printf(_("  [RBVF] vpd page=0x%02X result=rejected\n"), page);
			continue;
		}

		pamt = sizeof (pagebuf) - scg_getresid(scgp);
		if (pamt < 4) {
			printf(_("  [RBVF] vpd page=0x%02X result=short bytes=%d\n"),
			    page, pamt);
			continue;
		}

		pdecl = (int)a_to_u_2_byte(&pagebuf[2]) + 4;
		if (pdecl < pamt)
			pamt = pdecl;

		class_name = page >= 0xC0 ?
		    "vendor_specific_range" : "standard_or_device_specific";

		printf(_("    VPD 0x%02X class=%s len=%d raw="),
		    page, class_name, pamt);
		rb_vf_hex(pagebuf, pamt);
		printf("\n");

		printf(_("  [RBVF] vpd page=0x%02X class=%s len=%d raw="),
		    page, class_name, pamt);
		rb_vf_hex(pagebuf, pamt);
		printf("\n");
	}
}

LOCAL void
rb_vf_parse_mode_pages(const unsigned char *mode, int amt,
			const char *source)
{
	const unsigned char	*p;
	const unsigned char	*pend;
	int			total;
	int			block_desc_len;

	if (amt < 8) {
		printf(_("  [RBVF] mode_inventory source=%s result=short bytes=%d\n"),
		    source, amt);
		return;
	}

	total = (int)a_to_u_2_byte(mode) + 2;
	if (total < amt)
		amt = total;

	block_desc_len = (int)a_to_u_2_byte(&mode[6]);
	if (8 + block_desc_len > amt) {
		printf(_("  [RBVF] mode_inventory source=%s result=bad_block_descriptor length=%d bytes=%d\n"),
		    source, block_desc_len, amt);
		return;
	}

	p = mode + 8 + block_desc_len;
	pend = mode + amt;

	while (p + 2 <= pend) {
		unsigned int	page;
		unsigned int	subpage;
		unsigned int	ps;
		unsigned int	spf;
		int		plen;
		int		ptotal;
		const char	*class_name;

		ps = (p[0] & 0x80) ? 1U : 0U;
		spf = (p[0] & 0x40) ? 1U : 0U;
		page = p[0] & 0x3F;
		subpage = 0;

		if (spf) {
			if (p + 4 > pend)
				break;
			subpage = p[1];
			plen = (int)a_to_u_2_byte(&p[2]);
			ptotal = 4 + plen;
		} else {
			plen = p[1];
			ptotal = 2 + plen;
		}

		if (ptotal <= 0 || p + ptotal > pend) {
			printf(_("  [RBVF] mode_truncated source=%s page=0x%02X subpage=0x%02X\n"),
			    source, page, subpage);
			break;
		}

		class_name = (page == 0x00 || page >= 0x30) ?
		    "vendor_candidate_range" : "standard_or_device_specific";

		printf(_("    Mode page 0x%02X"), page);
		if (spf)
			printf(_("/0x%02X"), subpage);
		printf(_(" class=%s ps=%u spf=%u len=%d raw="),
		    class_name, ps, spf, ptotal);
		rb_vf_hex(p, ptotal);
		printf("\n");

		printf(_("  [RBVF] mode source=%s page=0x%02X subpage=0x%02X class=%s ps=%u spf=%u len=%d raw="),
		    source, page, subpage, class_name, ps, spf, ptotal);
		rb_vf_hex(p, ptotal);
		printf("\n");

		p += ptotal;
	}
}

LOCAL void
rb_vf_print_mode_pages(SCSI *scgp)
{
	unsigned char	mode[RB_VF_MODE_BUFFER_SIZE];
	int		amt;
	int		ret;

	fillbytes((char *)mode, sizeof (mode), '\0');

	scgp->silent++;
	ret = mode_sense_g1(scgp, mode, sizeof (mode), 0x3F, 0);
	scgp->silent--;

	if (ret < 0) {
		printf(_("  MODE SENSE all-pages: unavailable\n"));
		printf(_("  [RBVF] mode_inventory source=all_pages result=unavailable\n"));
	} else {
		amt = sizeof (mode) - scg_getresid(scgp);
		printf(_("  MODE SENSE(10) current pages:\n"));
		rb_vf_parse_mode_pages(mode, amt, "all_pages");
	}

	fillbytes((char *)mode, sizeof (mode), '\0');

	scgp->silent++;
	ret = rb_vf_mode_sense10_subpages(scgp, mode, sizeof (mode));
	scgp->silent--;

	if (ret < 0) {
		printf(_("  MODE SENSE all-subpages request: unsupported/rejected\n"));
		printf(_("  [RBVF] mode_inventory source=all_subpages result=unavailable\n"));
	} else {
		amt = sizeof (mode) - scg_getresid(scgp);
		printf(_("  MODE SENSE(10) all-subpages response:\n"));
		rb_vf_parse_mode_pages(mode, amt, "all_subpages");
	}
}

EXPORT int
retrobeam_vendor_fingerprint(SCSI *scgp)
{
	printf("\n");
	printf(_("RetroBeam vendor fingerprint (query-only):\n"));
	printf(_("  Drive inquiry vendor:  '%-8.8s'\n"),
	    scgp->inq != NULL ? scgp->inq->inq_vendor_info : "");
	printf(_("  Drive inquiry product: '%-16.16s'\n"),
	    scgp->inq != NULL ? scgp->inq->inq_prod_ident : "");
	printf(_("  Drive inquiry revision:'%-4.4s'\n"),
	    scgp->inq != NULL ? scgp->inq->inq_prod_revision : "");

	printf(_("  [RBVF] identity vendor='%-8.8s' product='%-16.16s' revision='%-4.4s'\n"),
	    scgp->inq != NULL ? scgp->inq->inq_vendor_info : "",
	    scgp->inq != NULL ? scgp->inq->inq_prod_ident : "",
	    scgp->inq != NULL ? scgp->inq->inq_prod_revision : "");

	rb_vf_print_features(scgp);
	printf(_("  INQUIRY VPD: disabled in v73; experimental EVPD response is not trusted on this path.\n"));
	printf(_("  [RBVF] vpd_inventory=disabled_untrusted\n"));
	rb_vf_print_mode_pages(scgp);

	printf(_("End RetroBeam vendor fingerprint.\n"));
	printf("\n");
	return (0);
}
EXPORT int
retrobeam_media_intelligence(SCSI *scgp)
{
	struct disk_info	di;
	unsigned long		current_write = 0;
	unsigned long		current_end = 0;
	int			mt;
	int			profile;
	int			ret;

	printf("\n");
	printf(_("RetroBeam media intelligence:\n"));

	profile = get_curprofile(scgp);
	mt = get_mediatype(scgp);

	if (profile >= 0) {
		printf(_("  Current profile: %s (0x%04X)\n"),
		    profile == 0 ? _("none") : pname((unsigned int)profile),
		    (unsigned int)profile);
		printf(_("  [RBMI] profile=0x%04X media_class=%s\n"),
		    (unsigned int)profile,
		    mt > MT_NONE ? get_mclassname(mt) : "unknown");
	} else {
		printf(_("  Current profile: unavailable\n"));
		printf(_("  [RBMI] profile=unavailable\n"));
	}

	rb_print_realtime_streaming(scgp);
	rb_print_write_speed_descriptors(scgp, mt);
	rb_print_nominal_write_performance(scgp, mt);
	rb_print_write_performance_exceptions(scgp);

	ret = scsi_get_perf_curspeed(scgp, NULL, &current_write, &current_end);
	if (ret >= 0) {
		int	ssp = get_singlespeed(mt);

		if (ssp > 0) {
			printf(_("  Firmware current write performance: %lu kB/s (~%.1fx), end LBA %lu\n"),
			    current_write,
			    (double)current_write / (double)ssp,
			    current_end);
		} else {
			printf(_("  Firmware current write performance: %lu kB/s, end LBA %lu\n"),
			    current_write, current_end);
		}
		printf(_("  [RBMI] current_write_kbps=%lu current_write_end_lba=%lu\n"),
		    current_write, current_end);
	} else {
		printf(_("  Firmware current write performance: unavailable/unsupported\n"));
		printf(_("  [RBMI] current_write_performance=unavailable\n"));
	}

	fillbytes((char *)&di, sizeof (di), '\0');
	scgp->silent++;
	ret = get_diskinfo(scgp, &di, sizeof (di));
	scgp->silent--;
	if (ret >= 0) {
		printf(_("  OPC descriptors currently recorded: %u\n"),
		    (unsigned int)di.num_opc_entries);
		printf(_("  [RBMI] opc_descriptors=%u\n"),
		    (unsigned int)di.num_opc_entries);
	} else {
		printf(_("  OPC descriptors currently recorded: unavailable\n"));
		printf(_("  [RBMI] opc_descriptors=unavailable\n"));
	}

		if (driveropts != NULL &&
	    hasdrvopt(driveropts, "vendorprobe") != NULL)
		retrobeam_vendor_fingerprint(scgp);
printf(_("End RetroBeam media intelligence.\n"));
	printf("\n");

	return (0);
}

/*
 * RetroBeam standards-based SET STREAMING policy.
 *
 * These options are deliberately separate from vendor-specific write
 * strategy/laser-power experiments. They map directly to MMC-defined
 * controls and are safe to capability-test without writing user data.
 */
typedef struct rb_streaming_policy {
	int	wrc;
	BOOL	exact;
	BOOL	restore;
	BOOL	advanced;
} rb_streaming_policy_t;

LOCAL BOOL
rb_stream_value_is(const char *p, const char *value)
{
	const char	*comma;
	size_t		len;

	if (p == NULL || value == NULL)
		return (FALSE);

	comma = strchr(p, ',');
	len = comma != NULL ? (size_t)(comma - p) : strlen(p);

	return (len == strlen(value) && strncmp(p, value, len) == 0);
}

LOCAL int
rb_parse_streaming_policy(rb_streaming_policy_t *policy)
{
	char	*p;
	BOOL	wrc_explicit = FALSE;
	BOOL	exact_explicit = FALSE;
	BOOL	restore_explicit = FALSE;

	policy->wrc = WRC_DEF_RC;
	policy->exact = FALSE;
	policy->restore = FALSE;
	policy->advanced = FALSE;

	if (driveropts == NULL)
		return (0);

	p = hasdrvoptx(driveropts, "streamwrc", 0);
	if (p != NULL) {
		wrc_explicit = TRUE;
		if (rb_stream_value_is(p, "default") ||
		    rb_stream_value_is(p, "firmware")) {
			policy->wrc = WRC_DEF_RC;
		} else if (rb_stream_value_is(p, "cav")) {
			policy->wrc = WRC_CAV;
		} else {
			errmsgno(EX_BAD,
			    _("Bad streamwrc value. Use default or cav.\n"));
			return (-1);
		}
	}

	p = hasdrvopt(driveropts, "streamexact");
	if (p != NULL) {
		exact_explicit = TRUE;
		if (rb_stream_value_is(p, "1") ||
		    rb_stream_value_is(p, "on") ||
		    rb_stream_value_is(p, "yes")) {
			policy->exact = TRUE;
		} else if (rb_stream_value_is(p, "0") ||
		    rb_stream_value_is(p, "off") ||
		    rb_stream_value_is(p, "no")) {
			policy->exact = FALSE;
		} else {
			errmsgno(EX_BAD,
			    _("Bad streamexact value. Use streamexact or nostreamexact.\n"));
			return (-1);
		}
	}

	p = hasdrvopt(driveropts, "streamrestore");
	if (p != NULL) {
		restore_explicit = TRUE;
		if (rb_stream_value_is(p, "1") ||
		    rb_stream_value_is(p, "on") ||
		    rb_stream_value_is(p, "yes")) {
			policy->restore = TRUE;
		} else if (rb_stream_value_is(p, "0") ||
		    rb_stream_value_is(p, "off") ||
		    rb_stream_value_is(p, "no")) {
			policy->restore = FALSE;
		} else {
			errmsgno(EX_BAD,
			    _("Bad streamrestore value. Use streamrestore alone.\n"));
			return (-1);
		}
	}

	if (policy->restore && (wrc_explicit || exact_explicit)) {
		errmsgno(EX_BAD,
		    _("streamrestore cannot be combined with streamwrc/streamexact.\n"));
		return (-1);
	}

	/* Silence a compiler warning while retaining explicit-option clarity. */
	(void)restore_explicit;

	policy->advanced = (policy->wrc != WRC_DEF_RC) || policy->exact;
	return (0);
}
LOCAL int
scsi_set_streaming(SCSI *scgp, unsigned long *readp, unsigned long *writep,
	unsigned long *endp, int wrc, BOOL exact, BOOL restore)
{
	register struct scg_cmd	*scmd = scgp->scmd;
	struct mmc_streaming	str;
	struct mmc_streaming	*sp = &str;

	fillbytes((char *)scmd, sizeof (*scmd), '\0');
	scmd->addr = (char *)sp;
	scmd->size = sizeof (*sp);
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g5_cdb.cmd = 0xB6;
	scmd->cdb.g5_cdb.lun = scg_lun(scgp);
	i_to_2_byte(&scmd->cdb.cmd_cdb[9], sizeof (*sp));

	scgp->cmdname = "set streaming";

	fillbytes(sp, sizeof (*sp), '\0');

	/*
	 * RB_RETROBEAM_STREAMING_ADVANCED_30
	 *
	 * MMC SET STREAMING exposes standards-based write rotational control,
	 * exact-speed selection and Restore Unit Defaults.  Keep every bit at
	 * the historical zero/default value unless the caller explicitly asks
	 * for an advanced policy.
	 */
	if (restore) {
		sp->p_rdd = 1;
		if (lverbose)
			printf(_("RetroBeam SET STREAMING: restore unit defaults.\n"));
	} else {
		sp->p_wrc = (unsigned char)wrc;
		sp->p_exact = exact ? 1 : 0;

		if (endp)
			i_to_4_byte(sp->end_lba, *endp);
		else
			i_to_4_byte(sp->end_lba, 0x7FFFFFFF);

		if (readp)
			i_to_4_byte(sp->read_size, *readp);
		else
			i_to_4_byte(sp->read_size, 0x7FFFFFFF);

		if (writep)
			i_to_4_byte(sp->write_size, *writep);
		else
			i_to_4_byte(sp->write_size, 0x7FFFFFFF);

		i_to_4_byte(sp->read_time, 1000);
		i_to_4_byte(sp->write_time, 1000);

		if (lverbose) {
			printf(_("RetroBeam SET STREAMING controls: rotation=%s exact=%s"),
			    wrc == WRC_CAV ? _("CAV") : _("firmware/media default"),
			    exact ? _("yes") : _("no"));
			if (writep)
				printf(_(" write=%lu kB/s"), *writep);
			if (endp)
				printf(_(" end_lba=%lu"), *endp);
			printf("\n");
		}
	}

	/*
	 * RB_RETROBEAM_STREAMING_PAYLOAD_PROOF_31
	 *
	 * Print the exact MMC Performance Descriptor submitted with SET
	 * STREAMING.  The CDB is identical for default/CAV/exact/restore;
	 * these policy controls live in this 28-byte data-out payload.
	 *
	 * This is diagnostic only and does not modify the descriptor.
	 */
	{
		unsigned char	*rbp = (unsigned char *)sp;
		size_t		rbi;

		printf(_("RetroBeam SET STREAMING payload: flags=0x%02X wrc=%u exact=%u restore=%u bytes="),
		    (unsigned int)rbp[0],
		    (unsigned int)sp->p_wrc,
		    sp->p_exact ? 1U : 0U,
		    sp->p_rdd ? 1U : 0U);

		for (rbi = 0; rbi < sizeof (*sp); rbi++)
			printf("%02X", (unsigned int)rbp[rbi]);

		printf("\n");
	}

#ifdef DEBUG
	scg_prbytes(_("Streaming data:"), (unsigned char *)sp, sizeof (*sp));
#endif

	return (scg_cmd(scgp));
}

/*
 * set speed using the streaming descriptors
 */
/*
 * RB_RETROBURNER_MEDIA_SPEED_NEGOTIATION_22
 *
 * Modern DVD write-speed negotiation.
 *
 * MMC GET PERFORMANCE type 03h returns the write-speed descriptors for
 * the currently inserted medium.  A manual speed request is intent, not
 * an arbitrary byte rate to force on the recorder: choose the descriptor
 * whose advertised write speed is closest to the request and program that
 * exact firmware/media value.
 *
 * SET STREAMING is preferred.  SET CD SPEED is the standards-based
 * compatibility fallback when streaming control is unavailable.
 *
 * Automatic mode remains firmware-led because callers that do not request
 * a speed do not enter this manual selection path.
 */
#define RB_MAX_WRITE_SPEED_DESCRIPTORS 100

LOCAL int
rb_select_write_speed_descriptor(SCSI *scgp, unsigned long requested,
                                  unsigned long *selectedp, unsigned long *endp,
                                  int *countp)
{
        struct mmc_performance_header *ph;
        struct mmc_write_speed *wsp;
        char buffer[8 + RB_MAX_WRITE_SPEED_DESCRIPTORS * 16];
        int amt;
        int count;
        int i;
        unsigned long candidate;
        unsigned long candidate_end;
        unsigned long diff;
        unsigned long best_diff = 0;
        unsigned long selected = 0;
        unsigned long selected_end = 0;
        BOOL have_selected = FALSE;

        fillbytes(buffer, sizeof (buffer), '\0');

        /*
         * First fetch one descriptor so the performance header tells us
         * how many descriptors the drive has for this inserted medium.
         */
        if (scsi_get_performance(scgp, buffer,
                                 8 + sizeof (struct mmc_write_speed),
                                 1, 0x03, 0) < 0)
                return (-1);

        ph = (struct mmc_performance_header *)buffer;
        amt = (a_to_4_byte(ph->p_datalen) - 4) /
              sizeof (struct mmc_write_speed);

        if (amt < 1)
                return (-1);
        if (amt > RB_MAX_WRITE_SPEED_DESCRIPTORS)
                amt = RB_MAX_WRITE_SPEED_DESCRIPTORS;

        fillbytes(buffer, sizeof (buffer), '\0');

        if (scsi_get_performance(scgp, buffer,
                                 8 + amt * sizeof (struct mmc_write_speed),
                                 amt, 0x03, 0) < 0)
                return (-1);

        ph = (struct mmc_performance_header *)buffer;
        count = (a_to_4_byte(ph->p_datalen) - 4) /
                sizeof (struct mmc_write_speed);

        if (count > amt)
                count = amt;
        if (count < 1)
                return (-1);

        wsp = (struct mmc_write_speed *)
              (((char *)ph) + sizeof (struct mmc_performance_header));

        /*
         * Match growisofs' useful behavior: pick the first descriptor with
         * the smallest absolute distance from the requested velocity.
         * No invented ASUS/media policy and no arbitrary speed increments.
         */
        for (i = 0; i < count; i++, wsp++) {
                candidate = a_to_u_4_byte(wsp->write_speed);
                candidate_end = a_to_u_4_byte(wsp->end_lba);

                if (candidate == 0)
                        continue;

                if (candidate >= requested)
                        diff = candidate - requested;
                else
                        diff = requested - candidate;

                if (!have_selected || diff < best_diff) {
                        selected = candidate;
                        selected_end = candidate_end;
                        best_diff = diff;
                        have_selected = TRUE;
                }
        }

        if (!have_selected)
                return (-1);

        if (selectedp)
                *selectedp = selected;
        if (endp)
                *endp = selected_end;
        if (countp)
                *countp = count;

        return (0);
}

/*
 * Set manual MMC DVD speed using media-advertised descriptors.
 */
EXPORT int
speed_select_mdvd(SCSI *scgp, int readspeed, int writespeed)
{
	unsigned long requested;
	unsigned long selected = 0;
	unsigned long end_lba = 0x7FFFFFFF;
	unsigned long wspeed;
	int descriptor_count = 0;
	int ret;
	rb_streaming_policy_t policy;

	if (rb_parse_streaming_policy(&policy) < 0)
		return (-1);

	/*
	 * Restore Unit Defaults is an explicit SET STREAMING operation and does
	 * not need a speed descriptor. It is intended for -setdropts/GUI reset.
	 */
	if (policy.restore) {
		if (scsi_set_streaming(scgp, (unsigned long *)NULL,
		    (unsigned long *)NULL, (unsigned long *)NULL,
		    WRC_DEF_RC, FALSE, TRUE) >= 0) {
			printf(_("RetroBeam streaming defaults restored by drive.\n"));
			return (0);
		}
		errmsgno(EX_BAD,
		    _("Drive rejected SET STREAMING Restore Unit Defaults.\n"));
		return (-1);
	}

	/*
	 * A non-positive request means "leave it to the firmware".
	 */
	if (writespeed <= 0)
		return (0);

	requested = (unsigned long)writespeed;

	if (rb_select_write_speed_descriptor(scgp, requested,
	    &selected, &end_lba, &descriptor_count) >= 0) {
		printf("RetroBeam write speed: requested %lu kB/s (~%lux), "
		       "media selected %lu kB/s (~%lux), %d descriptor%s.\n",
		       requested, (requested + 692) / 1385,
		       selected, (selected + 692) / 1385,
		       descriptor_count,
		       descriptor_count == 1 ? "" : "s");

		printf(_("RetroBeam streaming policy: rotation=%s exact=%s.\n"),
		    policy.wrc == WRC_CAV ? _("CAV") : _("firmware/media default"),
		    policy.exact ? _("yes") : _("no"));

		wspeed = selected;
		if (scsi_set_streaming(scgp, (unsigned long *)NULL,
		    &wspeed, &end_lba, policy.wrc, policy.exact, FALSE) >= 0) {
			printf("RetroBeam write speed method: SET STREAMING (0xB6).\n");
			return (0);
		}

		/*
		 * Never silently discard an explicit advanced request. SET CD SPEED
		 * cannot preserve SET STREAMING's exact-speed/CAV semantics.
		 */
		if (policy.advanced) {
			errmsgno(EX_BAD,
			    _("Drive rejected explicit RetroBeam streaming policy; no fallback attempted.\n"));
			return (-1);
		}

		/*
		 * Historical/default compatibility fallback remains unchanged.
		 */
		if (selected <= 0xFFFFUL) {
			scgp->silent++;
			ret = scsi_set_speed(scgp, readspeed,
			    (int)selected, ROTCTL_CLV);
			scgp->silent--;

			if (ret >= 0) {
				printf("RetroBeam write speed method: "
				       "SET CD SPEED (0xBB) fallback.\n");
				return (0);
			}
		}

		return (-1);
	}

	/*
	 * If an advanced policy was requested, descriptors are required: without
	 * them RetroBeam cannot truthfully claim an exact/CAV media configuration.
	 */
	if (policy.advanced) {
		errmsgno(EX_BAD,
		    _("Current media exposes no usable write-speed descriptor for the requested advanced streaming policy.\n"));
		return (-1);
	}

	/*
	 * Older MMC compatibility path for the default policy only.
	 */
	if (requested <= 0xFFFFUL) {
		scgp->silent++;
		ret = scsi_set_speed(scgp, readspeed,
		    (int)requested, ROTCTL_CLV);
		scgp->silent--;

		if (ret >= 0) {
			printf("RetroBeam write speed: media descriptors unavailable; "
			       "SET CD SPEED (0xBB) accepted %lu kB/s.\n",
			       requested);
			return (0);
		}
	}

	return (-1);
}


LOCAL char *
fname(code)
	unsigned int	code;
{
	uint16_t	i;

	for (i = 0; i < sizeof (fl) / sizeof (fl[0]); i++) {
		if (code == fl[i].code)
			return (fl[i].name);
	}
	return ("Unknown");
}

LOCAL char *
pname(code)
	unsigned int	code;
{
	uint16_t	i;

	for (i = 0; i < sizeof (pl) / sizeof (pl[0]); i++) {
		if (code == pl[i].code)
			return (pl[i].name);
	}
	return ("Unknown");
}

LOCAL BOOL
fname_known(code)
	unsigned int	code;
{
	uint16_t	i;

	for (i = 0; i < sizeof (fl) / sizeof (fl[0]); i++) {
		if (code == fl[i].code)
			return (TRUE);
	}
	return (FALSE);
}

LOCAL BOOL
pname_known(code)
	unsigned int	code;
{
	uint16_t	i;

	for (i = 0; i < sizeof (pl) / sizeof (pl[0]); i++) {
		if (code == pl[i].code)
			return (TRUE);
	}
	return (FALSE);
}

EXPORT int
print_features(scgp)
	SCSI	*scgp;
{
	unsigned char	fbuf[32 * 1024];
	unsigned char	*p;
	unsigned char	*pend;
	int	amt;
	int	flen;
	int	feature;
	int	i;

	flen = get_conflen(scgp, 0, 0);
	if (flen < 0)
		return (-1);
	if (sizeof (fbuf) < flen)
		flen = sizeof (fbuf);

	fillbytes(fbuf, sizeof (fbuf), '\0');
	scgp->silent++;
	i = get_configuration(scgp, (char *)fbuf, sizeof (fbuf), 0, 0);
	scgp->silent--;
	if (i < 0)
		return (-1);
	amt = sizeof (fbuf) - scg_getresid(scgp);

	p = fbuf;
	pend = &p[sizeof (fbuf) - scg_getresid(scgp)];
	flen = a_to_u_4_byte(p);
	if ((flen+4) < amt)
		amt = flen+4;
	pend = &p[amt];
	if (xdebug > 1)
		scg_prbytes(_("Features: "), fbuf, amt);

	feature = a_to_u_2_byte(&p[6]);
	if (xdebug > 0)
		printf(_("feature len: %d current profile 0x%04X len %lld\n"),
				flen, feature, (int64_t)(pend - p));

	p = fbuf + 8;	/* Skip feature header	*/
	while (p < pend) {
		int	col;

		col = 0;
		feature = a_to_u_2_byte(p);
		if (xdebug > 0)
			col += printf(_("Feature: 0x%04X "), feature);
		else
			col += printf(_("Feature: "));
		if (fname_known(feature))
			col += printf("'%s' ", fname(feature));
		else
			col += printf("0x%04X ", feature);
		col += printf("%s %s",
			p[2] & 1 ? _("(current)"):"",
			p[2] & 2 ? _("(persistent)"):"");

		if (feature == 0x108)
			col += printf(_("	Serial: '%.*s'"), p[3], &p[4]);
		if (xdebug > 1 && p[3]) {
			if (col < 50)
				printf("%*s", 50-col, "");
			scg_fprbytes(stdout, _(" Data: "), &p[4], p[3]);
		} else {
			printf("\n");
		}
		p += p[3];
		p += 4;
	}
	return (0);
}

LOCAL char *fdt[] = {
	"Reserved (0)",
	"Unformated or Blank Media",
	"Formatted Media",
	"No Media Present or Unknown Capacity"
};

EXPORT void
print_format_capacities(scgp)
	SCSI	*scgp;
{
	unsigned char	b[1024];
	int	i;
	unsigned char	*p;

	fillbytes(b, sizeof (b), '\0');
	scgp->silent++;
	i = read_format_capacities(scgp, (char *)b, sizeof (b));
	scgp->silent--;
	if (i < 0)
		return;

	i = b[3] + 4;
	fillbytes(b, sizeof (b), '\0');
	if (read_format_capacities(scgp, (char *)b, i) < 0)
		return;

	if (xdebug > 0) {
		i = b[3] + 4;
		scg_prbytes(_("Format cap: "), b, i);
	}
	i = b[3];
	if (i > 0) {
		int	cnt;
		uint32_t n1;
		uint32_t n2;
		printf(_("\n    Capacity  Blklen/Sparesz.  Format-type  Type\n"));
		for (p = &b[4]; i > 0; i -= 8, p += 8) {
			cnt = 0;
			n1 = a_to_u_4_byte(p);
			n2 = a_to_u_3_byte(&p[5]);
			printf("%12lu %16lu         0x%2.2X  %s\n",
				(unsigned long)n1, (unsigned long)n2,
				(p[4] >> 2) & 0x3F,
				fdt[p[4] & 0x03]);
		}
	}
}

EXPORT int
get_format_capacities(scgp, bp, cnt)
	SCSI	*scgp;
	char *	bp;
	int	cnt;
{
	int			len = sizeof (struct scsi_format_cap_header);
	struct scsi_format_cap_header	*hp;

	fillbytes(bp, cnt, '\0');
	if (cnt < len)
		return (-1);
	scgp->silent++;
	if (read_format_capacities(scgp, bp, len) < 0) {
		scgp->silent--;
		return (-1);
	}
	scgp->silent--;

	if (scg_getresid(scgp) > 0)
		return (-1);

	hp = (struct scsi_format_cap_header *)bp;
	len = hp->len;
	len += sizeof (struct scsi_format_cap_header);
	while (len > cnt)
		len -= sizeof (struct scsi_format_cap_desc);

	if (read_format_capacities(scgp, bp, len) < 0)
		return (-1);

	len -= scg_getresid(scgp);
	return (len);
}

EXPORT int
read_format_capacities(scgp, bp, cnt)
	SCSI	*scgp;
	char *	bp;
	int	cnt;
{
	register struct	scg_cmd	*scmd = scgp->scmd;

	fillbytes((char *)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0x23;
	scmd->cdb.g1_cdb.lun = scg_lun(scgp);
	g1_cdblen(&scmd->cdb.g1_cdb, cnt);

	scgp->cmdname = "read_format_capacities";

	return (scg_cmd(scgp));
}

EXPORT void
przone(rp)
	struct rzone_info *rp;
{
	int	rsize = a_to_2_byte(rp->data_len)+2;

	if (rsize < 12)
		return;
	printf(_("rzone size:         %d\n"), rsize);
	printf(_("rzone number:       %d\n"), rp->rzone_num_msb * 256 + rp->rzone_num_lsb);
	printf(_("border number:      %d\n"), rp->border_num_msb * 256 + rp->border_num_lsb);
	printf(_("ljrs:               %d\n"), rp->ljrs);
	printf(_("track mode:         %d copy: %d\n"), rp->trackmode, rp->copy);
	printf(_("damage:             %d\n"), rp->damage);
	printf(_("reserved track:     %d blank: %d incremental: %d fp: %d\n"),
						rp->rt, rp->blank,
						rp->incremental, rp->fp);
	printf(_("data mode:          %d\n"), rp->datamode);
	printf(_("lra valid:          %d\n"), rp->lra_v);
	printf(_("nwa valid:          %d\n"), rp->nwa_v);
	printf(_("rzone start:        %ld\n"), a_to_4_byte(rp->rzone_start));
	printf(_("next wr addr:       %ld\n"), a_to_4_byte(rp->next_recordable_addr));
	printf(_("free blocks:        %ld\n"), a_to_4_byte(rp->free_blocks));
	printf(_("blocking factor:    %ld\n"), a_to_4_byte(rp->block_factor));
	printf(_("rzone size:         %ld\n"), a_to_4_byte(rp->rzone_size));
	printf(_("last recorded addr: %ld\n"), a_to_4_byte(rp->last_recorded_addr));
	if (rsize < 40)
		return;
	printf(_("read compat lba:    %ld\n"), a_to_4_byte(rp->read_compat_lba));
	if (rsize < 44)
		return;
	printf(_("next layerjmp addr: %ld\n"), a_to_4_byte(rp->next_layer_jump));
	if (rsize < 48)
		return;
	printf(_("last layerjmp addr: %ld\n"), a_to_4_byte(rp->last_layer_jump));
}

EXPORT int
get_diskinfo(scgp, dip, cnt)
	SCSI		*scgp;
	struct disk_info *dip;
	int		cnt;
{
	int	len;
	int	ret;

	fillbytes((char *)dip, cnt, '\0');

	/*
	 * Used to be 2 instead of 4 (now). But some Y2k ATAPI drives as used
	 * by IOMEGA create a DMA overrun if we try to transfer only 2 bytes.
	 */
	if (read_disk_info(scgp, (char *)dip, 4) < 0)
		return (-1);
	len = a_to_u_2_byte(dip->data_len);
	len += 2;
	if (len > cnt)
		len = cnt;
	ret = read_disk_info(scgp, (char *)dip, len);

#ifdef	DEBUG
	if (lverbose > 1)
		scg_prbytes(_("Disk info:"), (unsigned char *)dip,
				len-scg_getresid(scgp));
#endif
	return (ret);
}

#define	IS(what, flag)		printf(_("Disk Is %s%s\n"), flag?"":_("not "), what);

LOCAL	char	res[] = "reserved";

EXPORT char *
get_ses_type(ses_type)
	int	ses_type;
{
static	char	ret[16];

	switch (ses_type) {

	case SES_DA_ROM:	return ("CD-DA or CD-ROM");
	case SES_CDI:		return ("CDI");
	case SES_XA:		return ("CD-ROM XA");
	case SES_UNDEF:		return ("undefined");
	default:
				js_snprintf(ret, sizeof (ret), "%s: 0x%2.2X",
							res, ses_type);
				return (ret);
	}
}

EXPORT void
print_diskinfo(dip, is_cd)
	struct disk_info	*dip;
	BOOL			is_cd;
{
static	char *dt_name[] = { "standard", "track resources", "POW resources", res, res, res, res, res };
static	char *ds_name[] = { "empty", "incomplete/appendable", "complete", "illegal" };
static	char *ss_name[] = { "empty", "incomplete/appendable", "illegal", "complete", };
static	char *fd_name[] = { "none", "incomplete", "in progress", "completed", };

	IS(_("erasable"), dip->erasable);
	printf(_("data type:                %s\n"), dt_name[dip->dtype]);
	printf(_("disk status:              %s\n"), ds_name[dip->disk_status]);
	printf(_("session status:           %s\n"), ss_name[dip->sess_status]);
	printf(_("BG format status:         %s\n"), fd_name[dip->bg_format_stat]);
	printf(_("first track:              %d\n"),
		dip->first_track);
	printf(_("number of sessions:       %d\n"),
		dip->numsess + dip->numsess_msb * 256);
	printf(_("first track in last sess: %d\n"),
		dip->first_track_ls + dip->first_track_ls_msb * 256);
	printf(_("last track in last sess:  %d\n"),
		dip->last_track_ls + dip->last_track_ls_msb * 256);
	IS(_("unrestricted"), dip->uru);
	printf(_("Disk type: "));
	if (is_cd) {
		printf("%s", get_ses_type(dip->disk_type));
	} else {
		printf(_("DVD, HD-DVD or BD"));
	}
	printf("\n");
	if (dip->did_v)
		printf(_("Disk id: 0x%lX\n"), a_to_u_4_byte(dip->disk_id));

	if (is_cd) {
		printf(_("last start of lead in: %ld\n"),
			msf_to_lba(dip->last_lead_in[1],
			dip->last_lead_in[2],
			dip->last_lead_in[3], FALSE));
		printf(_("last start of lead out: %ld\n"),
			msf_to_lba(dip->last_lead_out[1],
			dip->last_lead_out[2],
			dip->last_lead_out[3], TRUE));
	}

	if (dip->dbc_v)
		printf(_("Disk bar code: 0x%lX%lX\n"),
			a_to_u_4_byte(dip->disk_barcode),
			a_to_u_4_byte(&dip->disk_barcode[4]));

	if (dip->dac_v)
		printf(_("Disk appl. code: %d\n"), dip->disk_appl_code);

	if (dip->num_opc_entries > 0) {
		printf(_("OPC table:\n"));
	}
}

EXPORT int
prdiskstatus(scgp, dp, is_cd)
	SCSI	*scgp;
	cdr_t	*dp;
	BOOL	is_cd;
{
	struct disk_info	di;
	struct rzone_info	rz;
	int			sessions;
	int			track;
	int			tracks;
	int			t;
	int			s;
	long			raddr;
	long			lastaddr = -1;
	long			lastsess = -1;
	long			leadout = -1;
	long			lo_sess = 0;
	long			nwa = -1;
	long			rsize = -1;
	long			border_size = -1;
	int			profile;

	profile = get_curprofile(scgp);
	if (profile > 0) {
		int mt = get_mediatype(scgp);

		printf(_("Mounted media class:      %s\n"),
				get_mclassname(mt));
		if (pname_known(profile)) {
			printf(_("Mounted media type:       %s\n"),
				pname(profile));
		}
	}
	get_diskinfo(scgp, &di, sizeof (di));
	print_diskinfo(&di, is_cd);

	sessions = di.numsess + di.numsess_msb * 256;
	tracks = di.last_track_ls + di.last_track_ls_msb * 256;

	printf(_("\nTrack  Sess Type   Start Addr End Addr   Size\n"));
	printf(_("==============================================\n"));
	fillbytes((char *)&rz, sizeof (rz), '\0');
	for (t = di.first_track; t <= tracks; t++) {
		fillbytes((char *)&rz, sizeof (rz), '\0');
		get_trackinfo(scgp, (char *)&rz, TI_TYPE_TRACK, t, sizeof (rz));
		if (lverbose > 1)
			przone(&rz);
		track = rz.rzone_num_lsb + rz.rzone_num_msb * 256;
		s = rz.border_num_lsb + rz.border_num_msb * 256;
		raddr = a_to_4_byte(rz.rzone_start);
		if (rsize >= 0)
			border_size = raddr - (lastaddr+rsize);
		if (!rz.blank && s > lastsess) { /* First track in last sess ? */
			lastaddr = raddr;
			lastsess = s;
		}
		nwa = a_to_4_byte(rz.next_recordable_addr);
		rsize = a_to_4_byte(rz.rzone_size);
		if (!rz.blank) {
			leadout = raddr + rsize;
			lo_sess = s;
		}
		printf("%5d %5d %-6s %-10ld %-10ld %ld",
			track, s,
			rz.blank ? _("Blank") :
				rz.trackmode & 4 ? _("Data") : _("Audio"),
			raddr, raddr + rsize -1, rsize);
		if (lverbose > 0)
			printf(" %10ld", border_size);
		printf("\n");
	}
	printf("\n");
	if (lastaddr >= 0)
		printf(_("Last session start address:         %ld\n"), lastaddr);
	if (leadout >= 0)
		printf(_("Last session leadout start address: %ld\n"), leadout);
	if (rz.nwa_v) {
		printf(_("Next writable address:              %ld\n"), nwa);
		printf(_("Remaining writable size:            %ld\n"), rsize);
	}

	return (0);
}

EXPORT int
sessstatus(scgp, is_cd, offp, nwap)
	SCSI	*scgp;
	BOOL	is_cd;
	long	*offp;
	long	*nwap;
{
	struct disk_info	di;
	struct rzone_info	rz;
	int			sessions;
	int			track;
	int			tracks;
	int			t;
	int			s;
	long			raddr;
	long			lastaddr = -1;
	long			lastsess = -1;
	long			leadout = -1;
	long			lo_sess = 0;
	long			nwa = -1;
	long			rsize = -1;
	long			border_size = -1;


	if (get_diskinfo(scgp, &di, sizeof (di)) < 0)
		return (-1);

	sessions = di.numsess + di.numsess_msb * 256;
	tracks = di.last_track_ls + di.last_track_ls_msb * 256;

	fillbytes((char *)&rz, sizeof (rz), '\0');
	for (t = di.first_track; t <= tracks; t++) {
		fillbytes((char *)&rz, sizeof (rz), '\0');
		if (get_trackinfo(scgp, (char *)&rz, TI_TYPE_TRACK, t, sizeof (rz)) < 0)
			return (-1);
		track = rz.rzone_num_lsb + rz.rzone_num_msb * 256;
		s = rz.border_num_lsb + rz.border_num_msb * 256;
		raddr = a_to_4_byte(rz.rzone_start);
		if (rsize >= 0)
			border_size = raddr - (lastaddr+rsize);
		if (!rz.blank && s > lastsess) { /* First track in last sess ? */
			lastaddr = raddr;
			lastsess = s;
		}
		nwa = a_to_4_byte(rz.next_recordable_addr);
		rsize = a_to_4_byte(rz.rzone_size);
		if (!rz.blank) {
			leadout = raddr + rsize;
			lo_sess = s;
		}
	}
	if (lastaddr >= 0 && offp != NULL)
		*offp = lastaddr;

	if (rz.nwa_v && nwap != NULL)
		*nwap = nwa;

	return (0);
}

EXPORT void
print_performance_mmc(scgp)
	SCSI	*scgp;
{
	unsigned long	reads;
	unsigned long	writes;
	unsigned long	ends = 0x7FFFFFFF;
	int	oxdebug = xdebug;

	/*
	 * Do not try to fail with old drives...
	 */
	if (get_curprofile(scgp) < 0)
		return;

	if (xdebug == 0)
		xdebug = -1;

	printf(_("\nCurrent performance according to MMC get performance:\n"));
	scsi_get_perf_curspeed(scgp, &reads, &writes, &ends);

	printf(_("\nMaximum performance according to MMC get performance:\n"));
	scsi_get_perf_maxspeed(scgp, &reads, &writes, &ends);

	xdebug = oxdebug;
}
