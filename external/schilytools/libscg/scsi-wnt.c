/* @(#)scsi-wnt.c	1.51 17/08/01 Copyright 1998-2017 J. Schilling, A.L. Faber, J.A. Key */
#ifndef lint
static	char __sccsid[] =
	"@(#)scsi-wnt.c	1.51 17/08/01 Copyright 1998-2017 J. Schilling, A.L. Faber, J.A. Key";
#endif
/*
 *	Interface for the Win32 ASPI library.
 *		You need wnaspi32.dll and aspi32.sys
 *		Both can be installed from ASPI_ME
 *
 *	Warning: you may change this source, but if you do that
 *	you need to change the _scg_version and _scg_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 *
 *	Copyright (c) 1998-2017 J. Schilling
 *	Copyright (c) 1999 A.L. Faber for the first implementation
 *			   of this interface.
 *	TODO:
 *	-	DMA resid handling
 *	-	better handling of maxDMA
 *	-	SCSI reset support
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


/*
 * Include for Win32 ASPI AspiRouter
 *
 * NOTE: aspi-win32.h includes Windows.h and Windows.h includes
 *	 Base.h which has a second typedef for BOOL.
 *	 We define BOOL to make all local code use BOOL
 *	 from Windows.h and use the hidden __SBOOL for
 *	 our global interfaces.
 *
 *	 These workarounds are now applied in schily/windows.h
 */
#include <schily/windows.h>
#include <winioctl.h>
#include <scg/aspi-win32.h>
#include <scg/spti-wnt.h>

#if	defined(__CYGWIN32__) || defined(__CYGWIN__)	/* Use dlopen()	*/
#include <dlfcn.h>
#endif

/*
 *	Warning: you may change this source, but if you do that
 *	you need to change the _scg_version and _scg_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 */
LOCAL	char	_scg_trans_version[] = "retroburner-scsi-wnt-0.4";	/* The version for this transport*/
LOCAL	char	_scg_itrans_version[] = "RetroBurner-SPTI-0.4";	/* The version for SPTI */

/*
 * Local defines and constants
 */
/*#define DEBUG_WNTASPI*/

#define	MAX_SCG		64	/* Max # of SCSI controllers	*/
#define	MAX_TGT		16	/* Max # of SCSI Targets	*/
#define	MAX_LUN		8	/* Max # of SCSI LUNs		*/

#ifdef DEBUG_WNTASPI
#endif

struct scg_local {
	int	dummy;
};
#define	scglocal(p)	((struct scg_local *)((p)->local))

/*
 * Local variables
 */
LOCAL	int	busses;
LOCAL	DWORD	(*pfnGetASPI32SupportInfo)(void)		= NULL;
LOCAL	DWORD	(*pfnSendASPI32Command)(LPSRB)			= NULL;
LOCAL	BOOL	(*pfnGetASPI32Buffer)(PASPI32BUFF)		= NULL;
LOCAL	BOOL	(*pfnFreeASPI32Buffer)(PASPI32BUFF)		= NULL;
LOCAL	BOOL	(*pfnTranslateASPI32Address)(PDWORD, PDWORD)	= NULL;

LOCAL	int	AspiLoaded			= 0;    /* ASPI or SPTI */
LOCAL	HANDLE	hAspiLib			= NULL;	/* Used for Loadlib */

#define	MAX_DMA_WNT	(63L*1024L) /* Preserve legacy ASPI limit. */

/*
 * RB_RETROBURNER_SPTI_64K_03
 *
 * Native Windows SPTI supports a full 64 KiB payload on the tested modern
 * USB optical path. Keep the old 63 KiB ASPI compatibility ceiling separate.
 */
#define	MAX_DMA_SPTI	(64L*1024L)

/*
 * Local function prototypes
 */
LOCAL	void	exit_func	__PR((void));
#ifdef DEBUG_WNTASPI
LOCAL	void	DebugScsiSend	__PR((SCSI *scgp, SRB_ExecSCSICmd *s, int bDisplayBuffer));
#endif
LOCAL	void	copy_sensedata	__PR((SRB_ExecSCSICmd *cp, struct scg_cmd *sp));
LOCAL	void	set_error	__PR((SRB_ExecSCSICmd *cp, struct scg_cmd *sp));
LOCAL	BOOL	open_driver	__PR((SCSI *scgp));
LOCAL	BOOL	load_aspi	__PR((SCSI *scgp));
LOCAL	BOOL	close_driver	__PR((void));
LOCAL	int	ha_inquiry	__PR((SCSI *scgp, int id, SRB_HAInquiry	*ip));
#ifdef	__USED__
LOCAL	int	resetSCSIBus	__PR((SCSI *scgp));
#endif
LOCAL	int	scsiabort	__PR((SCSI *scgp, SRB_ExecSCSICmd *sp));


/* SPTI Start ---------------------------------------------------------------*/
/*
 * From scsipt.c - Copyright (C) 1999 Jay A. Key
 * Homepage: http://akrip.sourceforge.net/
 * Native NT support functions via the SCSI Pass Through interface instead
 * of ASPI.  Although based on information from the NT 4.0 DDK from
 * Microsoft, the information has been sufficiently distilled to allow
 * compilation w/o having the DDK installed.
 * added to scsi-wnt.c by Richard Stemmer, rs@epost.de
 * See http://www.ste-home.de/cdrtools-spti/
 */

#define	PREFER_SPTI	1		/* Prefer SPTI if available, else try ASPI, force ASPI with dev=ASPI: */
/* #define	CREATE_NONSHARED 1 */	/* open CDROM-Device not SHARED if possible */
/* #define	_DEBUG_SCSIPT 1   */
#ifdef _DEBUG_SCSIPT
FILE *scgp_errfile; /* File for SPTI-Debug-Messages */
#endif

#define	SENSE_LEN_SPTI		32	/* Sense length for ASPI is only 14 */
#define	NUM_MAX_NTSCSI_DRIVES	26	/* a: ... z:			*/
#define	NUM_FLOPPY_DRIVES	2
#define	NUM_MAX_NTSCSI_HA	NUM_MAX_NTSCSI_DRIVES

#define	NTSCSI_HA_INQUIRY_SIZE	36

#define	SCSI_CMD_INQUIRY	0x12

typedef struct {
	BYTE	ha;			/* SCSI Bus #			*/
	BYTE	tgt;			/* SCSI Target #		*/
	BYTE	lun;			/* SCSI Lun #			*/
	BYTE	PortNumber;		/* SCSI Card # (\\.\SCSI%d)	*/
	BYTE	PathId;			/* SCSI Bus/Channel # on card n	*/
	BYTE	driveLetter;		/* Win32 drive letter (e.g. c:)	*/
	BOOL	bUsed;			/* Win32 drive letter is used	*/
	HANDLE	hDevice;		/* Win32 handle for ioctl()	*/
	BYTE	inqData[NTSCSI_HA_INQUIRY_SIZE];
} DRIVE;

typedef struct {
	BYTE	numAdapters;
	DRIVE	drive[NUM_MAX_NTSCSI_DRIVES];
} SPTIGLOBAL;

LOCAL	int	InitSCSIPT(void);
LOCAL	int	DeinitSCSIPT(void);
LOCAL	void	GetDriveInformation(BYTE i, DRIVE *pDrive);
LOCAL	BYTE	SPTIGetNumAdapters(void);
LOCAL	BYTE	SPTIGetDeviceIndex(BYTE ha, BYTE tgt, BYTE lun);
LOCAL	DWORD	SPTIHandleHaInquiry(LPSRB_HAInquiry lpsrb);
LOCAL	DWORD	SPTIExecSCSICommand(LPSRB_ExecSCSICmd lpsrb, int sptTimeOutValue, BOOL bBeenHereBefore);
LOCAL	HANDLE	GetFileHandle(BYTE i, BOOL openshared);

/*
 * RB_RETROBURNER_USB_ENUM_01
 *
 * Windows 8 and later may report IOCTL_SCSI_GET_ADDRESS as successful for
 * some USB optical devices while returning an unusable/zeroed address.
 * Query the storage bus type through the supported storage-property API
 * before relying on that legacy SCSI-address request.
 *
 * This RetroBurner implementation was written independently from public
 * cdrtfe behaviour/documentation and Microsoft IOCTL documentation.
 */
LOCAL BOOL
rb_get_storage_bus_type(HANDLE fh, STORAGE_BUS_TYPE *busType)
{
	STORAGE_PROPERTY_QUERY	query;
	BYTE			descriptorBuffer[1024];
	PSTORAGE_DEVICE_DESCRIPTOR descriptor;
	DWORD			returned = 0;

	if (busType == NULL)
		return (FALSE);

	*busType = BusTypeUnknown;
	memset(&query, 0, sizeof (query));
	memset(descriptorBuffer, 0, sizeof (descriptorBuffer));

	query.PropertyId = StorageDeviceProperty;
	query.QueryType = PropertyStandardQuery;

	if (!DeviceIoControl(fh,
			    IOCTL_STORAGE_QUERY_PROPERTY,
			    &query,
			    sizeof (query),
			    descriptorBuffer,
			    sizeof (descriptorBuffer),
			    &returned,
			    NULL)) {
		return (FALSE);
	}

	if (returned < offsetof(STORAGE_DEVICE_DESCRIPTOR, RawPropertiesLength))
		return (FALSE);

	descriptor = (PSTORAGE_DEVICE_DESCRIPTOR)descriptorBuffer;
	*busType = descriptor->BusType;
	return (TRUE);
}

LOCAL void
rb_use_drive_letter_address(BYTE i, DRIVE *pDrive)
{
	pDrive->bUsed		= TRUE;
	pDrive->ha		= i;
	pDrive->PortNumber	= i + 64;
	pDrive->PathId		= 0;
	pDrive->tgt		= 0;
	pDrive->lun		= 0;
	pDrive->driveLetter	= i;
	pDrive->hDevice		= INVALID_HANDLE_VALUE;
}

LOCAL	BOOL	bSCSIPTInit = FALSE;
LOCAL	SPTIGLOBAL sptiglobal;
LOCAL	BOOL	bUsingSCSIPT = FALSE;
LOCAL	BOOL	bForceAccess = FALSE;
LOCAL	int	sptihamax;
LOCAL	USHORT	sptihasortarr[NUM_MAX_NTSCSI_HA];

/*
 * RB_RETROBURNER_SPTI_TRACE_02
 *
 * Opt-in RetroBurner SPTI diagnostics.
 *
 * Set RETROBURNER_SPTI_TRACE to an absolute log file path to enable.
 * No payload/user data is logged. Successful high-frequency WRITE commands
 * are sampled; failed commands are logged in full.
 */
#define	RB_TRACE_WRITE_INTERVAL	2048UL

