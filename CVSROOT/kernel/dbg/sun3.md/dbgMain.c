/* dbgMain.c -
 *
 *     This contains the routines which read and execute commands from kdbx.
 *
 *     All reads and writes to kdbx occur over channel A of the Rs232 line.
 *
 * Copyright (C) 1985 Regents of the University of California
 * All rights reserved.
 */

#ifndef lint
static char rcsid[] = "$Header$ SPRITE (Berkeley)";
#endif not lint

#include "sprite.h"
#include "exc.h"
#include "dbg.h"
#include "dbgInt.h"
#include "machine.h"
#include "proc.h"
#include "vmMachInt.h"
#include "vm.h"
#include "vmInt.h"
#include "sunMon.h"
#include "net.h"
#include "netEther.h"
#include "netInet.h"
#include "dev.h"
#include "byte.h"

Boolean	dbg_BeingDebugged = FALSE;		/* TRUE if are under control
						 * of kdbx.*/
Boolean	dbg_Rs232Debug = FALSE;			/* TRUE if are using the RS232
						 * line to debug, FALSE if are
						 * using the network. */
Boolean	dbg_UsingNetwork = FALSE;		/* TRUE if the debugger is
						 * using the network interface*/
char	requestBuffer[DBG_MAX_REQUEST_SIZE];	/* Buffer to receive request
						 * into. */
int	requestOffset;				/* Offset in buffer where next
						 * bytes should be read from.*/
char	replyBuffer[DBG_MAX_REPLY_SIZE];	/* Buffer to hold reply. */
int	replyOffset = 0;			/* Offset in buffer where next
						 * bytes in reply should go. */
int	curMsgNum;				/* The current message that
						 * is being processed. */

/*
 * Number of times to poll before timing out and resending (about 2 seconds).
 */
#ifdef SUN3
int	dbgTimeout = 50000;
#else
int	dbgTimeout = 15000;
#endif

/*
 * Information about the latest packet received.
 */
Boolean			gotPacket;	
int			dbgPacketLength;
Net_InetAddress		dbgMyIPAddr;
Net_InetAddress		dbgSrcIPAddr;
Net_InetAddress		dbgSrcPort;
Net_EtherHdr		dbgEtherHdr;
Net_ScatterGather	dbgGather;

/*
 * Size of debugging packet header and data.
 */
#define	PACKET_HDR_SIZE (sizeof(Net_EtherHdr) + Dbg_PacketHdrSize() + 4)
#define PACKET_DATA_SIZE (DBG_MAX_REPLY_SIZE - PACKET_HDR_SIZE)

/*
 * Strings which describe each of the opcodes that kdbx can send us.
 */
static char *opcodeNames[] =  {
	"Read all GPRS",
	"Write GPR",
	"Continue",
	"Single Step",
	"Detach",
	"Inst Read",
	"Inst Write",
	"Data Read",
	"Data Write",
	"Process to walk stack for",
	"Read information after stopped",
	"Return version string",
	"Divert syslog to the console",
	"Reboot the machine",
	"Set up things to start a call command",
	"Clean up things after a call command has executed",
	"UNKNOWN OPCODE"
};

/*
 * Strings which describe the different exceptions that can occur.
*/
static char *exceptionNames[] = {
    "Reset",
    "Bus Error",
    "Address Error",
    "Illegal Instruction",
    "Zero Div",
    "CHK Instruction",
    "TrapV",
    "Privilege Violation",
    "Trace Trap",
    "EMU 1010",
    "EMU 1111",
    "Stack Format Error",
    "Uninitialized Vector",
    "Spurious Interrupt",
    "Level 1 Interrupt",
    "Level 2 Interrupt",
    "Level 3 Interrupt",
    "Level 4 Interrupt",
    "Level 5 Interrupt",
    "Level 6 Interrupt",
    "Level 7 Interrupt",
    "Syscall Trap",
    "Signal Return Trap",
    "Bad Trap",
    "Breakpoint Trap",
    "UNKNOWN EXCEPTION"
};

/*
 * The type of machine that we are on.
 */
int		machineType;

/*
 * The RS 232 Port that we communicate with.  Channel A is used
 * although one could argue for using port B.  However, facilities
 * have welded a bracket to the 2/50 and 3/75 bodies which render
 * the B port useless.  (Can't attach the connector...)
 */
Dbg_Rs232Channel		dbgChannel = DBG_RS232_CHANNELA;

/*
 * Declare global variables.
 */
int		dbgSfcReg;
int		dbgDfcReg;
int 		dbgUserContext;
int 		dbgKernelContext;
Exc_TrapStack	dbgTrapStack;
int		dbgTermReason;
int		dbgInDebugger;
int		dbgIntPending;
int		dbgExcType;
Boolean		dbgPanic;
int		dbgSavedSP;
int		dbgMaxStackAddr;
Boolean		dbg_UsingSyslog = FALSE;
Boolean		dbgCanUseSyslog = TRUE;
static	   int	oldContext;

