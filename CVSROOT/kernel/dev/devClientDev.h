/*
 * devClientDev.h --
 *
 *	Declarations for the device used to monitor client state.
 *
 *
 * Copyright 1989 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * $Header$ SPRITE (Berkeley)
 */

#ifndef _DEVCLIENTDEV
#define _DEVCLIENTDEV

/* procedures */
extern ReturnStatus DevClientStateOpen _ARGS_((Fs_Device *devicePtr,
	int useFlags, Fs_NotifyToken data, int *flagsPtr));
extern ReturnStatus DevClientStateClose _ARGS_((Fs_Device *devicePtr,
	int useFlags, int openCount, int writerCount));
extern ReturnStatus DevClientStateRead _ARGS_((Fs_Device *devicePtr,
	Fs_IOParam *readPtr, Fs_IOReply *replyPtr));
extern ReturnStatus DevClientStateIOControl _ARGS_((Fs_Device *devicePtr,
	Fs_IOCParam *ioctlPtr, Fs_IOReply *replyPtr));
extern void Dev_ClientHostUp _ARGS_((int spriteID));
extern void Dev_ClientHostDown _ARGS_((int spriteID));
extern void Dev_ClientStateWakeRecovery _ARGS_((void));
    extern ReturnStatus DevClientStateWrite _ARGS_((Fs_Device *devicePtr,
	    Fs_IOParam *writePtr, Fs_IOReply *replyPtr));

#endif /* _DEVCLIENTDEV */