LOCAL	HANDLE	rbTraceHandle = INVALID_HANDLE_VALUE;
LOCAL	BOOL	rbTraceConfigured = FALSE;
LOCAL	ULONG	rbTraceCmdCount = 0;
LOCAL	ULONG	rbTraceWriteCount = 0;
/*
 * RB_RETROBURNER_SPTI_FAKE_BURN_05
 *
 * Development-only fake media-write transport for black-box comparison.
 *
 * Enabled only when:
 *   RETROBURNER_SPTI_FAKE_BURN=RETROBURNER-FAKE-WRITE
 *
 * Read/query SPTI commands continue to the real drive. Any SCSI DATA OUT
 * request, plus known no-data media/drive-mutating MMC commands, is completed
 * locally with SS_COMP and never reaches DeviceIoControl().
 *
 * No user payload data is written to the trace.
 */
LOCAL	BOOL	rbFakeConfigured = FALSE;
LOCAL	BOOL	rbFakeEnabled = FALSE;
LOCAL	ULONGLONG rbFakeWriteCount = 0;
LOCAL	ULONGLONG rbFakeWriteBytes = 0;
LOCAL	ULONG	rbFakeFirstLba = 0xFFFFFFFFUL;
LOCAL	ULONG	rbFakeLastEndLba = 0;
LOCAL	ULONG	rbFakeFirstBlocks = 0;
LOCAL	ULONG	rbFakeLastBlocks = 0;
LOCAL	ULONG	rbFakeMaxBlocks = 0;

LOCAL BOOL
rb_fake_enabled()
{
	char	value[64];
	DWORD	n;

	if (rbFakeConfigured)
		return (rbFakeEnabled);

	rbFakeConfigured = TRUE;
	value[0] = '\0';
	n = GetEnvironmentVariableA("RETROBURNER_SPTI_FAKE_BURN",
				    value, sizeof (value));

	if (n > 0 && n < sizeof (value) &&
	    strcmp(value, "RETROBURNER-FAKE-WRITE") == 0)
		rbFakeEnabled = TRUE;

	return (rbFakeEnabled);
}

LOCAL BOOL
rb_fake_mutating_opcode(op)
	BYTE	op;
{
	switch (op) {
	case 0x04:	/* FORMAT UNIT */
	case 0x0A:	/* WRITE(6) */
	case 0x15:	/* MODE SELECT(6) */
	case 0x1B:	/* START STOP UNIT */
	case 0x1E:	/* PREVENT/ALLOW */
	case 0x2A:	/* WRITE(10) */
	case 0x2C:	/* ERASE(10) */
	case 0x2E:	/* WRITE AND VERIFY(10) */
	case 0x35:	/* SYNCHRONIZE CACHE */
	case 0x3B:	/* WRITE BUFFER */
	case 0x53:	/* RESERVE TRACK */
	case 0x54:	/* SEND OPC INFORMATION */
	case 0x55:	/* MODE SELECT(10) */
	case 0x5B:	/* CLOSE TRACK/SESSION */
	case 0x5D:	/* SEND CUE SHEET */
	case 0xA1:	/* BLANK */
	case 0xA3:	/* SEND KEY */
	case 0xA6:	/* LOAD/UNLOAD */
	case 0xAA:	/* WRITE(12) */
	case 0xAE:	/* WRITE AND VERIFY(12) */
	case 0xB6:	/* SET STREAMING */
	case 0xBB:	/* SET CD SPEED */
	case 0xBF:	/* SEND DVD STRUCTURE */
		return (TRUE);
	default:
		return (FALSE);
	}
}

LOCAL void
rb_trace_write(text)
	const char	*text;
{
	DWORD	written;

	if (rbTraceHandle == INVALID_HANDLE_VALUE || text == NULL)
		return;

	SetFilePointer(rbTraceHandle, 0, NULL, FILE_END);
	WriteFile(rbTraceHandle, text, (DWORD)strlen(text), &written, NULL);
}

LOCAL void
rb_trace_init()
{
	char	path[2048];
	char	line[256];
	DWORD	n;

	if (rbTraceConfigured)
		return;

	rbTraceConfigured = TRUE;
	path[0] = '\0';
	n = GetEnvironmentVariableA("RETROBURNER_SPTI_TRACE",
				    path, sizeof (path));

	if (n == 0 || n >= sizeof (path))
		return;
	if (path[0] == '0' && path[1] == '\0')
		return;

	rbTraceHandle = CreateFileA(path,
				    GENERIC_WRITE,
				    FILE_SHARE_READ | FILE_SHARE_WRITE,
				    NULL,
				    OPEN_ALWAYS,
				    FILE_ATTRIBUTE_NORMAL,
				    NULL);

	if (rbTraceHandle == INVALID_HANDLE_VALUE)
		return;

	js_snprintf(line, sizeof (line),
	    "[RBSPTI] BEGIN version=0.2 pid=%lu write_interval=%lu\r\n",
	    (unsigned long)GetCurrentProcessId(),
	    (unsigned long)RB_TRACE_WRITE_INTERVAL);
	rb_trace_write(line);

	if (rb_fake_enabled())
		rb_trace_write("[RBSPTI] FAKE_BURN_ARMED token_ok=1 hardware_mutation_path=blocked\r\n");
}

LOCAL BOOL
rb_trace_enabled()
{
	rb_trace_init();
	return (rbTraceHandle != INVALID_HANDLE_VALUE);
}

LOCAL void
rb_trace_close()
{
	char	line[512];
	ULONG	firstLba;

	if (rbTraceHandle == INVALID_HANDLE_VALUE)
		return;

	if (rb_fake_enabled()) {
		firstLba = rbFakeFirstLba;
		if (firstLba == 0xFFFFFFFFUL)
			firstLba = 0;

		js_snprintf(line, sizeof (line),
		    "[RBSPTI] FAKE_SUMMARY writes=%llu bytes=%llu first_lba=%lu first_blocks=%lu last_end_lba=%lu last_blocks=%lu max_blocks=%lu\r\n",
		    (unsigned long long)rbFakeWriteCount,
		    (unsigned long long)rbFakeWriteBytes,
		    (unsigned long)firstLba,
		    (unsigned long)rbFakeFirstBlocks,
		    (unsigned long)rbFakeLastEndLba,
		    (unsigned long)rbFakeLastBlocks,
		    (unsigned long)rbFakeMaxBlocks);
		rb_trace_write(line);
	}

	rb_trace_write("[RBSPTI] END\r\n");
	FlushFileBuffers(rbTraceHandle);
	CloseHandle(rbTraceHandle);
	rbTraceHandle = INVALID_HANDLE_VALUE;
}

LOCAL void
rb_hex_bytes(src, len, dst, dstlen)
	const BYTE	*src;
	int		len;
	char		*dst;
	int		dstlen;
{
	static const char hex[] = "0123456789ABCDEF";
	int	i;
	int	p = 0;

	if (dst == NULL || dstlen <= 0)
		return;

	dst[0] = '\0';
	if (src == NULL || len <= 0)
		return;

	for (i = 0; i < len && (p + 2) < dstlen; i++) {
		dst[p++] = hex[(src[i] >> 4) & 0x0F];
		dst[p++] = hex[src[i] & 0x0F];
	}
	dst[p] = '\0';
}
/*
 * Initialization of SCSI Pass Through Interface code.  Responsible for
 * setting up the array of SCSI devices.  This code will be a little
 * different from the normal code -- it will query each drive letter from
 * C: through Z: to see if it is  a CD.  When we identify a CD, we then
 * send CDB with the INQUIRY command to it -- NT will automagically fill in
 * the PathId, TargetId, and Lun for us.
 */
