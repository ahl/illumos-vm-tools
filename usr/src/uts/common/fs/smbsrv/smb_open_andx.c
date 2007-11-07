/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <smbsrv/smb_vops.h>

/*
 * This module provides the common open functionality to the various
 * open and create SMB interface functions.
 */

/*
 *
 *  Client Request                     Description
 *  ================================== =================================
 *
 *  UCHAR WordCount;                   Count of parameter words = 15
 *  UCHAR AndXCommand;                 Secondary (X) command;  0xFF =
 *                                      none
 *  UCHAR AndXReserved;                Reserved (must be 0)
 *  USHORT AndXOffset;                 Offset to next command WordCount
 *  USHORT Flags;                      Additional information: bit set-
 *                                      0 - return additional info
 *                                      1 - exclusive oplock requested
 *                                      2 - batch oplock requested
 *  USHORT DesiredAccess;              File open mode
 *  USHORT SearchAttributes;
 *  USHORT FileAttributes;
 *  UTIME CreationTime;                Creation timestamp for file if it
 *                                      gets created
 *  USHORT OpenFunction;               Action to take if file exists
 *  ULONG AllocationSize;              Bytes to reserve on create or
 *                                      truncate
 *  ULONG Reserved[2];                 Must be 0
 *  USHORT ByteCount;                  Count of data bytes;    min = 1
 *  UCHAR BufferFormat                 0x04
 *  STRING FileName;
 *
 *  Server Response                    Description
 *  ================================== =================================
 *
 *  UCHAR WordCount;                   Count of parameter words = 15
 *  UCHAR AndXCommand;                 Secondary (X) command;  0xFF =
 *                                      none
 *  UCHAR AndXReserved;                Reserved (must be 0)
 *  USHORT AndXOffset;                 Offset to next command WordCount
 *  USHORT Fid;                        File handle
 *  USHORT FileAttributes;
 *  UTIME LastWriteTime;
 *  ULONG DataSize;                    Current file size
 *  USHORT GrantedAccess;              Access permissions actually
 *                                      allowed
 *  USHORT FileType;                   Type of file opened
 *  USHORT DeviceState;                State of the named pipe
 *  USHORT Action;                     Action taken
 *  ULONG ServerFid;                   Server unique file id
 *  USHORT Reserved;                   Reserved (must be 0)
 *  USHORT ByteCount;                  Count of data bytes = 0
 *
 * DesiredAccess describes the access the client desires for the file (see
 * section 3.6 -  Access Mode Encoding).
 *
 * OpenFunction specifies the action to be taken depending on whether or
 * not the file exists (see section 3.8 -  Open Function Encoding).  Action
 *
 * in the response specifies the action as a result of the Open request
 * (see section 3.9 -  Open Action Encoding).
 *
 * SearchAttributes indicates the attributes that the file must have to be
 * found while searching to see if it exists.  The encoding of this field
 * is described in the "File Attribute Encoding" section elsewhere in this
 * document.  If SearchAttributes is zero then only normal files are
 * returned.  If the system file, hidden or directory attributes are
 * specified then the search is inclusive -- both the specified type(s) of
 * files and normal files are returned.
 *
 * FileType returns the kind of resource actually opened:
 *
 *  Name                       Value  Description
 *  ========================== ====== ==================================
 *
 *  FileTypeDisk               0      Disk file or directory as defined
 *                                     in the attribute field
 *  FileTypeByteModePipe       1      Named pipe in byte mode
 *  FileTypeMessageModePipe    2      Named pipe in message mode
 *  FileTypePrinter            3      Spooled printer
 *  FileTypeUnknown            0xFFFF Unrecognized resource type
 *
 * If bit0 of Flags is clear, the FileAttributes, LastWriteTime, DataSize,
 * FileType, and DeviceState have indeterminate values in the response.
 *
 * This SMB can request an oplock on the opened file.  Oplocks are fully
 * described in the "Oplocks" section elsewhere in this document, and there
 * is also discussion of oplocks in the SMB_COM_LOCKING_ANDX SMB
 * description.  Bit1 and bit2 of the Flags field are used to request
 * oplocks during open.
 *
 * The following SMBs may follow SMB_COM_OPEN_ANDX:
 *
 *    SMB_COM_READ    SMB_COM_READ_ANDX
 *    SMB_COM_IOCTL
 */

