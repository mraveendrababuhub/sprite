/* 
 *  main.c --
 *
 *	The main program for Sprite: initializes modules and creates
 *	system processes. Also creates a process to run the Init program.
 *
 * Copyright 1986 Regents of the University of California
 * All rights reserved.
 */

#ifndef lint
static char rcsid[] = "$Header$ SPRITE (Berkeley)";
#endif /* !lint */

#include "machMon.h"
#include "dbg.h"


/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *	First main routine for sun4.  All it does is print Hello World.
 *	It should loop, doing this forever.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
main()
{
    extern	void	Timer_TimerInit();
    extern	void	Timer_TimerStart();
   /*
    * Initialize machine dependent info.  MUST BE CALLED HERE!!!.
    */
    Mach_Init();
    Mach_MonPrintf("After Mach_Init\n");
    Dbg_Init();
    Mach_MonPrintf("After Dbg_Init\n");
    /* Sys Init here */
    Vm_BootInit();
    Mach_MonPrintf("After Vm_BootInit\n");
    Timer_TimerInit();
    Timer_TimerStart();
    Mach_MonPrintf("After Timer Initialize routines\n");
    Sync_Init();
    Mach_MonPrintf("After Sync_Init\n");
    Vm_Init();
    Mach_MonPrintf("After Vm_Init\n");
#ifdef NOTDEF
    NetIEInit("IE", 0, 0x3fd0c000);
    Mach_MonPrintf("After NetIEInit\n");
#else
    Net_Init();
    Mach_MonPrintf("After Net_Init\n");
#endif NOTDEF
    Mach_MonPrintf("Enabling interrupts\n");
    ENABLE_INTR();
#ifdef NOTDEF
    DBG_CALL;
    Mach_MonPrintf("Debugger returned\n");
    Mach_MonPrintf("Trying it again\n");
    DBG_CALL;
    Mach_MonPrintf("Debugger returned again\n");
#else
    Mach_MonPrintf("Here we are.\n");
#endif

    for ( ; ; ) {
	;
    }
}

int
diddly(x)
int	x;
{
    if (x == 0) {
	return 0;
    } else {
	Mach_MonPrintf("Hello World!\n");
	diddly(x - 1);
    }
    return 1;
}

printf(arg, a1, a2, a3, a4, a5, a6)
char	*arg;
{
	Mach_MonPrintf(arg, a1, a2, a3, a4, a5, a6);
	return;
}