LOCAL int
InitSCSIPT(void)
{
	BYTE	i;
	BYTE	j;
	char	buf[4];
	UINT	uDriveType;
	int	retVal = 0;
	USHORT hasortval;
	char adapter_name[20];
	HANDLE	fh;
	ULONG	returned;
	BOOL	status;
	char	InquiryBuffer[2048];
	PSCSI_ADAPTER_BUS_INFO	ai;
	BYTE	bus;

	if (bSCSIPTInit)
		return (0);

	/*
	 * Detect all Busses on all SCSI-Adapters
	 * Fill up map array that allows us to later assign devices to
	 * bus numbers.
	 */
	sptihamax = 0;
	i = 0;
	do {
		js_snprintf(adapter_name, sizeof (adapter_name), "\\\\.\\SCSI%d:", i);
		fh = CreateFile(adapter_name, GENERIC_READ | GENERIC_WRITE,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL,
						OPEN_EXISTING, 0, NULL);
		if (fh != INVALID_HANDLE_VALUE) {
			status	= DeviceIoControl(fh,
						IOCTL_SCSI_GET_INQUIRY_DATA,
						NULL,
						0,
						InquiryBuffer,
						2048,
						&returned,
						FALSE);
			if (status) {
				ai = (PSCSI_ADAPTER_BUS_INFO) InquiryBuffer;
				for (bus = 0; bus < ai->NumberOfBusses; bus++) {
					sptihasortarr[sptihamax] = ((i<<8) | bus);
					sptihamax++;
				}
			}
			CloseHandle(fh);
		}
		i++;
	} while (fh != INVALID_HANDLE_VALUE);

	errno = 0;
	memset(&sptiglobal, 0, sizeof (SPTIGLOBAL));
	for (i = 0; i < NUM_MAX_NTSCSI_DRIVES; i++)
		sptiglobal.drive[i].hDevice = INVALID_HANDLE_VALUE;

	for (i = NUM_FLOPPY_DRIVES; i < NUM_MAX_NTSCSI_DRIVES; i++) {
		js_snprintf(buf, sizeof (buf), "%c:\\", (char)('A'+i));
		uDriveType = GetDriveType(buf);
#ifdef	CDROM_ONLY
		if (uDriveType == DRIVE_CDROM) {
#else
		if (TRUE) {
#endif
			GetDriveInformation(i, &sptiglobal.drive[i]);
			if (sptiglobal.drive[i].bUsed) {
				retVal++;
				hasortval = (sptiglobal.drive[i].PortNumber<<8) | sptiglobal.drive[i].PathId;
				for (j = 0; j < sptihamax; j++) {
					if (hasortval <= sptihasortarr[j])
						break;
				}
				if (j == sptihamax) {
					sptihasortarr[j] = hasortval;
					sptihamax++;
				} else if (hasortval < sptihasortarr[j]) {
					memmove(&sptihasortarr[j+1], &sptihasortarr[j], (sptihamax-j) * sizeof (USHORT));
					sptihasortarr[j] = hasortval;
					sptihamax++;
				}
			}
		}
	}
	if (sptihamax > 0) {
		for (i = NUM_FLOPPY_DRIVES; i < NUM_MAX_NTSCSI_DRIVES; i++)
			if (sptiglobal.drive[i].bUsed)
				for (j = 0; j < sptihamax; j++) {
					if (sptihasortarr[j] ==
					    ((sptiglobal.drive[i].PortNumber<<8) | sptiglobal.drive[i].PathId)) {
						sptiglobal.drive[i].ha = j;
						break;
					}
				}
	}
	sptiglobal.numAdapters = SPTIGetNumAdapters();

	bSCSIPTInit = TRUE;

	if (retVal > 0)
		bUsingSCSIPT = TRUE;

	return (retVal);
}


LOCAL int
DeinitSCSIPT(void)
{
	BYTE	i;

	if (!bSCSIPTInit)
		return (0);

	for (i = NUM_FLOPPY_DRIVES; i < NUM_MAX_NTSCSI_DRIVES; i++) {
		if (sptiglobal.drive[i].bUsed) {
			CloseHandle(sptiglobal.drive[i].hDevice);
		}
	}

	sptiglobal.numAdapters = SPTIGetNumAdapters();

	memset(&sptiglobal, 0, sizeof (SPTIGLOBAL));
	bSCSIPTInit = FALSE;
	return (-1);
}


/*
 * Returns the number of "adapters" present.
 */
LOCAL BYTE
SPTIGetNumAdapters(void)
{
	BYTE	buf[256];
	WORD	i;
	BYTE	numAdapters = 0;

	memset(buf, 0, 256);

	/*
	 * PortNumber 0 should exist, so pre-mark it.  This avoids problems
	 * when the primary IDE drives are on PortNumber 0, but can't be opened
	 * because of insufficient privelege (ie. non-admin).
	 */
	buf[0] = 1;
	for (i = 0; i < NUM_MAX_NTSCSI_DRIVES; i++) {
		if (sptiglobal.drive[i].bUsed)
			buf[sptiglobal.drive[i].ha] = 1;
	}

	for (i = 0; i <= 255; i++)
		if (buf[i])
			numAdapters = (BYTE)(i + 1);

/*	numAdapters++; */

	return (numAdapters);
}

#include <ctype.h>
LOCAL BOOL
w2k_or_newer(void)
{
	OSVERSIONINFO osver;

	memset(&osver, 0, sizeof (osver));
	osver.dwOSVersionInfoSize = sizeof (osver);
	GetVersionEx(&osver);
	if (osver.dwPlatformId == VER_PLATFORM_WIN32_NT) {
		/*
		 * Win2000 is NT-5.0, Win-XP is NT-5.1
		 */
		if (osver.dwMajorVersion > 4)
			return (TRUE);
	}
	return (FALSE);
}

LOCAL BOOL
w2kstyle_create(void)
{
	OSVERSIONINFO osver;

/*	return FALSE; */
	memset(&osver, 0, sizeof (osver));
	osver.dwOSVersionInfoSize = sizeof (osver);
	GetVersionEx(&osver);
	if (osver.dwPlatformId == VER_PLATFORM_WIN32_NT) {
		/*
		 * Win2000 is NT-5.0, Win-XP is NT-5.1
		 */
		if (osver.dwMajorVersion > 4)
			return (TRUE);

		if (osver.dwMajorVersion == 4) {		/* NT-4.x */
			char	*vers = osver.szCSDVersion;

			if (strlen(vers) == 0)
				return (FALSE);

			/*
			 * Servicepack is installed, skip over non-digit part
			 */
			while (*vers != '\0' && !isdigit(*vers))
				vers++;
			if (*vers == '\0')
				return (FALSE);

			if (isdigit(vers[0]) &&
			    (atoi(vers) >= 4 || isdigit(vers[1])))	/* Fom Service Pack 4 */
				return (TRUE);				/* same as for W2K */
		}
	}
	return (FALSE);
}


/*
 * Universal function to get a file handle to the CD device.  Since
 * NT 4.0 wants just the GENERIC_READ flag, and Win2K wants both
 * GENERIC_READ and GENERIC_WRITE (why a read-only CD device needs
 * GENERIC_WRITE access is beyond me...), the easist workaround is to just
 * try them both.
 */
LOCAL HANDLE
GetFileHandle(BYTE i, BOOL openshared)
{
	char	buf[12];
	char	rbLine[512];
	HANDLE	fh;
	DWORD	dwFlags = GENERIC_READ;
	DWORD	dwAccessMode = 0;
	DWORD	rbOpenError;

	dwAccessMode = FILE_SHARE_READ;
	if (w2kstyle_create()) {
		dwFlags |= GENERIC_WRITE;
		dwAccessMode |= FILE_SHARE_WRITE;
#ifdef _DEBUG_SCSIPT
		js_fprintf(scgp_errfile, "SPTI: GetFileHandle(): Setting for Win2K\n");
#endif
	}
	js_snprintf(buf, sizeof (buf), "\\\\.\\%c:", (char)('A'+i));
#ifdef CREATE_NONSHARED
	if (openshared) {
		fh = CreateFile(buf, dwFlags, dwAccessMode, NULL,
						OPEN_EXISTING, 0, NULL);
	} else {
		fh = CreateFile(buf, dwFlags, 0, NULL,
						OPEN_EXISTING, 0, NULL);
	}
	if (!openshared && fh == INVALID_HANDLE_VALUE &&
	    GetLastError() == ERROR_SHARING_VIOLATION)
#endif
		fh = CreateFile(buf, dwFlags, dwAccessMode, NULL,
						OPEN_EXISTING, 0, NULL);
	if (fh == INVALID_HANDLE_VALUE) {
		dwFlags ^= GENERIC_WRITE;
		dwAccessMode ^= FILE_SHARE_WRITE;
#ifdef CREATE_NONSHARED
		if (openshared) {
			fh = CreateFile(buf, dwFlags, dwAccessMode, NULL,
						OPEN_EXISTING, 0, NULL);
		} else {
			fh = CreateFile(buf, dwFlags, 0, NULL,
						OPEN_EXISTING, 0, NULL);
		}
		if (!openshared && fh == INVALID_HANDLE_VALUE &&
		    GetLastError() == ERROR_SHARING_VIOLATION)
#endif
			fh = CreateFile(buf, dwFlags, dwAccessMode, NULL,
						OPEN_EXISTING, 0, NULL);
	}

	rbOpenError = (fh == INVALID_HANDLE_VALUE) ?
			GetLastError() : ERROR_SUCCESS;

	if (rb_trace_enabled()) {
		js_snprintf(rbLine, sizeof (rbLine),
		    "[RBSPTI] OPEN drive=%c: path=%s openshared=%d access=0x%08lX share=0x%08lX ok=%d winerr=%lu handle=%p\r\n",
		    (char)('A'+i),
		    buf,
		    (int)openshared,
		    (unsigned long)dwFlags,
		    (unsigned long)dwAccessMode,
		    fh != INVALID_HANDLE_VALUE ? 1 : 0,
		    (unsigned long)rbOpenError,
		    (void *)fh);
		rb_trace_write(rbLine);
	}

#ifdef _DEBUG_SCSIPT
	if (fh == INVALID_HANDLE_VALUE)
		js_fprintf(scgp_errfile, "SPTI: CreateFile() failed! -> %d\n", GetLastError());
	else
		js_fprintf(scgp_errfile, "SPTI: CreateFile() returned %d\n", GetLastError());
#endif

	return (fh);
}


/*
 * fills in a pDrive structure with information from a SCSI_INQUIRY
 * and obtains the ha:tgt:lun values via IOCTL_SCSI_GET_ADDRESS
 */
LOCAL void
GetDriveInformation(BYTE i, DRIVE *pDrive)
{
	HANDLE		fh;
	BOOL		status;
	SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER swb;
	SCSI_ADDRESS	scsiAddr;
	ULONG		length;
	ULONG		returned;
	BYTE		inqData[NTSCSI_HA_INQUIRY_SIZE];

#ifdef _DEBUG_SCSIPT
	js_fprintf(scgp_errfile, "SPTI: Checking drive %c:", 'A'+i);
#endif

	fh = GetFileHandle(i, TRUE);	/* No NONSHARED Create for inquiry */

	if (fh == INVALID_HANDLE_VALUE) {
#ifdef _DEBUG_SCSIPT
		js_fprintf(scgp_errfile, "       : fh == INVALID_HANDLE_VALUE\n");
#endif
		return;
	}

#ifdef _DEBUG_SCSIPT
	js_fprintf(scgp_errfile, "       : Index %d: fh == %08X\n", i, fh);
#endif


	/*
	 * Get the drive inquiry data
	 */
	memset(&swb, 0, sizeof (swb));
	memset(inqData, 0, sizeof (inqData));
	swb.spt.Length		= sizeof (SCSI_PASS_THROUGH_DIRECT);
	swb.spt.CdbLength	= 6;
	swb.spt.SenseInfoLength	= 24;
	swb.spt.DataIn		= SCSI_IOCTL_DATA_IN;
	swb.spt.DataTransferLength = sizeof (inqData);
	swb.spt.TimeOutValue	= 2;
	swb.spt.DataBuffer	= inqData;
	swb.spt.SenseInfoOffset	= offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);
	swb.spt.Cdb[0]		= SCSI_CMD_INQUIRY;
	swb.spt.Cdb[4]		= NTSCSI_HA_INQUIRY_SIZE;

	length = sizeof (SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER);
	status = DeviceIoControl(fh,
			    IOCTL_SCSI_PASS_THROUGH_DIRECT,
			    &swb,
			    sizeof (swb),
			    &swb,
			    sizeof (swb),
			    &returned,
			    NULL);

	if (!status) {
		CloseHandle(fh);
#ifdef _DEBUG_SCSIPT
		js_fprintf(scgp_errfile, "SPTI: Error DeviceIoControl() -> %d\n", GetLastError());
#endif
		return;
	}

	memcpy(pDrive->inqData, inqData, NTSCSI_HA_INQUIRY_SIZE);

	/*
	 * Prefer the storage-property bus classification for USB/IEEE-1394.
	 * This avoids the Windows 8+ case where IOCTL_SCSI_GET_ADDRESS can
	 * report success but return a zero/useless address for a USB bridge.
	 */
	{
		STORAGE_BUS_TYPE rbBusType = BusTypeUnknown;
		BOOL rbBusKnown = rb_get_storage_bus_type(fh, &rbBusType);

		if (rbBusKnown &&
		    (rbBusType == BusTypeUsb || rbBusType == BusType1394)) {
			rb_use_drive_letter_address(i, pDrive);
#ifdef _DEBUG_SCSIPT
			js_fprintf(scgp_errfile,
			    "RetroBurner external-bus device %c: BusType=%d synthetic=(%d:%d:%d)\n",
			    (char)i+'A', (int)rbBusType, i, 0, 0);
#endif
		} else {
			/*
			 * Original Schily path for devices with a meaningful legacy
			 * SCSI address.
			 */
			memset(&scsiAddr, 0, sizeof (SCSI_ADDRESS));
			scsiAddr.Length = sizeof (SCSI_ADDRESS);

			if (DeviceIoControl(fh, IOCTL_SCSI_GET_ADDRESS, NULL, 0,
				    &scsiAddr, sizeof (SCSI_ADDRESS), &returned,
				    NULL)) {
#ifdef _DEBUG_SCSIPT
				js_fprintf(scgp_errfile,
				    "Device %c: Port=%d, PathId=%d, TargetId=%d, Lun=%d\n",
				    (char)i+'A', scsiAddr.PortNumber, scsiAddr.PathId,
				    scsiAddr.TargetId, scsiAddr.Lun);
#endif
				pDrive->bUsed		= TRUE;
				pDrive->ha		= scsiAddr.PortNumber;
				pDrive->PortNumber	= scsiAddr.PortNumber;
				pDrive->PathId		= scsiAddr.PathId;
				pDrive->tgt		= scsiAddr.TargetId;
				pDrive->lun		= scsiAddr.Lun;
				pDrive->driveLetter	= i;
				pDrive->hDevice		= INVALID_HANDLE_VALUE;

			} else if (GetLastError() == ERROR_NOT_SUPPORTED) {
				/*
				 * Preserve Schily's original USB/FireWire fallback for
				 * systems where the legacy address query is rejected.
				 */
				rb_use_drive_letter_address(i, pDrive);
#ifdef _DEBUG_SCSIPT
				js_fprintf(scgp_errfile,
				    "RetroBurner unsupported-address fallback %c: synthetic=(%d:%d:%d)\n",
				    (char)i+'A', i, 0, 0);
#endif
			} else {
				pDrive->bUsed = FALSE;
#ifdef _DEBUG_SCSIPT
				js_fprintf(scgp_errfile,
				    "SPTI: Device %c: Error IOCTL_SCSI_GET_ADDRESS(): %d\n",
				    (char)i+'A', GetLastError());
#endif
				CloseHandle(fh);
				return;
			}
		}
	}
#ifdef _DEBUG_SCSIPT
	js_fprintf(scgp_errfile,  "SPTI: Adding drive %c: (%d:%d:%d)\n", 'A'+i,
					pDrive->ha, pDrive->tgt, pDrive->lun);
#endif
	if (rb_trace_enabled()) {
		STORAGE_BUS_TYPE rbTraceBusType = BusTypeUnknown;
		BOOL rbTraceBusKnown;
		char rbLine[512];

		rbTraceBusKnown =
		    rb_get_storage_bus_type(fh, &rbTraceBusType);

		js_snprintf(rbLine, sizeof (rbLine),
		    "[RBSPTI] ENUM drive=%c: bus_known=%d bus_type=%d initial_ha=%u port=%u path=%u tgt=%u lun=%u synthetic=%d\r\n",
		    (char)('A'+i),
		    (int)rbTraceBusKnown,
		    (int)rbTraceBusType,
		    (unsigned int)pDrive->ha,
		    (unsigned int)pDrive->PortNumber,
		    (unsigned int)pDrive->PathId,
		    (unsigned int)pDrive->tgt,
		    (unsigned int)pDrive->lun,
		    pDrive->PortNumber >= 64 ? 1 : 0);
		rb_trace_write(rbLine);
	}
	CloseHandle(fh);
}



