/* 
 * Host_Start.c --
 *
 *	Source code for the Host_Start library procedure.
 *
 * Copyright 1988 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#ifndef lint
static char rcsid[] = "$Header: /user5/kupfer/spriteserver/src/lib/c/host/RCS/Host_Start.c,v 1.2 92/06/07 11:50:56 kupfer Exp $ SPRITE (Berkeley)";
#endif not lint

#include <stdio.h>
#include <host.h>

/*
 * Information about the current host database file.  Note that native 
 * Sprite uses a new hosts format that is incompatible with the old one.  
 * The new hosts library requires the new net code (FDDI support, multiple 
 * interfaces, etc.), which we haven't gotten around to putting into the 
 * Sprite server.
 */
    
FILE *		hostFile = (FILE *) NULL;
char *		hostFileName = "/users/kupfer/spriteserver/lib/spritehosts";

/*
 *-----------------------------------------------------------------------
 *
 * Host_Start --
 *
 *	Begin reading from the the current host file.
 *
 * Results:
 *	0 is returned if all went well.  Otherwise -1 is returned
 *	and errno tells what went wrong.
 *
 * Side Effects:
 *	If the file was open, it is reset to the beginning. If it was not
 *	open, it is now.
 *
 *-----------------------------------------------------------------------
 */

int
Host_Start()
{
    if (hostFile != (FILE *) NULL) {
	rewind(hostFile);
    } else {
	hostFile = fopen(hostFileName, "r");
	if (hostFile == (FILE *) NULL) {
	    return -1;
	}
    }
    return 0;
}
