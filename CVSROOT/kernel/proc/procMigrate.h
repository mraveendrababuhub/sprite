/*
 * procMigrate.h --
 *
 *	Declarations of procedures and constants for process migration. 
 *
 * Copyright 1986, 1988 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 *
 * $ProcMigrate: proto.h,v 1.4 86/03/20 14:00:21 andrew Exp $ SPRITE (Berkeley)
 */

#ifndef _PROCMIGRATE
#define _PROCMIGRATE

#include "proc.h"
#include "trace.h"
#include "sys.h"

/*
 * Define a macro to get a valid PCB for a migrated process.  This gets the
 * PCB corresponding to the process ID, and if it is a valid PCB the macro
 * then checks to make sure the process is migrated and from the specified
 * host.
 */

#define PROC_GET_MIG_PCB(pid, procPtr, hostID) 		    	\
	procPtr = Proc_LockPID(pid);	 			\
	if (procPtr != (Proc_ControlBlock *) NIL && 		\
	        (!(procPtr->genFlags & PROC_FOREIGN) || 	\
	        procPtr->peerHostID != remoteHostID)) {		\
	    Proc_Unlock(procPtr);				\
 	    procPtr = (Proc_ControlBlock *) NIL; 		\
	}

/*
 * Structure to contain information for the arguments to a system call
 * for a migrated process.    The size is the size of the argument passed
 * to or from the other node.  The disposition is SYS_PARAM_IN and/or
 * SYS_PARAM_OUT.
 */

typedef struct {
    int size;
    int disposition;
} Proc_ParamInfo;

/*
 * Define a simple buffer for passing both buffer size and pointer in
 * a single argument.  (This simplifies the case when the buffer is NIL).
 */

typedef struct {
    int size;
    Address ptr;
} Proc_MigBuffer;

/* 
 * Generic information sent when migrating a system call back to the
 * home machine.  The processID transferred is the ID of the process
 * on the machine servicing the RPC.
 */
	
typedef struct {
    Proc_PID			processID;
    int			 	callNumber;
    Boolean			parseArgs;
    int				numArgs;
    int				replySize;
    Proc_ParamInfo		info[SYS_MAX_ARGS];
} Proc_RemoteCall;

/*
 * Declare variables and constants for instrumentation.  First,
 * declare variables for the trace package.  These are followed by
 * structures that are passed into the trace package.  Each trace
 * record contains the process ID of the process being operated upon,
 * whether the operation is done for a process on its home node or a
 * remote one, and either a system call number and ReturnStatus or a
 * migration meta-command such as transferring state.
 */

extern Trace_Header proc_TraceHeader;
extern Trace_Header *proc_TraceHdrPtr;
#define PROC_NUM_TRACE_RECS 500

/*
 * "Events" for the trace package.
 *
 *	PROC_MIGTRACE_BEGIN_MIG 	- starting to transfer a process
 *	PROC_MIGTRACE_END_MIG 		- completed transferring a process
 *	PROC_MIGTRACE_COMMAND  	- a particular transfer operation
 *	PROC_MIGTRACE_CALL		- a migrated system call
 *	
 */

#define PROC_MIGTRACE_BEGIN_MIG 	0
#define PROC_MIGTRACE_END_MIG 		1
#define PROC_MIGTRACE_COMMAND  	2
#define PROC_MIGTRACE_CALL		3
#define PROC_MIGTRACE_MIGTRAP		4

typedef struct {
    int callNumber;
    ReturnStatus status;
} Proc_SysCallTrace;

typedef struct {
    int type;
    ClientData data;
} Proc_CommandTrace;


typedef struct {
    Proc_PID processID;
    int flags;
    union {
	Proc_SysCallTrace call;
	Proc_CommandTrace command;
	int filler;
    } info;
} Proc_TraceRecord;
	
/*
 * Flags for Proc_TraceRecords:
 *
 * 	PROC_MIGTRACE_START - start of an RPC
 *	PROC_MIGTRACE_HOME  - operation is for a process on its home machine
 *
 * Both of these flags are boolean, so absence of a flag implies its
 * opposite (end of an RPC, or that the operation is for a foreign process).
 */

#define PROC_MIGTRACE_START	0x01
#define PROC_MIGTRACE_HOME	0x02

/*
 * Define a structure for passing information via callback for killing
 * a migrated process.  [Not used yet, but potentially.]
 */
typedef struct Proc_DestroyMigProcData {
    Proc_ControlBlock *procPtr;		/* local copy of process to kill */
    ReturnStatus status;		/* status to return when it exits */
} Proc_DestroyMigProcData;