LOCAL DWORD
SPTIHandleHaInquiry(LPSRB_HAInquiry lpsrb)
{
	DWORD	*pMTL;

	lpsrb->HA_Count    = sptiglobal.numAdapters;
	if (lpsrb->SRB_HaId >= sptiglobal.numAdapters) {
		lpsrb->SRB_Status = SS_INVALID_HA;
		return (SS_INVALID_HA);
	}
	lpsrb->HA_SCSI_ID  = 7;			/* who cares... we're not really an ASPI manager */
	memcpy(lpsrb->HA_ManagerId,  "AKASPI v0.000001", 16);
	memcpy(lpsrb->HA_Identifier, "SCSI Adapter    ", 16);
	lpsrb->HA_Identifier[13] = (char)('0'+lpsrb->SRB_HaId);
	memset(lpsrb->HA_Unique, 0, 16);
	lpsrb->HA_Unique[3] = 8;
	pMTL = (LPDWORD)&lpsrb->HA_Unique[4];
	*pMTL = 64 * 1024;

	lpsrb->SRB_Status = SS_COMP;
	return (SS_COMP);
}

/*
 * Looks up the index in the drive array for a given ha:tgt:lun triple
 */
LOCAL BYTE
SPTIGetDeviceIndex(BYTE ha, BYTE tgt, BYTE lun)
{
	BYTE	i;

#ifdef _DEBUG_SCSIPT
	js_fprintf(scgp_errfile,  "SPTI: SPTIGetDeviceIndex\n");
#endif

	for (i = NUM_FLOPPY_DRIVES; i < NUM_MAX_NTSCSI_DRIVES; i++) {
		if (sptiglobal.drive[i].bUsed) {
			DRIVE	*lpd;

			lpd = &sptiglobal.drive[i];
			if ((lpd->ha == ha) && (lpd->tgt == tgt) && (lpd->lun == lun))
				return (i);
		}
	}

	return (0);
}

/*
 * Converts ASPI-style SRB to SCSI Pass Through IOCTL
 */

LOCAL DWORD
SPTIExecSCSICommand(LPSRB_ExecSCSICmd lpsrb, int sptTimeOutValue, BOOL bBeenHereBefore)
{
	BOOL	status;
	BOOL	rbIsWrite;
	BOOL	rbFailed;
	BOOL	rbLogCommand;
	SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER swb;
	ULONG	length;
	ULONG	returned;
	ULONG	rbCmdNo;
	ULONG	rbWriteNo;
	ULONG	rbLba;
	ULONG	rbBlocks;
	DWORD	rbStartTick;
	DWORD	rbElapsed;
	DWORD	rbWinErr;
	BYTE	idx;
	BYTE	j;
	int	rbSenseBytes;
	char	rbCdbHex[64];
	char	rbSenseHex[(SENSE_LEN_SPTI * 2) + 1];
	char	rbLine[1536];
	const char *rbKind;

	idx = SPTIGetDeviceIndex(lpsrb->SRB_HaId, lpsrb->SRB_Target, lpsrb->SRB_Lun);

	if (idx == 0) {
		lpsrb->SRB_Status = SS_NO_DEVICE;
		return (SS_NO_DEVICE);
	}

	if (lpsrb->CDBByte[0] == SCSI_CMD_INQUIRY) {
		lpsrb->SRB_Status = SS_COMP;
		memcpy(lpsrb->SRB_BufPointer, sptiglobal.drive[idx].inqData,
		    NTSCSI_HA_INQUIRY_SIZE);
		return (SS_COMP);
	}

	if (sptiglobal.drive[idx].hDevice == INVALID_HANDLE_VALUE)
		sptiglobal.drive[idx].hDevice =
		    GetFileHandle(sptiglobal.drive[idx].driveLetter, FALSE);

	memset(&swb, 0, sizeof (swb));
	swb.spt.Length		= sizeof (SCSI_PASS_THROUGH);
	swb.spt.CdbLength	= lpsrb->SRB_CDBLen;
	if (lpsrb->SRB_Flags & SRB_DIR_IN)
		swb.spt.DataIn	= SCSI_IOCTL_DATA_IN;
	else if (lpsrb->SRB_Flags & SRB_DIR_OUT)
		swb.spt.DataIn	= SCSI_IOCTL_DATA_OUT;
	else
		swb.spt.DataIn	= SCSI_IOCTL_DATA_UNSPECIFIED;
	swb.spt.DataTransferLength = lpsrb->SRB_BufLen;
	swb.spt.TimeOutValue	= sptTimeOutValue;
	swb.spt.SenseInfoLength	= lpsrb->SRB_SenseLen;
	swb.spt.DataBuffer	= lpsrb->SRB_BufPointer;
	swb.spt.SenseInfoOffset	=
	    offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);
	memcpy(swb.spt.Cdb, lpsrb->CDBByte, lpsrb->SRB_CDBLen);
	length = sizeof (swb);

	rb_trace_init();
	rbCmdNo = ++rbTraceCmdCount;
	rbWriteNo = 0;
	rbLba = 0;
	rbBlocks = 0;

	rbIsWrite = (swb.spt.Cdb[0] == 0x2A ||
		     swb.spt.Cdb[0] == 0xAA);

	if (rbIsWrite) {
		rbWriteNo = ++rbTraceWriteCount;
		rbLba = ((ULONG)swb.spt.Cdb[2] << 24) |
			((ULONG)swb.spt.Cdb[3] << 16) |
			((ULONG)swb.spt.Cdb[4] << 8) |
			(ULONG)swb.spt.Cdb[5];

		if (swb.spt.Cdb[0] == 0x2A) {
			rbBlocks = ((ULONG)swb.spt.Cdb[7] << 8) |
				   (ULONG)swb.spt.Cdb[8];
		} else {
			rbBlocks = ((ULONG)swb.spt.Cdb[6] << 24) |
				   ((ULONG)swb.spt.Cdb[7] << 16) |
				   ((ULONG)swb.spt.Cdb[8] << 8) |
				   (ULONG)swb.spt.Cdb[9];
		}
	}

	if (rb_fake_enabled() &&
	    ((lpsrb->SRB_Flags & SRB_DIR_OUT) ||
	     rb_fake_mutating_opcode(swb.spt.Cdb[0]))) {
		BOOL	rbShapeChange;
		ULONG	rbSenseLen;
		ULONGLONG rbBytes;

		rbBytes = (ULONGLONG)swb.spt.DataTransferLength;
		rbShapeChange = FALSE;

		if (rbIsWrite) {
			rbFakeWriteCount++;
			rbFakeWriteBytes += rbBytes;

			if (rbFakeFirstLba == 0xFFFFFFFFUL) {
				rbFakeFirstLba = rbLba;
				rbFakeFirstBlocks = rbBlocks;
			}

			if (rbBlocks > rbFakeMaxBlocks)
				rbFakeMaxBlocks = rbBlocks;

			if (rbFakeLastBlocks != 0 &&
			    rbFakeLastBlocks != rbBlocks)
				rbShapeChange = TRUE;

			rbFakeLastBlocks = rbBlocks;
			rbFakeLastEndLba = rbLba + rbBlocks;

			if (rb_trace_enabled() &&
			    (rbFakeWriteCount <= 16 ||
			     (rbFakeWriteCount % 1024ULL) == 0 ||
			     rbShapeChange)) {
				rb_hex_bytes(swb.spt.Cdb, swb.spt.CdbLength,
				    rbCdbHex, sizeof (rbCdbHex));

				js_snprintf(rbLine, sizeof (rbLine),
				    "[RBSPTI] FAKE_WRITE n=%llu cmd=%lu drive=%c: op=0x%02X lba=%lu blocks=%lu bytes=%llu total_bytes=%llu cdb=%s hardware_called=0 result=GOOD\r\n",
				    (unsigned long long)rbFakeWriteCount,
				    (unsigned long)rbCmdNo,
				    (char)('A' + sptiglobal.drive[idx].driveLetter),
				    (unsigned int)swb.spt.Cdb[0],
				    (unsigned long)rbLba,
				    (unsigned long)rbBlocks,
				    (unsigned long long)rbBytes,
				    (unsigned long long)rbFakeWriteBytes,
				    rbCdbHex);
				rb_trace_write(rbLine);
			}
		} else if (rb_trace_enabled()) {
			rb_hex_bytes(swb.spt.Cdb, swb.spt.CdbLength,
			    rbCdbHex, sizeof (rbCdbHex));

			js_snprintf(rbLine, sizeof (rbLine),
			    "[RBSPTI] FAKE_MUTATE cmd=%lu drive=%c: op=0x%02X dir_out=%d req_bytes=%lu cdb=%s hardware_called=0 result=GOOD\r\n",
			    (unsigned long)rbCmdNo,
			    (char)('A' + sptiglobal.drive[idx].driveLetter),
			    (unsigned int)swb.spt.Cdb[0],
			    (lpsrb->SRB_Flags & SRB_DIR_OUT) ? 1 : 0,
			    (unsigned long)swb.spt.DataTransferLength,
			    rbCdbHex);
			rb_trace_write(rbLine);
		}

		rbSenseLen = (ULONG)lpsrb->SRB_SenseLen;
		if (rbSenseLen > 0)
			memset(lpsrb->SenseArea, 0, rbSenseLen);

		lpsrb->SRB_SenseLen = 0;
		lpsrb->SRB_TargStat = 0;
		lpsrb->SRB_Status = SS_COMP;
		return (SS_COMP);
	}