/*
 * Saved exception stack stuff.
 */
static	int		savedTrapStackLength;
static	int		savedExcStackLength;
static	Exc_TrapStack	savedTrapStack;
static	Boolean		callInProgress = FALSE;


/*
 * ----------------------------------------------------------------------------
 *
 * DbgCheckNmis --
 *
 *	Turn Non-maskable-interrupts on and off to allow keyboard events to
 *	take place.  Only needed on Sun-2's because of the funny mapping
 *	between kernel and user address spaces - they are turned on in the
 *	main debugging loop on Sun-3's.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 * ----------------------------------------------------------------------------
 */
DbgCheckNmis()
{
#ifdef SUN2
    int	oldContext;
    oldContext = VmMachGetKernelContext(); 
    VmMachSetKernelContext(VMMACH_KERN_CONTEXT);
    Mon_StartNmi();
    Mon_StopNmi();
    VmMachSetKernelContext(oldContext);
#endif
}


/*
 * ----------------------------------------------------------------------------
 *
 * InRange --
 *
 *     Return true if the given address is a valid kernel address and false
 *     otherwise.
 *
 * Results:
 *     True if the given address is a valid kernel address and false
 *     otherwise.
 *
 * Side effects:
 *     None.
 *
 * ----------------------------------------------------------------------------
 */
static Boolean InRange(addr, numBytes) 
    unsigned 	int addr; 
    int		numBytes; 
{
    VmMachPTE		pte;
    int			i;
    unsigned	int	protection;
    int			firstPage;
    int			lastPage;
    unsigned	int	maxAddr;

#ifdef SUN2
    maxAddr = 0x1000000;
#else
    maxAddr = 0x10000000;
#endif
    if (addr > maxAddr || (addr + numBytes - 1) > maxAddr) {
	return(FALSE);
    }
    if ((int) (addr) & 0x1) {
	Sys_Printf("Dbg: odd address: %x\n", addr);
	return(FALSE);
    }

    firstPage = ((unsigned int) addr) >> VMMACH_PAGE_SHIFT;
    lastPage = (((unsigned int) addr) + numBytes - 1) >> VMMACH_PAGE_SHIFT;
    for (i = firstPage; i <= lastPage; i++) {
	pte = VmMachGetPageMap((Address)(i << VMMACH_PAGE_SHIFT));
	protection = pte & VMMACH_PROTECTION_FIELD;
	if (!(pte & VMMACH_RESIDENT_BIT) || 
	    (protection != VMMACH_KR_PROT && protection != VMMACH_KRW_PROT &&
	     protection != VMMACH_UR_PROT && protection != VMMACH_URW_PROT)) {
	    return(FALSE);
	}
    }

    return(TRUE);
}


/*
 * ----------------------------------------------------------------------------
 *
 * TranslateOpcode --
 *
 *     Return the string which describes the given opcode.
 *
 * Results:
 *     The string which describes the given opcode.
 *
 * Side effects:
 *     None.
 *
 * ----------------------------------------------------------------------------
 */
char *
TranslateOpcode(opcode)
    Dbg_Opcode opcode;		/* The opcode which is to be translated. */
{
	int index;

	index = (int) opcode;
	if (index < 0 || index > (int) DBG_UNKNOWN) {
	    index = (int) DBG_UNKNOWN;
	}

	return(opcodeNames[index]);
}


/*
 * ----------------------------------------------------------------------------
 *
 * TranslateException --
 *
 *     Return the string that describes the given exception.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 * ----------------------------------------------------------------------------
 */
char *
TranslateException(exception)
    int exception;		/* The exception which is to be translated. */
{

	if (exception < 0 || exception > EXC_UNKNOWN_EXC) {
	    exception = EXC_UNKNOWN_EXC;
	}

	return(exceptionNames[exception]);
}


/*
 * ----------------------------------------------------------------------------
 *
 * ReadBytes --
 *
 *     Read the given number of bytes into the given address from the 
 *     rs232 line.  Acknowledge every third character.  This is done to 
 *     prevent kdbx from overunning us.  We can do every third character 
 *     because the receive buffer is of size three.  We also acknowledge the
 *     last character sent.
 *
 * Results:
 *     The bytes that were read are stored into bytePtr.
 *
 * Side effects:
 *     None.
 *
 * ----------------------------------------------------------------------------
 */
