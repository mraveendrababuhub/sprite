/* 
 * devConfig.c --
 *
 *	Configuration table for the devices in the system.  There is
 *	a table for the possible controllers in the system, and
 *	then a table for devices.  Devices are implicitly associated
 *	with a controller.  This file should be automatically generated
 *	by a config program, but it isn't.
 *
 * Copyright 1989 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#ifndef lint
static char rcsid[] = "$Header: /sprite/src/kernel/dev/sun4.md/RCS/devConfig.c,v 9.6 90/10/19 15:40:19 mendel Exp $ SPRITE (Berkeley)";
#endif not lint


#include "sprite.h"
#include "devInt.h"
#include "scsiHBA.h"
#include "devTypes.h"

/*
 * Per device include files.
 */
#include "scsi3.h"
#include "xylogics450.h"
#include "jaguar.h"
#include "devTMR.h"

/*
 * The controller configuration table.
 */
DevConfigController devCntrlr[] = {
   /* Name     Address,  Addr space, ID, InitProc   IntrVector  IntrRoutine. */
    { "SCSI3",  0x200000, DEV_VME_D16A24, 0, DevSCSI3Init, 64, DevSCSI3Intr },
    { "SCSI3#1",  0x210000, DEV_VME_D16A24, 1, DevSCSI3Init, 65, DevSCSI3Intr },
    { "SCSI3#2",  0x220000, DEV_VME_D16A24, 2, DevSCSI3Init, 66, DevSCSI3Intr },
    { "SCSI3#3",  0x230000, DEV_VME_D16A24, 3, DevSCSI3Init, 67, DevSCSI3Intr },
    { "Xylogics450", 0xee40, DEV_VME_D16A16,	 0, DevXylogics450Init,
				    72, DevXylogics450Intr},
    { "Jaguar0", 0x8800, DEV_VME_D16A16, 0, DevJaguarInit, 211, DevJaguarIntr},
    { "Jaguar1", 0x9000, DEV_VME_D16A16, 1, DevJaguarInit, 215, DevJaguarIntr},
    { "Jaguar2", 0x9800, DEV_VME_D16A16, 2, DevJaguarInit, 216, DevJaguarIntr},
    { "Jaguar3", 0xc000, DEV_VME_D16A16, 3, DevJaguarInit, 217, DevJaguarIntr},
    { "tmr0", 0xFFD14000, DEV_OBIO, 0, Dev_TimerProbe, 0, ((Boolean (*)())0)},
};
int devNumConfigCntrlrs = sizeof(devCntrlr) / sizeof(DevConfigController);

/*
 * Table of SCSI HBA types attached to this system.
 */

ScsiDevice *((*devScsiAttachProcs[]) _ARGS_((Fs_Device *devicePtr,
		void (*insertProc) (List_Links *elementPtr,
                                    List_Links *elementListHdrPtr)))) = {
    DevSCSI3AttachDevice,		/* SCSI Controller type 0. */
    DevNoHBAAttachDevice,		/* SCSI Controller type 1. */
    DevJaguarAttachDevice,		/* SCSI Controller type 2. */
};
int devScsiNumHBATypes = sizeof(devScsiAttachProcs) / 
			 sizeof(devScsiAttachProcs[0]);

/*
 * A list of disk devices that is used when probing for a root partition.
 * The choices are:
 * Drive 0 partition 0 of xylogics 450 controller 0.
 * SCSI Disk target ID 0 LUN 0 partition 0 on SCSI3 HBA 0. 
 * SCSI Disk target ID 0 LUN 0 partition 0 on SCSI3 HBA 1. 
 */
Fs_Device devFsDefaultDiskPartitions[] = { 
    { -1, DEV_XYLOGICS, 0, (ClientData) NIL },	
    { -1, SCSI_MAKE_DEVICE_TYPE(DEV_SCSI_DISK, DEV_SCSI3_HBA, 0, 0, 0, 0),
	  SCSI_MAKE_DEVICE_UNIT(DEV_SCSI_DISK, DEV_SCSI3_HBA, 0, 0, 0, 0),
		(ClientData) NIL }, 
    { -1, SCSI_MAKE_DEVICE_TYPE(DEV_SCSI_DISK, DEV_SCSI3_HBA, 0, 0, 0, 2),
	  SCSI_MAKE_DEVICE_UNIT(DEV_SCSI_DISK, DEV_SCSI3_HBA, 0, 0, 0, 2),
		(ClientData) NIL }, 
    { -1, SCSI_MAKE_DEVICE_TYPE(DEV_SCSI_DISK, DEV_SCSI3_HBA, 1, 0, 0, 0),
	  SCSI_MAKE_DEVICE_UNIT(DEV_SCSI_DISK, DEV_SCSI3_HBA, 1, 0, 0, 0),
		(ClientData) NIL }, 
    { -1, SCSI_MAKE_DEVICE_TYPE(DEV_SCSI_DISK, DEV_SCSI3_HBA, 1, 0, 0, 2),
	  SCSI_MAKE_DEVICE_UNIT(DEV_SCSI_DISK, DEV_SCSI3_HBA, 1, 0, 0, 2),
		(ClientData) NIL }, 
    { -1, SCSI_MAKE_DEVICE_TYPE(DEV_SCSI_DISK, DEV_SCSI3_HBA, 1, 4, 0, 0),
	  SCSI_MAKE_DEVICE_UNIT(DEV_SCSI_DISK, DEV_SCSI3_HBA, 1, 4, 0, 0),
		(ClientData) NIL }, 
    { -1, SCSI_MAKE_DEVICE_TYPE(DEV_SCSI_DISK, DEV_SCSI3_HBA, 1, 4, 0, 2),
	  SCSI_MAKE_DEVICE_UNIT(DEV_SCSI_DISK, DEV_SCSI3_HBA, 1, 4, 0, 2),
		(ClientData) NIL }, 
    };
int devNumDefaultDiskPartitions = sizeof(devFsDefaultDiskPartitions) / 
			  sizeof(Fs_Device);