#ifdef _DEBUG_SCSIPT
	js_fprintf(scgp_errfile, "SPTI: SPTIExecSCSICmd: calling DeviceIoControl()");
	js_fprintf(scgp_errfile, "       : cmd == 0x%02X", swb.spt.Cdb[0]);
#endif

	rbStartTick = GetTickCount();

	status = DeviceIoControl(sptiglobal.drive[idx].hDevice,
			    IOCTL_SCSI_PASS_THROUGH_DIRECT,
			    &swb,
			    length,
			    &swb,
			    length,
			    &returned,
			    NULL);

	/*
	 * Capture GetLastError immediately, before trace formatting or any other
	 * Win32 call can change the thread's last-error value.
	 */
	rbWinErr = GetLastError();
	rbElapsed = GetTickCount() - rbStartTick;

	lpsrb->SRB_SenseLen = swb.spt.SenseInfoLength;
	memcpy(lpsrb->SenseArea, swb.ucSenseBuf, lpsrb->SRB_SenseLen);

	rbFailed = (!status || swb.spt.ScsiStatus != 0);
	rbLogCommand = rb_trace_enabled() &&
	    (!rbIsWrite ||
	     rbFailed ||
	     rbWriteNo == 1 ||
	     (rbWriteNo % RB_TRACE_WRITE_INTERVAL) == 0);

	if (rbLogCommand) {
		rb_hex_bytes(swb.spt.Cdb, swb.spt.CdbLength,
		    rbCdbHex, sizeof (rbCdbHex));

		if (rbFailed)
			rbKind = "FAIL";
		else if (rbIsWrite)
			rbKind = "WRITE_CHECKPOINT";
		else
			rbKind = "CMD";

		js_snprintf(rbLine, sizeof (rbLine),
		    "[RBSPTI] %s cmd=%lu write=%lu drive=%c: addr=%u,%u,%u op=0x%02X dir=%u req_bytes=%lu xfer_bytes=%lu ioctl_returned=%lu timeout_s=%lu ioctl_ok=%d winerr=%lu winerr_valid=%d scsi=0x%02X sense_len=%u elapsed_ms=%lu retry=%d lba=%lu blocks=%lu cdb=%s\r\n",
		    rbKind,
		    (unsigned long)rbCmdNo,
		    (unsigned long)rbWriteNo,
		    (char)('A' + sptiglobal.drive[idx].driveLetter),
		    (unsigned int)lpsrb->SRB_HaId,
		    (unsigned int)lpsrb->SRB_Target,
		    (unsigned int)lpsrb->SRB_Lun,
		    (unsigned int)swb.spt.Cdb[0],
		    (unsigned int)swb.spt.DataIn,
		    (unsigned long)lpsrb->SRB_BufLen,
		    (unsigned long)swb.spt.DataTransferLength,
		    (unsigned long)returned,
		    (unsigned long)swb.spt.TimeOutValue,
		    status ? 1 : 0,
		    (unsigned long)rbWinErr,
		    status ? 0 : 1,
		    (unsigned int)swb.spt.ScsiStatus,
		    (unsigned int)swb.spt.SenseInfoLength,
		    (unsigned long)rbElapsed,
		    bBeenHereBefore ? 1 : 0,
		    (unsigned long)rbLba,
		    (unsigned long)rbBlocks,
		    rbCdbHex);
		rb_trace_write(rbLine);
	}

	if (rbFailed && rb_trace_enabled()) {
		rbSenseBytes = (int)swb.spt.SenseInfoLength;
		if (rbSenseBytes > SENSE_LEN_SPTI)
			rbSenseBytes = SENSE_LEN_SPTI;
		if (rbSenseBytes < 0)
			rbSenseBytes = 0;

		rb_hex_bytes(swb.ucSenseBuf, rbSenseBytes,
		    rbSenseHex, sizeof (rbSenseHex));

		js_snprintf(rbLine, sizeof (rbLine),
		    "[RBSPTI] SENSE cmd=%lu drive=%c: bytes=%d data=%s\r\n",
		    (unsigned long)rbCmdNo,
		    (char)('A' + sptiglobal.drive[idx].driveLetter),
		    rbSenseBytes,
		    rbSenseHex);
		rb_trace_write(rbLine);

		/*
		 * Failure is already present; make the diagnostic durable before
		 * cdrecord unwinds/aborts. No per-WRITE flush is performed.
		 */
		FlushFileBuffers(rbTraceHandle);
	}

	if (status && swb.spt.ScsiStatus == 0) {
		lpsrb->SRB_Status = SS_COMP;
#ifdef _DEBUG_SCSIPT
		js_fprintf(scgp_errfile, "       : SRB_Status == SS_COMP\n");
#endif
	} else {
		DWORD	dwErrCode;

		lpsrb->SRB_Status = SS_ERR;
		lpsrb->SRB_TargStat = swb.spt.ScsiStatus;

		/*
		 * Use the value captured immediately after DeviceIoControl so the
		 * trace code itself cannot perturb the existing recovery decision.
		 */
		dwErrCode = rbWinErr;
#ifdef _DEBUG_SCSIPT
		js_fprintf(scgp_errfile,
		    "       : error == %d   handle == %08X\n",
		    dwErrCode, sptiglobal.drive[idx].hDevice);
#endif
		/*
		 * Existing Schily media-change/invalid-handle recovery path.
		 */
		if (!bBeenHereBefore &&
		    ((dwErrCode == ERROR_MEDIA_CHANGED) ||
		     (dwErrCode == ERROR_INVALID_HANDLE))) {
			if (dwErrCode != ERROR_INVALID_HANDLE)
				CloseHandle(sptiglobal.drive[idx].hDevice);
			GetDriveInformation(idx, &sptiglobal.drive[idx]);
			if (sptihamax > 0) {
				if (sptiglobal.drive[idx].bUsed)
					for (j = 0; j < sptihamax; j++) {
						if (sptihasortarr[j] ==
						    ((sptiglobal.drive[idx].PortNumber << 8) |
						     sptiglobal.drive[idx].PathId)) {
							sptiglobal.drive[idx].ha = j;
							break;
						}
					}
			}
#ifdef _DEBUG_SCSIPT
			js_fprintf(scgp_errfile,
			    "SPTI: SPTIExecSCSICommand: Retrying after ERROR_MEDIA_CHANGED\n");
#endif
			return (SPTIExecSCSICommand(lpsrb,
			    sptTimeOutValue, TRUE));
		}
	}
	return (lpsrb->SRB_Status);
}
/* SPTI End -----------------------------------------------------------------*/


LOCAL void
exit_func()
{
	rb_trace_close();

	if (!close_driver())
		errmsgno(EX_BAD, "Cannot close Win32-ASPI-Driver.\n");
}

/*
 * Return version information for the low level SCSI transport code.
 * This has been introduced to make it easier to trace down problems
 * in applications.
 */
LOCAL char *
scgo_version(scgp, what)
	SCSI	*scgp;
	int	what;
{
	if (scgp != (SCSI *)0) {
		switch (what) {

		case SCG_VERSION:
			if (bUsingSCSIPT)
				return (_scg_itrans_version);
			return (_scg_trans_version);
		/*
		 * If you changed this source, you are not allowed to
		 * return "schily" for the SCG_AUTHOR request.
		 */
		case SCG_AUTHOR:
			return (_scg_auth_schily);
		case SCG_SCCS_ID:
			return (__sccsid);
		}
	}
	return ((char *)0);
}

LOCAL int
scgo_help(scgp, f)
	SCSI	*scgp;
	FILE	*f;
{
	__scg_help(f, "ASPI", "Generic transport independent SCSI",
		"ASPI:", "bus,target,lun", "ASPI:1,2,0", TRUE, FALSE);
	__scg_help(f, "SPTI", "Generic SCSI for Windows NT/2000/XP",
		"SPTI:", "bus,target,lun", "SPTI:1,2,0", TRUE, FALSE);
	return (0);
}