#include <smbsrv/smb_incl.h>

/*
 * SMB: open
 *
 * This message is sent to obtain a file handle for a data file.  This
 * returned Fid is used in subsequent client requests such as read, write,
 * close, etc.
 *
 * Client Request                     Description
 * ================================== =================================
 *
 * UCHAR WordCount;                   Count of parameter words = 2
 * USHORT DesiredAccess;              Mode - read/write/share
 * USHORT SearchAttributes;
 * USHORT ByteCount;                  Count of data bytes;    min = 2
 * UCHAR BufferFormat;                0x04
 * STRING FileName[];                 File name
 *
 * FileName is the fully qualified file name, relative to the root of the
 * share specified in the Tid field of the SMB header.  If Tid in the SMB
 * header refers to a print share, this SMB creates a new file which will
 * be spooled to the printer when closed.  In this case, FileName is
 * ignored.
 *
 * SearchAttributes specifies the type of file desired.  The encoding is
 * described in the "File Attribute Encoding" section.
 *
 * DesiredAccess controls the mode under which the file is opened, and the
 * file will be opened only if the client has the appropriate permissions.
 * The encoding of DesiredAccess is discussed in the section entitled
 * "Access Mode Encoding".
 *
 * Server Response                    Description
 * ================================== =================================
 *
 * UCHAR WordCount;                   Count of parameter words = 7
 * USHORT Fid;                        File handle
 * USHORT FileAttributes;             Attributes of opened file
 * UTIME LastWriteTime;               Time file was last written
 * ULONG DataSize;                    File size
 * USHORT GrantedAccess;              Access allowed
 * USHORT ByteCount;                  Count of data bytes = 0
 *
 * Fid is the handle value which should be used for subsequent file
 * operations.
 *
 * FileAttributes specifies the type of file obtained.  The encoding is
 * described in the "File Attribute Encoding" section.
 *
 * GrantedAccess indicates the access permissions actually allowed, and may
 * have one of the following values:
 *
 *    0  read-only
 *    1  write-only
 *    2 read/write
 *
 * File Handles (Fids) are scoped per client.  A Pid may reference any Fid
 * established by itself or any other Pid on the client (so far as the
 * server is concerned).  The actual accesses allowed through the Fid
 * depends on the open and deny modes specified when the file was opened
 * (see below).
 *
 * The MS-DOS compatibility mode of file open provides exclusion at the
 * client level.  A file open in compatibility mode may be opened (also in
 * compatibility mode) any number of times for any combination of reading
 * and writing (subject to the user's permissions) by any Pid on the same
 * client.  If the first client has the file open for writing, then the
 * file may not be opened in any way by any other client.  If the first
 * client has the file open only for reading, then other clients may open
 * the file, in compatibility mode, for reading..  The above
 * notwithstanding, if the filename has an extension of .EXE, .DLL, .SYM,
 * or .COM other clients are permitted to open the file regardless of
 * read/write open modes of other compatibility mode opens.  However, once
 * multiple clients have the file open for reading, no client is permitted
 * to open the file for writing and no other client may open the file in
 * any mode other than compatibility mode.
 *
 * The other file exclusion modes (Deny read/write, Deny write, Deny read,
 * Deny none) provide exclusion at the file level.  A file opened in any
 * "Deny" mode may be opened again only for the accesses allowed by the
 * Deny mode (subject to the user's permissions).  This is true regardless
 * of the identity of the second opener -a different client, a Pid from the
 * same client, or the Pid that already has the file open.  For example, if
 * a file is open in "Deny write" mode a second open may only obtain read
 * permission to the file.
 *
 * Although Fids are available to all Pids on a client, Pids other than the
 * owner may not have the full access rights specified in the open mode by
 * the Fid's creator.  If the open creating the Fid specified a deny mode,
 * then any Pid using the Fid, other than the creating Pid, will have only
 * those access rights determined by "anding" the open mode rights and the
 * deny mode rights, i.e., the deny mode is checked on all file accesses.
 * For example, if a file is opened for Read/Write in Deny write mode, then
 * other clients may only read the file and cannot write; if a file is
 * opened for Read in Deny read mode, then the other clients can neither
 * read nor write the file.
 */