static
ReadBytes(numBytes, bytePtr, ack)
    int		numBytes;	/* The number of bytes to read */
    Address	bytePtr;	/* Where to store the data that was read. */
    Boolean	ack;		/* ack every 3rd character. */
{
    int		i;
    Boolean	justAcked;

    for (i = 0; i < numBytes; i++) {
	justAcked = FALSE;
	bytePtr[i] = DbgRs232ReadChar(dbgChannel);
	if (dbgTraceLevel >= 4) {
	    Sys_Printf("Read byte = %x\n", bytePtr[i]);
	}
	if (ack && (i + 1) % 3 == 0) {
	    if (dbgTraceLevel >= 4) {
		Sys_Printf("Acking read\n");
	    }
	    justAcked = TRUE;
	    DbgRs232WriteChar(dbgChannel, 0);
	}
    }
    if (ack && !justAcked) {
	if (dbgTraceLevel >= 4) {
	    Sys_Printf("Acking read\n");
	}
	DbgRs232WriteChar(dbgChannel, 0);
    }
}


/*
 * ----------------------------------------------------------------------------
 *
 * WriteBytes --
 *
 *     Write the given number of bytes from the given address over the 
 *     rs232 line.  We have to wait for an ack once we have sent a specified
 *     number of bytes.  This is to prevent us from overrunning kdbx.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 * ----------------------------------------------------------------------------
 */
static
WriteBytes(numBytes, bytePtr)
    int		numBytes;	/* The number of bytes to write */
    Address	bytePtr;
{
    int		  i;
    char 	input;

    if (dbgTraceLevel >= 3) {
	Byte_Copy(4, bytePtr, (char *) &i);
	Sys_Printf("\r\nWriting: %d", i);
    }

    for (i = 0; i < numBytes; i++) {
	DbgRs232WriteChar(dbgChannel, bytePtr[i]);
	if ((i + 1) % DBG_ACK_SIZE == 0) {
	    input = DbgRs232ReadChar(dbgChannel);
	}
    }
}


/*
 * ----------------------------------------------------------------------------
 *
 * DbgComplain --
 *
 *     Complain because we are allready in the debugger.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 * ----------------------------------------------------------------------------
 */
void
DbgComplain(trapStack)
    Exc_TrapStack	trapStack;
{
    Sys_Printf("%s exception in the debugger at pc %x addr %x\n",
			TranslateException(dbgExcType), trapStack.excStack.pc,
			trapStack.excStack.tail.addrBusErr.faultAddr);
    Mon_Abort();
}


/*
 * ----------------------------------------------------------------------------
 *
 * Dbg_Init --
 *
 *     Initialize the debugger.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     dbgMonPC and dbgDoTrace are initialized.
 *
 * ----------------------------------------------------------------------------
 */
void
Dbg_Init()
{
    dbgMonPC = 0;
    dbgTraceLevel = 0;
    dbgInDebugger = 0;
    dbgIntPending = 0;
    dbgPanic = FALSE;
    dbg_BeingDebugged = FALSE;
    machineType = Mach_GetMachineType();
    Mon_Printf("Machine type %d\n", machineType);
}


/*
 * ----------------------------------------------------------------------------
 *
 * Dbg_InputPacket --
 *
 *     See if the current packet is for us.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     gotPacket is set to true if we got a packet that we liked.
 *
 * ----------------------------------------------------------------------------
 */
void
Dbg_InputPacket(packetPtr, packetLength)
    Address	packetPtr;
    int		packetLength;
{
    Address	dataPtr;
    int		dataLength;
    Net_EtherHdr	*etherHdrPtr;

    etherHdrPtr = (Net_EtherHdr *)packetPtr;
    if (etherHdrPtr->type != NET_ETHER_IP) {
	if (dbgTraceLevel >= 5) {
	    Sys_Printf("Non-IP (Type=0x%x) ", (int)etherHdrPtr->type);
	}
	return;
    }
    if (gotPacket) {
	return;
    }
    if (dbgTraceLevel >= 4) {
	Sys_Printf("Validating packet\n");
    }
    if (Dbg_ValidatePacket(packetLength - sizeof(Net_EtherHdr),
			   packetPtr + sizeof(Net_EtherHdr),
			   &dataLength, &dataPtr,
			   &dbgMyIPAddr, &dbgSrcIPAddr, &dbgSrcPort)) {
	if (dbgTraceLevel >= 4) {
	    Sys_Printf("Got a packet: length=%d\n", dataLength);
	}
	Byte_Copy(sizeof(Net_EtherHdr), etherHdrPtr, &dbgEtherHdr);
	gotPacket = TRUE;
	Byte_Copy(dataLength, dataPtr, requestBuffer);
    }
}


/*
 * ----------------------------------------------------------------------------
 *
 * ReadRequest --
 *
 *     Read the next request from kdbx.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     TRUE if didn't time out.
 *
 * ----------------------------------------------------------------------------
 */