LOCAL int
scgo_open(scgp, device)
	SCSI	*scgp;
	char	*device;
{
	int	busno	= scg_scsibus(scgp);
	int	tgt	= scg_target(scgp);
	int	tlun	= scg_lun(scgp);

	if (busno >= MAX_SCG || tgt >= MAX_TGT || tlun >= MAX_LUN) {
		errno = EINVAL;
		if (scgp->errstr)
			js_snprintf(scgp->errstr, SCSI_ERRSTR_SIZE,
				"Illegal value for busno, target or lun '%d,%d,%d'",
				busno, tgt, tlun);
		return (-1);
	}

	if (device != NULL &&
	    (strcmp(device, "SPTI") == 0 || strcmp(device, "ASPI") == 0))
		goto devok;

	if ((device != NULL && *device != '\0') || (busno == -2 && tgt == -2)) {
		errno = EINVAL;
		if (scgp->errstr)
			js_snprintf(scgp->errstr, SCSI_ERRSTR_SIZE,
				"Open by 'devname' not supported on this OS");
		return (-1);
	}
devok:
	if (AspiLoaded <= 0) {	/* do not change access method on open driver */
		bForceAccess = FALSE;
#ifdef PREFER_SPTI
		bUsingSCSIPT = TRUE;
#else
		bUsingSCSIPT = FALSE;
#endif
		if (!w2k_or_newer())
			bUsingSCSIPT = FALSE;

		if (scgp->debug > 0) {
			js_fprintf((FILE *)scgp->errfile,
				"scgo_open: Prefered SCSI transport: %s\n",
					bUsingSCSIPT ? "SPTI":"ASPI");
		}
		if (device != NULL && strcmp(device, "SPTI") == 0) {
			bUsingSCSIPT = TRUE;
			bForceAccess = TRUE;
		} else if (device != NULL && strcmp(device, "ASPI") == 0) {
			bUsingSCSIPT = FALSE;
			bForceAccess = TRUE;
		}
		if (device != NULL && scgp->debug > 0) {
			js_fprintf((FILE *)scgp->errfile,
				"scgo_open: Selected SCSI transport: %s\n",
					bUsingSCSIPT ? "SPTI":"ASPI");
		}
	}

	/*
	 *  Check if variables are within the range
	 */
	if (tgt >= 0 && tgt >= 0 && tlun >= 0) {
		/*
		 * This is the non -scanbus case.
		 */
		;
	} else if (tgt == -2 && tgt == -2 &&
		    (tgt == -2 || tlun >= 0)) {
		/*
		 * This is the dev=ASPI case.
		 */
		;
	} else if (tgt != -1 || tgt != -1 || tlun != -1) {
		errno = EINVAL;
		return (-1);
	}

	if (scgp->local == NULL) {
		scgp->local = malloc(sizeof (struct scg_local));
		if (scgp->local == NULL)
			return (0);
	}
	/*
	 * Try to open ASPI-Router
	 */
	if (!open_driver(scgp))
		return (-1);

	/*
	 * More than we have ...
	 */
	if (busno >= busses) {
		close_driver();
		return (-1);
	}

	/*
	 * Install Exit Function which closes the ASPI-Router
	 */
	atexit(exit_func);

	/*
	 * Success after all
	 */
	return (1);
}

LOCAL int
scgo_close(scgp)
	SCSI	*scgp;
{
	exit_func();
	return (0);
}

LOCAL long
scgo_maxdma(scgp, amt)
	SCSI	*scgp;
	long	amt;
{
	/*
	 * SPTI is the native Windows transport used by RetroBurner.  The old
	 * 63 KiB limit remains for ASPI; SPTI may use the full 64 KiB transfer.
	 */
	if (bUsingSCSIPT)
		return (MAX_DMA_SPTI);

	return (MAX_DMA_WNT);
}

LOCAL void *
scgo_getbuf(scgp, amt)
	SCSI	*scgp;
	long	amt;
{
	if (scgp->debug > 0) {
		js_fprintf((FILE *)scgp->errfile,
				"scgo_getbuf: %ld bytes\n", amt);
	}
	scgp->bufbase = malloc((size_t)(amt));
	return (scgp->bufbase);
}

LOCAL void
scgo_freebuf(scgp)
	SCSI	*scgp;
{
	if (scgp->bufbase)
		free(scgp->bufbase);
	scgp->bufbase = NULL;
}

LOCAL int
scgo_numbus(scgp)
	SCSI	*scgp;
{
	return (busses);
}

LOCAL __SBOOL
scgo_havebus(scgp, busno)
	SCSI	*scgp;
	int	busno;
{
	if (busno < 0 || busno >= busses)
		return (FALSE);

	return (TRUE);
}

LOCAL int
scgo_fileno(scgp, busno, tgt, tlun)
	SCSI	*scgp;
	int	busno;
	int	tgt;
	int	tlun;
{
	if (busno < 0 || busno >= busses ||
	    tgt < 0 || tgt >= MAX_TGT ||
	    tlun < 0 || tlun >= MAX_LUN)
		return (-1);

	/*
	 * Return fake
	 */
	return (1);
}


LOCAL int
scgo_initiator_id(scgp)
	SCSI	*scgp;
{
	SRB_HAInquiry	s;

	if (ha_inquiry(scgp, scg_scsibus(scgp), &s) < 0)
		return (-1);
	return (s.HA_SCSI_ID);
}

LOCAL int
scgo_isatapi(scgp)
	SCSI	*scgp;
{
	return (-1);	/* XXX Need to add real test */
}


/*
 * XXX scgo_reset not yet tested
 */
LOCAL int
scgo_reset(scgp, what)
	SCSI	*scgp;
	int	what;
{

	DWORD			Status = 0;
	DWORD			EventStatus = WAIT_OBJECT_0;
	HANDLE			Event	 = NULL;
	SRB_BusDeviceReset	s;

	if (what == SCG_RESET_NOP) {
		if (bUsingSCSIPT)
			return (-1);
		else
			return (0);  /* Can ASPI really reset? */
	}
	if (what != SCG_RESET_BUS) {
		errno = EINVAL;
		return (-1);
	}
	if (bUsingSCSIPT) {
		js_fprintf((FILE *)scgp->errfile,
					"Reset SCSI device not implemented with SPTI\n");
		return (-1);
	}

	/*
	 * XXX Does this reset TGT or BUS ???
	 */
	if (scgp->debug > 0) {
		js_fprintf((FILE *)scgp->errfile,
				"Attempting to reset SCSI device\n");
	}

	/*
	 * Check if ASPI library is loaded
	 */
	if (AspiLoaded <= 0) {
		js_fprintf((FILE *)scgp->errfile,
				"error in scgo_reset: ASPI driver not loaded !\n");
		return (-1);
	}

	memset(&s, 0, sizeof (s));	/* Clear SRB_BesDeviceReset structure */

	Event = CreateEvent(NULL, TRUE, FALSE, NULL);

	/*
	 * Set structure variables
	 */
	s.SRB_Cmd	= SC_RESET_DEV;			/* ASPI command code = SC_RESET_DEV	*/
	s.SRB_HaId	= scg_scsibus(scgp);		/* ASPI host adapter number		*/
	s.SRB_Flags	= SRB_EVENT_NOTIFY;		/* Flags				*/
	s.SRB_Target	= scg_target(scgp);		/* Target's SCSI ID			*/
	s.SRB_Lun	= scg_lun(scgp);		/* Target's LUN number			*/
	s.SRB_PostProc	= (LPVOID)Event;		/* Post routine				*/

	/*
	 * Initiate SCSI command
	 */
	Status = pfnSendASPI32Command((LPSRB)&s);

	/*
	 * Check status
	 */
	if (Status == SS_PENDING) {
		/*
		 * Wait till command completes
		 */
		EventStatus = WaitForSingleObject(Event, INFINITE);
	}


	/**************************************************/
	/* Reset event to non-signaled state.		  */
	/**************************************************/

	if (EventStatus == WAIT_OBJECT_0) {
		/*
		 * Clear event
		 */
		ResetEvent(Event);
	}

	/*
	 * Close the event handle
	 */
	CloseHandle(Event);

	/*
	 * Check condition
	 */
	if (s.SRB_Status != SS_COMP) {
		js_fprintf((FILE *)scgp->errfile,
					"ERROR! 0x%08X\n", s.SRB_Status);

		/*
		 * Indicate that error has occured
		 */
		return (-1);
	}

	if (scgp->debug > 0) {
		js_fprintf((FILE *)scgp->errfile,
					"Reset SCSI device completed\n");
	}

	/*
	 * Everything went OK
	 */
	return (0);
}


#ifdef DEBUG_WNTASPI
LOCAL void
DebugScsiSend(scgp, s, bDisplayBuffer)
	SCSI		*scgp;
	SRB_ExecSCSICmd	*s;
	int		bDisplayBuffer;
{
	int i;

	js_fprintf((FILE *)scgp->errfile, "\n\nDebugScsiSend\n");
	js_fprintf((FILE *)scgp->errfile, "s->SRB_Cmd          = 0x%02x\n", s->SRB_Cmd);
	js_fprintf((FILE *)scgp->errfile, "s->SRB_HaId         = 0x%02x\n", s->SRB_HaId);
	js_fprintf((FILE *)scgp->errfile, "s->SRB_Flags        = 0x%02x\n", s->SRB_Flags);
	js_fprintf((FILE *)scgp->errfile, "s->SRB_Target       = 0x%02x\n", s->SRB_Target);
	js_fprintf((FILE *)scgp->errfile, "s->SRB_Lun          = 0x%02x\n", s->SRB_Lun);
	js_fprintf((FILE *)scgp->errfile, "s->SRB_BufLen       = 0x%02x\n", s->SRB_BufLen);
	js_fprintf((FILE *)scgp->errfile, "s->SRB_BufPointer   = %x\n",	   s->SRB_BufPointer);
	js_fprintf((FILE *)scgp->errfile, "s->SRB_CDBLen       = 0x%02x\n", s->SRB_CDBLen);
	js_fprintf((FILE *)scgp->errfile, "s->SRB_SenseLen     = 0x%02x\n", s->SRB_SenseLen);
	js_fprintf((FILE *)scgp->errfile, "s->CDBByte          =");
	for (i = 0; i < min(s->SRB_CDBLen, 16); i++) {
		js_fprintf((FILE *)scgp->errfile, " %02X ", s->CDBByte[i]);
	}
	js_fprintf((FILE *)scgp->errfile, "\n");

#ifdef	__MORE_DEBUG__
	if (bDisplayBuffer != 0 && s->SRB_BufLen >= 8) {

		js_fprintf((FILE *)scgp->errfile, "s->SRB_BufPointer   =");
		for (i = 0; i < 8; i++) {
			js_fprintf((FILE *)scgp->errfile,
					" %02X ", ((char *)s->SRB_BufPointer)[i]);
		}
		js_fprintf((FILE *)scgp->errfile, "\n");
	}
#endif	/* __MORE_DEBUG__ */

	js_fprintf((FILE *)scgp->errfile, "Debug done\n");
}
#endif

LOCAL void
copy_sensedata(cp, sp)
	SRB_ExecSCSICmd	*cp;
	struct scg_cmd	*sp;
{
	sp->sense_count	= cp->SRB_SenseLen;
	if (sp->sense_count > sp->sense_len)
		sp->sense_count = sp->sense_len;

	memset(&sp->u_sense.Sense, 0x00, sizeof (sp->u_sense.Sense));
	memcpy(&sp->u_sense.Sense, cp->SenseArea, sp->sense_count);

	sp->u_scb.cmd_scb[0] = cp->SRB_TargStat;
}

/*
 * Set error flags
 */