int
smb_com_open(struct smb_request *sr)
{
	struct open_param *op = &sr->arg.open;
	uint16_t file_attr;
	DWORD status;

	bzero(op, sizeof (sr->arg.open));
	if (smbsr_decode_vwv(sr, "ww", &op->omode, &op->fqi.srch_attr) != 0) {
		smbsr_decode_error(sr);
		/* NOTREACHED */
	}

	if (smbsr_decode_data(sr, "%S", sr, &op->fqi.path) != 0) {
		smbsr_decode_error(sr);
		/* NOTREACHED */
	}
	op->desired_access = smb_omode_to_amask(op->omode);
	op->share_access = smb_denymode_to_sharemode(op->omode, op->fqi.path);

	if ((op->desired_access == ((uint32_t)SMB_INVALID_AMASK)) ||
	    (op->share_access == ((uint32_t)SMB_INVALID_SHAREMODE))) {
		smbsr_raise_cifs_error(sr, NT_STATUS_INVALID_PARAMETER,
		    ERRDOS, ERROR_INVALID_PARAMETER);
		/* NOTREACHED */
	}

	op->dsize = 0; /* Don't set spurious size */
	op->utime.tv_sec = op->utime.tv_nsec = 0;
	op->create_disposition = FILE_OPEN;
	op->create_options = (op->omode & SMB_DA_WRITE_THROUGH)
	    ? FILE_WRITE_THROUGH : 0;

	if (sr->smb_flg & SMB_FLAGS_OPLOCK) {
		if (sr->smb_flg & SMB_FLAGS_OPLOCK_NOTIFY_ANY) {
			op->my_flags = MYF_BATCH_OPLOCK;
		} else {
			op->my_flags = MYF_EXCLUSIVE_OPLOCK;
		}
	}

	if ((status = smb_open_subr(sr)) != NT_STATUS_SUCCESS) {
		if (status == NT_STATUS_SHARING_VIOLATION)
			smbsr_raise_cifs_error(sr,
			    NT_STATUS_SHARING_VIOLATION,
			    ERRDOS, ERROR_SHARING_VIOLATION);
		else
			smbsr_raise_nt_error(sr, status);

		/* NOTREACHED */
	}

	if (MYF_OPLOCK_TYPE(op->my_flags) == MYF_OPLOCK_NONE) {
		sr->smb_flg &=
		    ~(SMB_FLAGS_OPLOCK | SMB_FLAGS_OPLOCK_NOTIFY_ANY);
	}

	if (op->dsize > 0xffffffff)
		smbsr_raise_error(sr, ERRDOS, ERRbadfunc);

	file_attr = op->dattr  & FILE_ATTRIBUTE_MASK;

	smbsr_encode_result(sr, 7, 0, "bwwllww",
	    7,
	    sr->smb_fid,
	    file_attr,
	    smb_gmt_to_local_time(op->utime.tv_sec),
	    (uint32_t)op->dsize,
	    op->omode & SMB_DA_ACCESS_MASK,
	    (uint16_t)0);	/* bcc */

	return (SDRC_NORMAL_REPLY);
}