/*
 * Define the state of this machine w.r.t accepting migrated processes.
 * A machine must always be willing to accept its own processes if they
 * are migrated home.  Other than that, a host may allow migrations onto
 * it under various sets of criteria, and may allow migrations away from
 * it under similar sets of criteria.
 *
 *	PROC_MIG_IMPORT_NEVER		- never allow migrations to this host.
 *	PROC_MIG_IMPORT_ROOT 		- allow migrations to this host only
 *					  by root.
 *	PROC_MIG_IMPORT_ALL  		- allow migrations by anyone.
 *	PROC_MIG_IMPORT_ANYINPUT 	- don't check keyboard input when
 *					  determining availability.
 *	PROC_MIG_IMPORT_ANYLOAD  	- don't check load average when
 *					  determining availability.
 *	PROC_MIG_IMPORT_ALWAYS  	- don't check either.
 *	PROC_MIG_EXPORT_NEVER    	- never export migrations from this
 * 					  host.
 *	PROC_MIG_EXPORT_ROOT	        - allow only root to export.
 *	PROC_MIG_EXPORT_ALL	        - allow anyone to export.
 *
 * For example, a reasonable default for a file server might be to import
 * and export only for root; for a user's machine, it might be to allow
 * anyone to migrate; and for a compute server, it might never export
 * and import always regardless of load average or keyboard input.  (The
 * load average would not have to be exceptionally low to determine
 * availability; the host still would only be selected if the load average
 * were low enough to gain something by migrating to it.)
 */

#define PROC_MIG_IMPORT_NEVER 			 0
#define PROC_MIG_IMPORT_ROOT    	0x00000001
#define PROC_MIG_IMPORT_ALL     	0x00000003
#define PROC_MIG_IMPORT_ANYINPUT	0x00000010
#define PROC_MIG_IMPORT_ANYLOAD		0x00000020
#define PROC_MIG_IMPORT_ALWAYS  \
			(PROC_MIG_IMPORT_ANYINPUT | PROC_MIG_IMPORT_ANYLOAD)
#define PROC_MIG_EXPORT_NEVER			 0
#define PROC_MIG_EXPORT_ROOT		0x00010000
#define PROC_MIG_EXPORT_ALL		0x00030000

#define PROC_MIG_ALLOW_DEFAULT (PROC_MIG_IMPORT_ALL | PROC_MIG_EXPORT_ALL)


/*
 * Information for encapsulating process state.
 */

/*
 * Identifiers to match encapsulated states and modules.
 */
typedef enum {
    PROC_MIG_ENCAP_PROC,
    PROC_MIG_ENCAP_VM,
    PROC_MIG_ENCAP_FS,
    PROC_MIG_ENCAP_MACH,
    PROC_MIG_ENCAP_PROF,
    PROC_MIG_ENCAP_SIG,
} Proc_EncapToken;

#define PROC_MIG_NUM_CALLBACKS 6

/*
 * Each module that participates has a token defined for it.
 * It also provides routines to encapsulate and deencapsulate data,
 * as well as optional routines that may be called prior to migration
 * and subsequent to migration.  This structure is passed around to
 * other modules performing encapsulation.
 */
typedef struct {
    Proc_EncapToken	token;		/* info about encapsulated data */
    int			size;		/* size of encapsulated data */
    ClientData		data;		/* for use by encapsulator */
    int			special;	/* indicates special action required */
    ClientData		specialToken;	/* for use during special action */
    int			processed;	/* indicates this module did possibly
					   destructive encapsulation operation
					   and should be called to clean up
					   on failure */
					   
} Proc_EncapInfo;


/*
 * External declarations of variables and procedures.
 */
extern int proc_MigDebugLevel;		/* amount of debugging info to print */
extern int proc_MigrationVersion;	/* to distinguish incompatible
					   versions of migration */
extern Boolean proc_DoTrace;		/* controls amount of tracing */
extern Boolean proc_DoCallTrace;	/* ditto */
extern Boolean proc_KillMigratedDebugs;	/* kill foreign processes instead
					   of putting them in the debugger */
extern int proc_AllowMigrationState;	/* how much migration to permit */

/*
 * Functions for process migration.  [Others should be moved here.]
 */
extern void Proc_ResumeMigProc();
extern void Proc_DestroyMigratedProc();

extern void Proc_RemoveMigDependency();
extern void Proc_AddMigDependency();
extern ReturnStatus Proc_WaitForHost();
extern ReturnStatus Proc_WaitForMigration();

extern ReturnStatus Proc_RpcMigCommand();
extern ReturnStatus Proc_RpcRemoteCall();


#endif /* _PROCMIGRATE */