LOCAL void
set_error(cp, sp)
	SRB_ExecSCSICmd	*cp;
	struct scg_cmd	*sp;
{
	switch (cp->SRB_Status) {

	case SS_COMP:			/* 0x01 SRB completed without error  */
		sp->error = SCG_NO_ERROR;
		sp->ux_errno = 0;
		break;

	case SS_ERR:			/* 0x04 SRB completed with error    */
		/*
		 * If the SCSI Status byte is != 0, we definitely could send
		 * the command to the target. We signal NO transport error.
		 */
		sp->error = SCG_NO_ERROR;
		sp->ux_errno = EIO;
		if (cp->SRB_TargStat)
			break;

	case SS_PENDING:		/* 0x00 SRB being processed	    */
		/*
		 * XXX Could SS_PENDING happen ???
		 */
	case SS_ABORTED:		/* 0x02 SRB aborted		    */
	case SS_ABORT_FAIL:		/* 0x03 Unable to abort SRB	    */
	default:
		sp->error = SCG_RETRYABLE;
		sp->ux_errno = EIO;
		break;

	case SS_INVALID_CMD:		/* 0x80 Invalid ASPI command	    */
	case SS_INVALID_HA:		/* 0x81 Invalid host adapter number */
	case SS_NO_DEVICE:		/* 0x82 SCSI device not installed   */

	case SS_INVALID_SRB:		/* 0xE0 Invalid parameter set in SRB */
	case SS_ILLEGAL_MODE:		/* 0xE2 Unsupported Windows mode    */
	case SS_NO_ASPI:		/* 0xE3 No ASPI managers	    */
	case SS_FAILED_INIT:		/* 0xE4 ASPI for windows failed init */
	case SS_MISMATCHED_COMPONENTS:	/* 0xE7 The DLLs/EXEs of ASPI don't */
					/*	version check		    */
	case SS_NO_ADAPTERS:		/* 0xE8 No host adapters to manager */

	case SS_ASPI_IS_SHUTDOWN:	/* 0xEA Call came to ASPI after	    */
					/*	PROCESS_DETACH		    */
	case SS_BAD_INSTALL:		/* 0xEB The DLL or other components */
					/*	are installed wrong	    */
		sp->error = SCG_FATAL;
		sp->ux_errno = EINVAL;
		break;

#ifdef	XXX
	case SS_OLD_MANAGER:		/* 0xE1 ASPI manager doesn't support */
					/*	windows			    */
#endif
	case SS_BUFFER_ALIGN:		/* 0xE1 Buffer not aligned (replaces */
					/*	SS_OLD_MANAGER in Win32)    */
		sp->error = SCG_FATAL;
		sp->ux_errno = EFAULT;
		break;

	case SS_ASPI_IS_BUSY:		/* 0xE5 No resources available to   */
					/*	execute command		    */
		sp->error = SCG_RETRYABLE;
		sp->ux_errno = EBUSY;
		break;

#ifdef	XXX
	case SS_BUFFER_TO_BIG:		/* 0xE6 Buffer size too big to handle*/
#endif
	case SS_BUFFER_TOO_BIG:		/* 0xE6 Correct spelling of 'too'   */
	case SS_INSUFFICIENT_RESOURCES:	/* 0xE9 Couldn't allocate resources */
					/*	needed to init		    */
		sp->error = SCG_RETRYABLE;
		sp->ux_errno = ENOMEM;
		break;
	}
}


struct aspi_cmd {
	SRB_ExecSCSICmd		s;
	char			pad[32];
};

LOCAL int
scgo_send(scgp)
	SCSI		*scgp;
{
	struct scg_cmd		*sp = scgp->scmd;
	DWORD			Status = 0;
	DWORD			EventStatus = WAIT_OBJECT_0;
	HANDLE			Event	 = NULL;
	struct aspi_cmd		ac;
	SRB_ExecSCSICmd		*s;

	s = &ac.s;

	/*
	 * Check if ASPI library is loaded
	 */
	if (AspiLoaded <= 0) {
		errmsgno(EX_BAD, "error in scgo_send: ASPI driver not loaded.\n");
		sp->error = SCG_FATAL;
		return (0);
	}

	if (scgp->fd < 0) {
		sp->error = SCG_FATAL;
		return (-1);
	}

	/*
	 * Initialize variables
	 */
	sp->error		= SCG_NO_ERROR;
	sp->sense_count		= 0;
	sp->u_scb.cmd_scb[0]	= 0;
	sp->resid		= 0;

	memset(&ac, 0, sizeof (ac));	/* Clear SRB structure */

	/*
	 * Check cbd_len > the maximum command pakket that can be handled by ASPI
	 */
	if (sp->cdb_len > 16) {
		sp->error = SCG_FATAL;
		sp->ux_errno = EINVAL;
		js_fprintf((FILE *)scgp->errfile,
			"sp->cdb_len > sizeof (SRB_ExecSCSICmd.CDBByte). Fatal error in scgo_send, exiting...\n");
		return (-1);
	}
	/*
	 * copy cdrecord command into SRB
	 */
	movebytes(&sp->cdb, &(s->CDBByte), sp->cdb_len);

	Event = CreateEvent(NULL, TRUE, FALSE, NULL);

	/*
	 * Fill ASPI structure
	 */
	s->SRB_Cmd	 = SC_EXEC_SCSI_CMD;		/* SCSI Command			*/
	s->SRB_HaId	 = scg_scsibus(scgp);		/* Host adapter number		*/
	s->SRB_Flags	 = SRB_EVENT_NOTIFY;		/* Flags			*/
	s->SRB_Target	 = scg_target(scgp);		/* Target SCSI ID		*/
	s->SRB_Lun	 = scg_lun(scgp);		/* Target SCSI LUN		*/
	s->SRB_BufLen	 = sp->size;			/* # of bytes transferred	*/
	s->SRB_BufPointer = sp->addr;			/* pointer to data buffer	*/
	s->SRB_CDBLen	 = sp->cdb_len;			/* SCSI command length		*/
	s->SRB_PostProc	 = Event;			/* Post proc event		*/
	if (bUsingSCSIPT)
		s->SRB_SenseLen	= SENSE_LEN_SPTI;	/* Length of sense buffer, SPTI returns SenseInfoLength */
	else
		s->SRB_SenseLen	= SENSE_LEN;		/* fixed length 14 for ASPI */
	/*
	 * Do we receive data from this ASPI command?
	 */
	if (sp->flags & SCG_RECV_DATA) {

		s->SRB_Flags |= SRB_DIR_IN;
	} else {
		/*
		 * Set direction to output
		 */
		if (sp->size > 0) {
			s->SRB_Flags |= SRB_DIR_OUT;
		}
	}

#ifdef DEBUG_WNTASPI
	/*
	 * Dump some debug information when enabled
	 */
	DebugScsiSend(scgp, s, TRUE);
/*	DebugScsiSend(scgp, s, (s->SRB_Flags&SRB_DIR_OUT) == SRB_DIR_OUT);*/
#endif

	/*
	 * ------------ Send SCSI command --------------------------
	 */

	ResetEvent(Event);			/* Clear event handle	    */
	if (bUsingSCSIPT) {
#ifdef _DEBUG_SCSIPT
		scgp_errfile = (FILE *)scgp->errfile;
#endif
		Status = SPTIExecSCSICommand(s, sp->timeout, FALSE);
	}
	else
		Status = pfnSendASPI32Command((LPSRB)s); /* Initiate SCSI command */
	if (Status == SS_PENDING) {		/* If in progress	    */
		/*
		 * Wait untill command completes, or times out.
		 */
		EventStatus = WaitForSingleObject(Event, sp->timeout*1000L);
/*		EventStatus = WaitForSingleObject(Event, 10L);*/

		if (EventStatus == WAIT_OBJECT_0)
			ResetEvent(Event);	/* Clear event, time out    */

		if (s->SRB_Status == SS_PENDING) { /* Check if we got a timeout */
			if (scgp->debug > 0) {
				js_fprintf((FILE *)scgp->errfile,
						"Timeout....\n");
			}
			scsiabort(scgp, s);
			ResetEvent(Event);	/* Clear event, time out    */
			CloseHandle(Event);	/* Close the event handle   */

			sp->error = SCG_TIMEOUT;
			return (1);		/* Return error		    */
		}
	}
	CloseHandle(Event);			/* Close the event handle   */

	/*
	 * Check ASPI command status
	 */
	if (s->SRB_Status != SS_COMP) {
		if (scgp->debug > 0) {
			js_fprintf((FILE *)scgp->errfile,
				"Error in scgo_send: s->SRB_Status is 0x%x\n", s->SRB_Status);
		}

		set_error(s, sp);		/* Set error flags	    */
		copy_sensedata(s, sp);		/* Copy sense and status    */

		if (scgp->debug > 0) {
			js_fprintf((FILE *)scgp->errfile,
				"Mapped to: error %d errno: %d\n", sp->error, sp->ux_errno);
		}
		return (1);
	}

	/*
	 * Return success
	 */
	return (0);
}

/***************************************************************************
 *									   *
 *  BOOL open_driver()							   *
 *									   *
 *  Opens the ASPI Router device driver and sets device_handle.		   *
 *  Returns:								   *
 *    TRUE - Success							   *
 *    FALSE - Unsuccessful opening of device driver			   *
 *									   *
 *  Preconditions: ASPI Router driver has be loaded			   *
 *									   *
 ***************************************************************************/