Boolean
ReadRequest(timeout)
    Boolean	timeout;	/* TRUE if should timeout after waiting a 
				 * while. */
{
    if (dbg_Rs232Debug) {
	/*
	 * Don't read anything yet - wait until the actual request for bytes.
	 */
	replyOffset = 0;
	return(TRUE);
    } else {
	int	timeOutCounter;

	gotPacket = FALSE;
	timeOutCounter = dbgTimeout;
	do {
	    DbgCheckNmis();
	    Net_RecvPoll();
	    if (timeout) {
		timeOutCounter--;
	    }
	} while(!gotPacket && timeOutCounter != 0);
	if (gotPacket) {
	    replyOffset = PACKET_HDR_SIZE;
	    requestOffset = 4;
	    curMsgNum = *(int *)(requestBuffer);
	    if (dbgTraceLevel >= 4) {
		Sys_Printf("MsgNum = %d\n", curMsgNum);
	    }
	}

	return(gotPacket);
    }
}


/*
 * ----------------------------------------------------------------------------
 *
 * GetRequestBytes --
 *
 *     Get the next numBytes bytes from the current request.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 * ----------------------------------------------------------------------------
 */
static void
GetRequestBytes(numBytes, dest)
    int		numBytes;
    Address	dest;
{
    if (dbg_Rs232Debug) {
	ReadBytes(numBytes, dest, TRUE);
    } else {
	Byte_Copy(numBytes, requestBuffer + requestOffset, dest);
	requestOffset += numBytes;
    }
}


/*
 * ----------------------------------------------------------------------------
 *
 * PutReplyBytes --
 *
 *     Put the given bytes into the reply buffer.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 * ----------------------------------------------------------------------------
 */
static void
PutReplyBytes(numBytes, src)
    int		numBytes;
    Address	src;
{
    if (replyOffset + numBytes > DBG_MAX_REPLY_SIZE) {
	Sys_Printf("PutReplyBytes: Buffer overflow\n");
	numBytes = DBG_MAX_REPLY_SIZE - replyOffset;
    }
    Byte_Copy(numBytes, src, &replyBuffer[replyOffset]);
    replyOffset += numBytes;
}


/*
 * ----------------------------------------------------------------------------
 *
 * SendReply --
 *
 *     Send a reply to kdbx.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 * ----------------------------------------------------------------------------
 */
static void
SendReply()
{
    void	Dbg_FormatPacket();

    if (dbg_Rs232Debug) {
	WriteBytes(replyOffset, replyBuffer);
    } else {
	Net_EtherHdr		*etherHdrPtr;

	if (dbgTraceLevel >= 4) {
	    Sys_Printf("Sending reply\n");
	}
	etherHdrPtr = (Net_EtherHdr *) replyBuffer;
	etherHdrPtr->source = dbgEtherHdr.destination;
	etherHdrPtr->destination = dbgEtherHdr.source;
	etherHdrPtr->type = dbgEtherHdr.type;
	dbgGather.bufAddr = replyBuffer + sizeof(Net_EtherHdr);
	dbgGather.length = replyOffset - sizeof(Net_EtherHdr);
	dbgGather.conditionPtr = (Sync_Condition *) NIL;
	*(int *)(replyBuffer + PACKET_HDR_SIZE - 4) = curMsgNum;
	Dbg_FormatPacket(dbgMyIPAddr, dbgSrcIPAddr, dbgSrcPort,
		     replyOffset - sizeof(Net_EtherHdr) - Dbg_PacketHdrSize(),
		     replyBuffer + sizeof(Net_EtherHdr));
	Net_OutputRawEther(etherHdrPtr, &dbgGather, 1);
	if (dbgTraceLevel >= 4) {
	    Sys_Printf("Sent reply\n");
	}
    }
}

/*
 * Whether syslog should remain diverted on continue or not.
 */
static Boolean	syslogDiverted = FALSE;
/*
 * This is a typedef which is used to take care of the hole that is put in
 * the stack when we are called.
 */
typedef struct {
    char	hole[DBG_STACK_HOLE];
} StackHole;

/*
 * 68020 and 68010 moveml instructions work differently.  68010 stores
 * original value of sp and 68020 stores sp - 4.  This has to be fixed
 * when getting around the exception stack.
 */
#ifdef SUN3
#define STACK_INC	4
#endif
#ifdef SUN2
#define STACK_INC	0
#endif

/*
 * ----------------------------------------------------------------------------
 *
 * Dbg_Main --
 *
 *     The main debugger loop.  This will read commands from the rs232 line
 *     and call the proper routine to execute them.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 * ----------------------------------------------------------------------------
 */