int
smb_com_open_andx(struct smb_request *sr)
{
	struct open_param	*op = &sr->arg.open;
	uint16_t		flags;
	uint32_t		CreationTime;
	uint16_t		granted_access;
	uint16_t		ofun;
	uint16_t		file_attr;
	int count;
	DWORD status;
	int rc;

	bzero(op, sizeof (sr->arg.open));
	op->dsize = 0;
	rc = smbsr_decode_vwv(sr, "b.wwwwwlwll4.", &sr->andx_com,
	    &sr->andx_off, &flags, &op->omode, &op->fqi.srch_attr,
	    &file_attr, &CreationTime, &ofun, &op->dsize, &op->timeo);
	if (rc != 0) {
		smbsr_decode_error(sr);
		/* NOTREACHED */
	}

	if (smbsr_decode_data(sr, "%u", sr, &op->fqi.path) != 0) {
		smbsr_decode_error(sr);
		/* NOTREACHED */
	}

	op->desired_access = smb_omode_to_amask(op->omode);
	op->share_access = smb_denymode_to_sharemode(op->omode, op->fqi.path);

	if ((op->desired_access == ((uint32_t)SMB_INVALID_AMASK)) ||
	    (op->share_access == ((uint32_t)SMB_INVALID_SHAREMODE))) {
		smbsr_raise_cifs_error(sr, NT_STATUS_INVALID_PARAMETER,
		    ERRDOS, ERROR_INVALID_PARAMETER);
		/* NOTREACHED */
	}

	op->dattr = file_attr;
	op->create_disposition = smb_ofun_to_crdisposition(ofun);
	if (op->create_disposition == ((uint32_t)SMB_INVALID_CRDISPOSITION)) {
		smbsr_raise_error(sr, ERRDOS, ERROR_INVALID_PARAMETER);
		/* NOTREACHED */
	}

	op->create_options = (op->omode & SMB_DA_WRITE_THROUGH)
	    ? FILE_WRITE_THROUGH : 0;

	if (flags & 2)
		op->my_flags = MYF_EXCLUSIVE_OPLOCK;
	else if (flags & 4)
		op->my_flags = MYF_BATCH_OPLOCK;

	if ((CreationTime != 0) && (CreationTime != 0xffffffff))
		op->utime.tv_sec = smb_local_time_to_gmt(CreationTime);
	op->utime.tv_nsec = 0;

	status = NT_STATUS_SUCCESS;
	/*
	 * According to NT, when exclusive share access failed,
	 * instead of raising "access deny" error immediately,
	 * we should wait for the client holding the exclusive
	 * file to close the file. If the wait timed out, we
	 * report a sharing violation; otherwise, we grant access.
	 * smb_open_subr returns NT_STATUS_SHARING_VIOLATION when
	 * it encounters an exclusive share access deny: we wait
	 * and retry.
	 */
	for (count = 0; count <= 4; count++) {
		if (count) {
			delay(MSEC_TO_TICK(400));
		}

		if ((status = smb_open_subr(sr)) == NT_STATUS_SUCCESS)
			break;
	}

	if (status != NT_STATUS_SUCCESS) {
		if (status == NT_STATUS_SHARING_VIOLATION)
			smbsr_raise_cifs_error(sr,
			    NT_STATUS_SHARING_VIOLATION,
			    ERRDOS, ERROR_SHARING_VIOLATION);
		else
			smbsr_raise_nt_error(sr, status);

		/* NOTREACHED */
	}

	if (op->dsize > 0xffffffff)
		smbsr_raise_error(sr, ERRDOS, ERRbadfunc);

	if (MYF_OPLOCK_TYPE(op->my_flags) != MYF_OPLOCK_NONE) {
		op->action_taken |= SMB_OACT_LOCK;
	} else {
		op->action_taken &= ~SMB_OACT_LOCK;
	}

	granted_access = (sr->tid_tree->t_access == SMB_TREE_READ_ONLY)
	    ? SMB_DA_ACCESS_READ : op->omode & SMB_DA_ACCESS_MASK;

	file_attr = op->dattr & FILE_ATTRIBUTE_MASK;
	if (STYPE_ISDSK(sr->tid_tree->t_res_type)) {
		smb_node_t *node = sr->fid_ofile->f_node;
		smbsr_encode_result(sr, 15, 0,
		    "b b.w w wll www wl 2. w",
		    15,
		    sr->andx_com, VAR_BCC,
		    sr->smb_fid,
		    file_attr,
		    smb_gmt_to_local_time(node->attr.sa_vattr.va_mtime.tv_sec),
		    (uint32_t)op->dsize,
		    granted_access, op->ftype,
		    op->devstate,
		    op->action_taken, op->fileid,
		    0);
	} else {
		smbsr_encode_result(sr, 15, 0,
		    "b b.w w wll www wl 2. w",
		    15,
		    sr->andx_com, VAR_BCC,
		    sr->smb_fid,
		    file_attr,
		    0L,
		    0L,
		    granted_access, op->ftype,
		    op->devstate,
		    op->action_taken, op->fileid,
		    0);
	}

	return (SDRC_NORMAL_REPLY);
}