LOCAL BOOL
open_driver(scgp)
	SCSI	*scgp;
{
	DWORD	astatus;
	BYTE	HACount;
	BYTE	ASPIStatus;
	int	i;

#ifdef DEBUG_WNTASPI
	js_fprintf((FILE *)scgp->errfile, "enter open_driver\n");
#endif

	/*
	 * Check if ASPI library is already loaded yet
	 */
	if (AspiLoaded > 0) {
		AspiLoaded++;
		return (TRUE);
	}

	/*
	 * Load the ASPI library or SPTI
	 */
#ifdef _DEBUG_SCSIPT
	scgp_errfile = (FILE *)scgp->errfile;
#endif
#ifdef	PREFER_SPTI
	if (bUsingSCSIPT)
		if (InitSCSIPT() > 0) AspiLoaded++;
#endif
#ifdef	PREFER_SPTI
	if ((!bUsingSCSIPT || !bForceAccess) && AspiLoaded <= 0) {
#else
	if (!bUsingSCSIPT || !bForceAccess) {
#endif
		if (load_aspi(scgp)) {
			AspiLoaded++;
			bUsingSCSIPT = FALSE;
		}
	}

#ifndef	PREFER_SPTI
	if ((bUsingSCSIPT || !bForceAccess) && AspiLoaded <= 0)
		if (InitSCSIPT() > 0)
			AspiLoaded++;
#endif	/*PREFER_SPTI*/

	if (AspiLoaded <= 0) {
		if (bUsingSCSIPT) {
			if (errno == 0)
				errno = ENOSYS;
		}
		js_fprintf((FILE *)scgp->errfile, "Can not load %s driver! ",
						bUsingSCSIPT ? "SPTI":"ASPI");
		return (FALSE);
	}

	if (bUsingSCSIPT) {
		if (scgp->debug > 0)
			js_fprintf((FILE *)scgp->errfile, "using SPTI Transport\n");

		if (!sptiglobal.numAdapters)
			astatus = (DWORD)(MAKEWORD(0, SS_NO_ADAPTERS));
		else
			astatus = (DWORD)(MAKEWORD(sptiglobal.numAdapters, SS_COMP));
	} else {
		astatus = pfnGetASPI32SupportInfo();
	}

	ASPIStatus = HIBYTE(LOWORD(astatus));
	HACount    = LOBYTE(LOWORD(astatus));

	if (scgp->debug > 0) {
		js_fprintf((FILE *)scgp->errfile,
			"open_driver %lX HostASPIStatus=0x%x HACount=0x%x\n", astatus, ASPIStatus, HACount);
	}

	if (ASPIStatus != SS_COMP && ASPIStatus != SS_NO_ADAPTERS) {
		js_fprintf((FILE *)scgp->errfile, "Could not find any host adapters\n");
		js_fprintf((FILE *)scgp->errfile, "ASPIStatus == 0x%02X", ASPIStatus);
		return (FALSE);
	}
	busses = HACount;

#ifdef DEBUG_WNTASPI
	js_fprintf((FILE *)scgp->errfile, "open_driver HostASPIStatus=0x%x HACount=0x%x\n", ASPIStatus, HACount);
	js_fprintf((FILE *)scgp->errfile, "leaving open_driver\n");
#endif

	for (i = 0; i < busses; i++) {
		SRB_HAInquiry	s;

		ha_inquiry(scgp, i, &s);
	}

	/*
	 * Indicate that library loaded/initialized properly
	 */
	return (TRUE);
}

LOCAL BOOL
load_aspi(scgp)
	SCSI	*scgp;
{
#if	defined(__CYGWIN32__) || defined(__CYGWIN__)
	hAspiLib = dlopen("WNASPI32", RTLD_NOW);
#else
	hAspiLib = LoadLibrary("WNASPI32");
#endif
	/*
	 * Check if ASPI library is loaded correctly
	 */
	if (hAspiLib == NULL) {
#ifdef	not_done_later
		js_fprintf((FILE *)scgp->errfile, "Can not load ASPI driver! ");
#endif
		return (FALSE);
	}

	/*
	 * Get a pointer to GetASPI32SupportInfo function
	 * and a pointer to SendASPI32Command function
	 */
#if	defined(__CYGWIN32__) || defined(__CYGWIN__)
	pfnGetASPI32SupportInfo = (DWORD(*)(void))dlsym(hAspiLib, "GetASPI32SupportInfo");
	pfnSendASPI32Command = (DWORD(*)(LPSRB))dlsym(hAspiLib, "SendASPI32Command");
#else
	pfnGetASPI32SupportInfo = (DWORD(*)(void))GetProcAddress(hAspiLib, "GetASPI32SupportInfo");
	pfnSendASPI32Command = (DWORD(*)(LPSRB))GetProcAddress(hAspiLib, "SendASPI32Command");
#endif

	if ((pfnGetASPI32SupportInfo == NULL) || (pfnSendASPI32Command == NULL)) {
		js_fprintf((FILE *)scgp->errfile,
				"ASPI function not found in library! ");
		return (FALSE);
	}

	/*
	 * The following functions are currently not used by libscg.
	 * If we start to use them, we need to check whether the founctions
	 * could be found in the ASPI library that just has been loaded.
	 */
#if	defined(__CYGWIN32__) || defined(__CYGWIN__)
	pfnGetASPI32Buffer = (BOOL(*)(PASPI32BUFF))dlsym(hAspiLib, "GetASPI32Buffer");
	pfnFreeASPI32Buffer = (BOOL(*)(PASPI32BUFF))dlsym(hAspiLib, "FreeASPI32Buffer");
	pfnTranslateASPI32Address = (BOOL(*)(PDWORD, PDWORD))dlsym(hAspiLib, "TranslateASPI32Address");
#else
	pfnGetASPI32Buffer = (BOOL(*)(PASPI32BUFF))GetProcAddress(hAspiLib, "GetASPI32Buffer");
	pfnFreeASPI32Buffer = (BOOL(*)(PASPI32BUFF))GetProcAddress(hAspiLib, "FreeASPI32Buffer");
	pfnTranslateASPI32Address = (BOOL(*)(PDWORD, PDWORD))GetProcAddress(hAspiLib, "TranslateASPI32Address");
#endif
	return (TRUE);
}

/***************************************************************************
 *									   *
 *  BOOL close_driver()							   *
 *									   *
 *  Closes the device driver						   *
 *  Returns:								   *
 *    TRUE - Success							   *
 *    FALSE - Unsuccessful closing of device driver			   *
 *									   *
 *  Preconditions: ASPI Router driver has be opened with open_driver	   *
 *									   *
 ***************************************************************************/
LOCAL BOOL
close_driver()
{
	if (--AspiLoaded > 0)
		return (TRUE);
	/*
	 * If library is loaded
	 */
	DeinitSCSIPT();
	/*
	 * Clear all variables
	 */
	if (hAspiLib) {
		pfnGetASPI32SupportInfo	= NULL;
		pfnSendASPI32Command	= NULL;
		pfnGetASPI32Buffer	= NULL;
		pfnFreeASPI32Buffer	= NULL;
		pfnTranslateASPI32Address = NULL;

		/*
		 * Free ASPI library, we do not need it any longer
		 */
#if	defined(__CYGWIN32__) || defined(__CYGWIN__)
		dlclose(hAspiLib);
#else
		FreeLibrary(hAspiLib);
#endif
		hAspiLib = NULL;
	}

	/*
	 * Indicate that shutdown has been finished properly
	 */
	return (TRUE);
}

LOCAL int
ha_inquiry(scgp, id, ip)
	SCSI		*scgp;
	int		id;
	SRB_HAInquiry	*ip;
{
	DWORD		Status;

	ip->SRB_Cmd	 = SC_HA_INQUIRY;
	ip->SRB_HaId	 = id;
	ip->SRB_Flags	 = 0;
	ip->SRB_Hdr_Rsvd = 0;

	if (bUsingSCSIPT)
		Status = SPTIHandleHaInquiry(ip);
	else
		Status = pfnSendASPI32Command((LPSRB)ip);

	if (scgp->debug > 0) {
		js_fprintf((FILE *)scgp->errfile, "Status : %ld\n",	Status);
		js_fprintf((FILE *)scgp->errfile, "hacount: %d\n", ip->HA_Count);
		js_fprintf((FILE *)scgp->errfile, "SCSI id: %d\n", ip->HA_SCSI_ID);
		js_fprintf((FILE *)scgp->errfile, "Manager: '%.16s'\n", ip->HA_ManagerId);
		js_fprintf((FILE *)scgp->errfile, "Identif: '%.16s'\n", ip->HA_Identifier);
		scg_prbytes("Unique:", ip->HA_Unique, 16);
	}
	if (ip->SRB_Status != SS_COMP)
		return (-1);
	return (0);
}

#ifdef	__USED__
LOCAL int
resetSCSIBus(scgp)
	SCSI	*scgp;
{
	DWORD			Status;
	HANDLE			Event;
	SRB_BusDeviceReset	s;

	if (bUsingSCSIPT) {
		js_fprintf((FILE *)scgp->errfile,
					"Reset SCSI bus not implemented with SPTI\n");
		return (FALSE);
	}

	js_fprintf((FILE *)scgp->errfile, "Attempting to reset SCSI bus\n");

	Event = CreateEvent(NULL, TRUE, FALSE, NULL);

	memset(&s, 0, sizeof (s));	/* Clear SRB_BesDeviceReset structure */

	/*
	 * Set structure variables
	 */
	s.SRB_Cmd = SC_RESET_DEV;
	s.SRB_PostProc = (LPVOID)Event;

	/*
	 * Clear event
	 */
	ResetEvent(Event);

	/*
	 * Initiate SCSI command
	 */
	Status = pfnSendASPI32Command((LPSRB)&s);

	/*
	 * Check status
	 */
	if (Status == SS_PENDING) {
		/*
		 * Wait till command completes
		 */
		WaitForSingleObject(Event, INFINITE);
	}

	/*
	 * Close the event handle
	 */
	CloseHandle(Event);

	/*
	 * Check condition
	 */
	if (s.SRB_Status != SS_COMP) {
		js_fprintf((FILE *)scgp->errfile, "ERROR  0x%08X\n", s.SRB_Status);

		/*
		 * Indicate that error has occured
		 */
		return (FALSE);
	}

	/*
	 * Everything went OK
	 */
	return (TRUE);
}
#endif	/* __USED__ */

LOCAL int
scsiabort(scgp, sp)
	SCSI		*scgp;
	SRB_ExecSCSICmd	*sp;
{
	DWORD			Status = 0;
	SRB_Abort		s;

	if (bUsingSCSIPT) {
		js_fprintf((FILE *)scgp->errfile,
					"Abort SCSI not implemented with SPTI\n");
		return (FALSE);
	}

	if (scgp->debug > 0) {
		js_fprintf((FILE *)scgp->errfile,
				"Attempting to abort SCSI command\n");
	}

	/*
	 * Check if ASPI library is loaded
	 */
	if (AspiLoaded <= 0) {
		js_fprintf((FILE *)scgp->errfile,
				"error in scsiabort: ASPI driver not loaded !\n");
		return (FALSE);
	}

	/*
	 * Set structure variables
	 */
	s.SRB_Cmd	= SC_ABORT_SRB;			/* ASPI command code = SC_ABORT_SRB	*/
	s.SRB_HaId	= scg_scsibus(scgp);		/* ASPI host adapter number		*/
	s.SRB_Flags	= 0;				/* Flags				*/
	s.SRB_ToAbort	= (LPSRB)&sp;			/* sp					*/

	/*
	 * Initiate SCSI abort
	 */
	Status = pfnSendASPI32Command((LPSRB)&s);

	/*
	 * Check condition
	 */
	if (s.SRB_Status != SS_COMP) {
		js_fprintf((FILE *)scgp->errfile, "Abort ERROR! 0x%08X\n", s.SRB_Status);

		/*
		 * Indicate that error has occured
		 */
		return (FALSE);
	}

	if (scgp->debug > 0)
		js_fprintf((FILE *)scgp->errfile, "Abort SCSI command completed\n");

	/*
	 * Everything went OK
	 */
	return (TRUE);
}