void
Dbg_Main(stackHole, trapStack)
    StackHole		stackHole;	/* The hole put in the stack so that
					 * kdbx can play around with the stack*/
    Exc_TrapStack	trapStack;	/* All of the stuff that is put onto the
					 * stack by the trap handler */
{
    short		trapCode;	/* Reason that we trapped that is sent
					 * to kdbx. */
    Boolean	  	done;		/* Boolean to tell us whether to leave
					 * the main debugger loop */
    int		  	trapStackLength;/* The length of the trap stack */
    int		  	excStackLength;	/* The length of the exception part 
					 * of the trap stack */
    Dbg_Opcode	  	opcode;		/* The operation that was requested */
    short	  	tOpcode;	/* Temporary used to receive the opcode
					 * which is sent as a short. */
					/* Process table entry that we switched
					 * stacks to. */
    Proc_ControlBlock	*procPtr = (Proc_ControlBlock *) NIL;
    Boolean		atInterruptLevel;/* TRUE if we were entered from an
					  * interrupt handler. */

#ifdef SUN3
    /*
     * Turn on non-maskable interrupts.
     */
    Mon_StartNmi();
#endif
    /*
     * Switch to kernel context so that we can access the monitor.
     */
    oldContext = VmMachGetKernelContext();
    VmMachSetKernelContext(VMMACH_KERN_CONTEXT);

    /*
     * Put us at interrupt level so that Sys_Printf won't accidently enable
     * interrupts.
     */
    atInterruptLevel = sys_AtInterruptLevel;
    sys_AtInterruptLevel = TRUE;

    /*
     * Force system log output to the console.
     */
    if (!syslogDiverted) {
	Dev_SyslogDebug(TRUE);
    }

    /*
     * We want to inform the user what caused the problem.  However we only
     * tell him if: 1) we are debugging the debugger; 2) we are not under 
     * debugger control (i.e. we don't want to inform the user on every trace 
     * trap), 3) we got something besides a trace trap or a breakpoint trap
     * exception.
     */
    if (dbgTraceLevel >= 1 || !dbg_BeingDebugged || 
        (trapStack.trapType != EXC_TRACE_TRAP && 
         trapStack.trapType != EXC_BRKPT_TRAP)) {
	Sys_Printf("Entering debugger with a %s exception\r\n",
					TranslateException(trapStack.trapType));
    }

    /*
     * The saved stack pointer points to all of the junk on the stack from
     * the exception.  We need to move the saved stack pointer past all of
     * this junk to the point where kdbx sees what it expects (it doesn't 
     * understand exception stacks).  Also we need to copy all of the 
     * trap stack to a global variable.  This allows access to this stack 
     * through the debugger (kdbx doesn't know we are in this routine so the
     * parameter trapStack cannot be printed out in kdbx).
     *
     * NOTE:
     *
     * When kdbx does a "call" command it adds things onto the stack starting 
     * from the saved stack pointer.  When it does this it will trash the 
     * trap stack that we were passed.  Thus saving a copy of the trap
     * stack also serves the purpose of preventing kdbx from trashing the
     * original copy.
     */
    trapStackLength = Exc_GetTrapStackSize(&trapStack);
    excStackLength = trapStackLength - 
			    (sizeof(Exc_TrapStack) - sizeof(Exc_ExcStack));

    Byte_Copy(trapStackLength, (Address) &trapStack, (Address) &dbgTrapStack);

    dbgTrapStack.genRegs[mach_SP] += excStackLength + STACK_INC;

    /*
     * Clear the trace bit from the status register.
     */
    dbgTrapStack.excStack.statusReg &= ~SUN_SR_TRACEMODE;

    /*
     * We need to tell kdbx what type of exception this is.  If the
     * termReason is DBG_INTERRUPT_SIG then we set the trap code to
     * interrupted.  Otherwise we don't know what caused us to get here so
     * we have to check the exception type.  If the exception is either a
     * trace trap of a breakpoint trap then we set the term reason to
     * DBG_TRACE_TRAP_SIG and set the trap code appropriately.  Otherwise we 
     * tell kdbx that the reason was an DBG_INTERRUPT_SIG and we set the trap
     * code appropriately for the given exception.
     */
    trapCode = trapStack.trapType + 1;	/* EXC trap codes are one less than
					 * DBG trap codes. */
    if (dbgTermReason == DBG_INTERRUPT_SIG) {
	trapCode = DBG_INTERRUPT;
    } else if (dbgPanic) {
	dbgPanic = FALSE;
	trapCode = DBG_INTERRUPT;
        dbgTermReason = DBG_INTERRUPT_SIG;
    } else if (trapStack.trapType == EXC_TRACE_TRAP ||
	       trapStack.trapType == EXC_BRKPT_TRAP) {
	dbgTermReason = DBG_TRACE_TRAP_SIG;
    } else {
	dbgTermReason = DBG_INTERRUPT_SIG;
    }

    if (dbg_Rs232Debug) {
	/*
	 * Initialize the rs232 line
	 */
	DbgRs232Init();
    } else {
	dbg_UsingNetwork = TRUE;
    }

    /*
     * If we are stopped after a continue or single step must write a
     * null byte to the debugger at the other end.  It knows that if it sees
     * a null byte we are stopped at this end.
     */
    if (dbg_BeingDebugged) {
	unsigned	char	ch;

	ch = 0;
	PutReplyBytes(1, (Address)&ch);
	SendReply();
	do {
	    if (ReadRequest(TRUE)) {
		GetRequestBytes(2, &tOpcode);
		opcode = (Dbg_Opcode) tOpcode;
		if (opcode == DBG_GET_STOP_INFO) {
		    break;
		}
	    }
	    /*
	     * We can only timeout if we are using network debugging.
	     */
	    Net_OutputRawEther(replyBuffer, &dbgGather, 1);
	    if (dbgTraceLevel >= 5) {
		Sys_Printf("DBG: Timeout\n");
	    }
	    Sys_Printf("TI ");
	} while (TRUE);
    } else {
	if (dbg_Rs232Debug) {
	    unsigned	char	ch;
	    /*
	     * Read in bytes until we get the sequence of 3 characters
	     * 127, 27, 7.
	     */
	    while (TRUE) {
		ReadBytes(1, (Address) &ch, FALSE);
		if (ch == 127) {
		    ReadBytes(1, (Address) &ch, FALSE);
		    if (ch == 27) {
			ReadBytes(1, (Address) &ch, FALSE);
			if (ch == 7) {
			    break;
			}
		    }
		}
	    }
	}
	(void) ReadRequest(FALSE);
	GetRequestBytes(2, &tOpcode);
	opcode = (Dbg_Opcode) tOpcode;
    }

    /*
     * Now read commands until kdbx tells us that we can return.
     */
    done = FALSE;
    while (!done) {
	if (dbgTraceLevel >= 2) {
	    Sys_Printf("Request: %s ", TranslateOpcode(opcode));
	}

	/*
	 * Process the request 
	 */
	switch (opcode) {

	    /*
	     * The client wants to read some data from us ...
	     */

	    case DBG_GET_STOP_INFO: {
		StopInfo	stopInfo;
		stopInfo.codeStart = (int)mach_CodeStart;
		if (procPtr != (Proc_ControlBlock *) NIL) {
		    stopInfo.maxStackAddr = 
		    		procPtr->stackStart + mach_KernStackSize;
		    Byte_Copy(sizeof(procPtr->saveRegs),
			    (Address) procPtr->saveRegs,
			    (Address) stopInfo.genRegs);
		    stopInfo.pc = (int) ((Address) Mach_ContextSwitch);
		} else {
		    stopInfo.maxStackAddr = dbgMaxStackAddr;
		    Byte_Copy(sizeof(dbgTrapStack.genRegs),
			    (Address) dbgTrapStack.genRegs, 
			    (Address) stopInfo.genRegs);
		    stopInfo.pc = dbgTrapStack.excStack.pc;
		}
		stopInfo.termReason = dbgTermReason;
		stopInfo.trapCode = trapCode;
		stopInfo.statusReg = 
			(unsigned short) dbgTrapStack.excStack.statusReg;
		PutReplyBytes(sizeof(stopInfo), (Address)&stopInfo);
		SendReply();
		break;
	    }
	    case DBG_READ_ALL_GPRS:
		if (procPtr != (Proc_ControlBlock *) NIL) {
		    PutReplyBytes(sizeof(procPtr->saveRegs),
				 (Address) procPtr->saveRegs);
		} else {
		    PutReplyBytes(sizeof(dbgTrapStack.genRegs),
			         (Address) dbgTrapStack.genRegs);
		}
		SendReply();
		break;

	    case DBG_GET_VERSION_STRING: {
		char	*SpriteVersion();
		char	*version;

		version = SpriteVersion();
		PutReplyBytes(String_Length(version) + 1, version);
		SendReply();
		break;
	    }
	    
	    case DBG_INST_READ:
	    case DBG_DATA_READ: {
		Dbg_ReadMem	readMem;
		int		status;

		GetRequestBytes(sizeof(readMem), (Address) &readMem); 
		if (dbgTraceLevel >= 2) {
		    Sys_Printf("Addr=%x Numbytes=%d ",
				readMem.address, readMem.numBytes);
		}

		VmMachSetKernelContext(oldContext);
		if (InRange((unsigned int) readMem.address, readMem.numBytes)) {
		    status = 1;
		    PutReplyBytes(sizeof(status), (Address)&status);
		    PutReplyBytes(readMem.numBytes, (Address)readMem.address);
		} else {
		    if (dbgTraceLevel >= 2) {
			Sys_Printf("FAILURE ");
		    }
		    status = 0;
		    PutReplyBytes(sizeof(status), (Address)&status);
		}
		VmMachSetKernelContext(VMMACH_KERN_CONTEXT);
		SendReply();
		break;
	    }

	    /*
	     * The client wants to write something to us.
	     */
	    case DBG_SET_PID: {
		Proc_PID	pid;

		GetRequestBytes(sizeof(pid), (Address) &pid);
		if (!dbg_Rs232Debug) {
		    int	dummy;

		    PutReplyBytes(4, (Address) &dummy);
		    SendReply();
		}
		if (dbgTraceLevel >= 2) {
		    Sys_Printf("pid %x ", pid);
		}
		if (pid == 0) {
		    procPtr = (Proc_ControlBlock *) NIL;
		} else {
		    procPtr = Proc_GetPCB(pid);
		    if (procPtr->state == PROC_UNUSED ||
		        procPtr->state == PROC_DEAD ||
			procPtr->state == PROC_NEW) {
			Sys_Printf("Can't backtrace stack for process %x\n",
					pid);
			procPtr = (Proc_ControlBlock *) NIL;
		    }
		}
		break;
	    }
	    case DBG_REBOOT: {
		int	stringLength;
		char	rebootString[100];
		/*
		 * For a reboot command first read the size of the string and
		 * then the string itself.
		 */
		GetRequestBytes(sizeof(int), &stringLength);
		if (stringLength != 0) {
		    GetRequestBytes(stringLength, (Address)rebootString);
		}
		rebootString[stringLength] = '\0';
		if (!dbg_Rs232Debug) {
		    int	dummy;

		    PutReplyBytes(4, (Address) &dummy);
		    SendReply();
		}
		Mon_Reboot(rebootString);
	    }
	    case DBG_INST_WRITE:
	    case DBG_DATA_WRITE: {
		Dbg_WriteMem		writeMem;
		unsigned	char	ch;
		/*
		 * For an instruction or a data write we first have to find out 
		 * which address to write to and how many bytes to write.  Next
		 * we have to make sure that the address is valid.  If it is
		 * then we read the data and write it to the given address.  If
		 * not we just report an error to kdbx.
		 */
		GetRequestBytes(2 * sizeof(int), (Address) &writeMem);
		if (dbgTraceLevel >= 2) {
		    Sys_Printf("Addr=%x Numbytes=%d ",
				writeMem.address, writeMem.numBytes);
		}

		VmMachSetKernelContext(oldContext);
		if (InRange((unsigned int) writeMem.address,
			    writeMem.numBytes)) {
		    GetRequestBytes(writeMem.numBytes,
				    (Address) writeMem.address);
		    ch = 1;
		} else {
		    int		i;
		    char	buf[100];

		    if (dbgTraceLevel >= 2) {
			Sys_Printf("FAILURE ");
		    }
		    GetRequestBytes(writeMem.numBytes, buf);
		    ch = 0;
		}
		VmMachSetKernelContext(VMMACH_KERN_CONTEXT);

		PutReplyBytes(1, (char *) &ch);
		SendReply();

		break;
	    }

	    case DBG_WRITE_GPR: {                
		Dbg_WriteGPR	writeGPR;

		/*
		 * First find out which register is being written and
		 * then read the value.
		 */
		GetRequestBytes(sizeof(writeGPR), (Address)&writeGPR);
		if (!dbg_Rs232Debug) {
		    int	dummy;

		    PutReplyBytes(4, (Address) &dummy);
		    SendReply();
		}
		if (dbgTraceLevel >= 2) {
		    Sys_Printf("register %d data %x ", writeGPR.regNum, 
				writeGPR.regVal);
		}
		dbgTrapStack.genRegs[writeGPR.regNum] = writeGPR.regVal;
		break;
	    }

	    case DBG_DIVERT_SYSLOG: 
		GetRequestBytes(sizeof(Boolean), (Address)&syslogDiverted);
		if (!dbg_Rs232Debug) {
		    int	dummy;

		    PutReplyBytes(4, (Address) &dummy);
		    SendReply();
		}
		break;

	    case DBG_BEGIN_CALL:
		/*
		 * We are beginning a call command.  Fix up the stack
		 * so that we will be able to continue.  We will put
		 * it back when we are done.
		 */
		savedTrapStackLength = trapStackLength;
		savedExcStackLength = excStackLength;
		Byte_Copy(trapStackLength, &dbgTrapStack, &savedTrapStack);

		dbgTrapStack.excStack.vor.stackFormat = EXC_SHORT;
		trapStackLength = Exc_GetTrapStackSize(&dbgTrapStack);
		excStackLength = trapStackLength - 
		    (sizeof(Exc_TrapStack) - sizeof(Exc_ExcStack));

		callInProgress = TRUE;
		if (dbgCanUseSyslog) {
		    dbg_UsingSyslog = TRUE;
		}
		if (!dbg_Rs232Debug) {
		    int	dummy;

		    PutReplyBytes(4, (Address) &dummy);
		    SendReply();
		}

		break;
	    case DBG_END_CALL: {
		char	*buffer;
		int	*firstIndexPtr;
		int	*lastIndexPtr;
		int	bufSize;
		int	length;
		if (callInProgress) {
		    /*
		     * Restore the state to the state before the
		     * call was begun.  Note that the DBG_END_CALL command will
		     * be executed until the sys log buffer is empty so only
		     * need to restore our state if the callInProgress flag is
		     * set.
		     */
		    trapStackLength = savedTrapStackLength;
		    excStackLength = savedExcStackLength;
		    Byte_Copy(sizeof(dbgTrapStack), &savedTrapStack,
			      &dbgTrapStack);
		    callInProgress = FALSE;
		}
		/*
		 * Dump the syslog buffer.
		 */
		Dev_SyslogReturnBuffer(&buffer, &firstIndexPtr,
				       &lastIndexPtr, &bufSize);
		if (*firstIndexPtr == -1) {
		    length = 0;
		    PutReplyBytes(4, (Address) &length);
		    dbg_UsingSyslog = FALSE;
		} else if (*firstIndexPtr <= *lastIndexPtr) {
		    length = *lastIndexPtr - *firstIndexPtr + 1;
		    if (length + 4 > PACKET_DATA_SIZE) {
			length = PACKET_DATA_SIZE - 4;
		    }
		    PutReplyBytes(4, (Address) &length);
		    PutReplyBytes(length,
				  (Address)&buffer[*firstIndexPtr]);
		    *firstIndexPtr += length;
		    if (*firstIndexPtr > *lastIndexPtr) {
			*firstIndexPtr = *lastIndexPtr = -1;
		    }
		} else {
		    length = bufSize - *firstIndexPtr;
		    if (length + 4 > PACKET_DATA_SIZE) {
			length = PACKET_DATA_SIZE - 4;
		    }
		    PutReplyBytes(4, (Address) &length);
		    PutReplyBytes(length,
				  (Address)buffer[*firstIndexPtr]);
		    *firstIndexPtr += length;
		    if (*firstIndexPtr == bufSize) {
			*firstIndexPtr = 0;
		    }
		}
		SendReply();
		break;
	    }
	    case DBG_CONTINUE: 
		/*
		 * The client wants to continue execution.
		 */
		GetRequestBytes(sizeof(int), 
				(Address) &dbgTrapStack.excStack.pc);
		if (dbgTraceLevel >= 2) {
		    Sys_Printf("Continuing from pc %x ",
				dbgTrapStack.excStack.pc);
		}
		if (!dbg_Rs232Debug) {
		    int	dummy;

		    PutReplyBytes(4, (Address) &dummy);
		    SendReply();
		}

		dbg_BeingDebugged = TRUE;
		done = TRUE;
		break;

	    case DBG_SINGLESTEP:
		/*
		 * The client wants to single step.
		 */
		GetRequestBytes(sizeof(int), 
				(Address) &dbgTrapStack.excStack.pc);
		if (dbgTraceLevel >= 2) {
		    Sys_Printf("Stepping from pc %x ",
				dbgTrapStack.excStack.pc);
		}
		if (!dbg_Rs232Debug) {
		    int	dummy;

		    PutReplyBytes(4, (Address) &dummy);
		    SendReply();
		}

		/* 
		 * Turn the trace bit on in the SR.
		 */
		dbgTrapStack.excStack.statusReg |= SUN_SR_TRACEMODE;
		dbg_BeingDebugged = TRUE;
		done = TRUE;
		break;

	    case DBG_DETACH:
		/*
		 * The debugger has terminated and wants to let us go about our
		 * business.
		 */
		GetRequestBytes(sizeof(int), 
				(Address) &dbgTrapStack.excStack.pc);
		if (dbgTraceLevel >= 2) {
		    Sys_Printf("Detaching at pc %x ",
				dbgTrapStack.excStack.pc);
		}
		if (!dbg_Rs232Debug) {
		    int	dummy;

		    PutReplyBytes(4, (Address) &dummy);
		    SendReply();
		}

		dbg_BeingDebugged = FALSE;
		done = TRUE;
		Sys_Printf("Sprite is now detached from the debugger\r\n");
		break;
	}

	if (dbgTraceLevel >= 2) {
	    Sys_Printf("\r\n");
	}
	if (!done) {
	    ReadRequest(FALSE);
	    GetRequestBytes(2, &tOpcode);
	    opcode = (Dbg_Opcode) tOpcode;
	}
    }

    /*
     * Return from the debugger.  Before we return, we must push the trap
     * stuff back onto the stack and restore the saved sp to point to the
     * exception stuff on the stack.
     */

    dbgSavedSP = dbgTrapStack.genRegs[mach_SP] - trapStackLength;
    dbgTrapStack.genRegs[mach_SP] -= excStackLength + STACK_INC;
    Byte_Copy(trapStackLength, (Address) &dbgTrapStack, (Address) dbgSavedSP);

    VmMachSetKernelContext(oldContext);
    sys_AtInterruptLevel = atInterruptLevel;
    dbg_UsingNetwork = FALSE;

    /*
     * Don't force system log output to the console.
     */
    if (!syslogDiverted) {
	Dev_SyslogDebug(FALSE);
    }
#ifdef SUN3
    /*
     * Turn off non-maskable interrupts.
     */
    Mon_StopNmi();
#endif
}
