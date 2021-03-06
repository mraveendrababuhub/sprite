/* 
 * server.c --
 *
 *	The server part of some multi-program synchronization primatives.
 *	The routines here control N client programs.  This just means
 *	telling them all to start, and hearing back from them when they're done.
 *
 * Copyright 1986 Regents of the University of California
 * All rights reserved.
 */

#ifndef lint
static char rcsid[] = "$Header: /sprite/src/tests/pdevtest/RCS/server.c,v 1.3 88/08/26 17:43:37 brent Exp Locker: brent $ SPRITE (Berkeley)";
#endif not lint


#include <sprite.h>
#include <status.h>
#include <sys/file.h>
#include <errno.h>
#include <fs.h>
#include <dev/pdev.h>
#include <stdio.h>
#include <time.h>
#include <sys.h>
#include <bit.h>
#include "pdevInt.h"

char *pdev="./pdev";

extern Boolean writeBehind;
extern int delay;

typedef int  (*IntProc)();

typedef struct ServerState {
    int cntlStream;	/* Control stream to find out new clientStream's */
    int numClients;
    int *clientStream;	/* Array of client streams */
    int maxStreamID;
    Address *request;	/* Array of client request buffers */
    char *clientState;	/* Array of client state words */
    int *selectMask;
    int selectMaskBytes;
    IntProc opTable[7];	/* Operation switch table */
} ServerState;

#define REPLY_BUF_SIZE		(16 * 1024)
#define REQUEST_BUF_SIZE	(REPLY_BUF_SIZE + sizeof(Pdev_Request))
#define CLIENT_OPENED	0x1
#define CLIENT_STARTED	0x2
#define CLIENT_FINISHED	0x4

/*
 * Need the select flag to know if we should block the client.
 */
Boolean blocked = FALSE;

/*
 * Forward Declarations
 */
ReturnStatus NullProc();
ReturnStatus ServeOpen();
ReturnStatus ServeRead();
ReturnStatus ServeWrite();
ReturnStatus ServeIOControl();
int ServeRequest();


/*
 *----------------------------------------------------------------------
 *
 * ServerSetup --
 *
 *	Establish contact with N clients.  A pseudo device is opened
 *	and we are declared its "master", or "server".  After this
 *	other processes can open the pseudo device and we'll get a private
 *	stream back that we use for requests from that process.
 *
 * Results:
 *	A pointer to state about the clients needed by ServerStart and
 *	ServerWait.
 *
 * Side effects:
 *	Opens the pseudo device as the server and waits for numClients
 *	opens by client processes.
 *	This exits (kills the process) upon error.
 *
 *----------------------------------------------------------------------
 */

void
ServerSetup(numClients, dataPtr)
    int numClients;
    ClientData *dataPtr;
{
    ServerState *statePtr;
    int client;
    int len;
    int amountRead;
    ReturnStatus status;
    Pdev_Notify notify;
    Pdev_SetBufArgs setBuf;
    int streamID;
    int maxStreamID;

    statePtr = (ServerState *)malloc(sizeof(ServerState));
    statePtr->clientStream = (int *)malloc(numClients * sizeof(int));
    statePtr->clientState = (char *)malloc(numClients);
    statePtr->request = (Address *)malloc(numClients * sizeof(Address));
    statePtr->numClients = numClients;

    statePtr->opTable[(int)PDEV_OPEN] = ServeOpen;
    statePtr->opTable[(int)PDEV_CLOSE] = NullProc;
    statePtr->opTable[(int)PDEV_READ] = ServeRead;
    statePtr->opTable[(int)PDEV_WRITE] = ServeWrite;
    statePtr->opTable[(int)PDEV_IOCTL] = ServeIOControl;

    /*
     * Open the pseudo device.
     */
    statePtr->cntlStream = open(pdev, O_CREAT | O_RDONLY | O_MASTER, 0666);
    if (statePtr->cntlStream < 0) {
	perror("Error opening pseudo device as master");
	exit(errno);
    }
    maxStreamID = 0;
    for (client=0 ; client<numClients ; client++) {
	/*
	 * Read on our control stream (the one we just opened) messages
	 * that contain new streamIDs.  These are for private streams
	 * back to the client process..
	 */
	amountRead = read(statePtr->cntlStream, (Address)&notify, sizeof(notify));
	if (amountRead < 0) {
	    perror("Error reading control stream");
	    exit(errno);
	} else if (amountRead != sizeof(notify)) {
	    fprintf(stderr,
		"Warning, short read (%d) on control stream\n", amountRead);
	}
	streamID = notify.newStreamID;
	if (streamID > statePtr->maxStreamID) {
	    statePtr->maxStreamID = streamID;
	}
	/*
	 * Tell the kernel where the request buffer is.
	 */
	statePtr->request[client] = (Address)malloc(REQUEST_BUF_SIZE);
	setBuf.requestBufAddr = statePtr->request[client];
	setBuf.requestBufSize = REQUEST_BUF_SIZE;
	setBuf.readBufAddr = (Address)NULL;
	setBuf.readBufSize = 0;
	Fs_IOControl(streamID, IOC_PDEV_SET_BUF,
			sizeof(Pdev_SetBufArgs), (Address)&setBuf, 0, NULL);
	/*
	 * Set(Unset) write-behind by the client.
	 */
	Fs_IOControl(streamID, IOC_PDEV_WRITE_BEHIND,
			sizeof(int), (Address)&writeBehind, 0, NULL);
	statePtr->clientStream[client] = streamID;
	statePtr->clientState[client] = CLIENT_OPENED;
	fprintf(stderr, "Got client on stream %d\n",streamID);
	ServeRequest(statePtr->clientStream[client],
		     statePtr->request[client],
		     statePtr->opTable);
    }
    /*
     * Now that we know the largest stream ID used for a client stream
     * we can allocate and initialize the select mask for the streams.
     */
    statePtr->selectMaskBytes = Bit_NumBytes(statePtr->maxStreamID);
    statePtr->selectMask = (int *)malloc(statePtr->selectMaskBytes);
    bzero((Address)statePtr->selectMask, statePtr->selectMaskBytes);
    for (client=0 ; client < numClients ; client++) {
	Bit_Set(statePtr->clientStream[client], statePtr->selectMask);
    }
    *dataPtr = (ClientData)statePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * ServeRequest --
 *
 *	The top level service routine that reads client requests
 *	and branches out to a handler for the request.  This takes
 *	care of error conditions and allocating space for the
 *	request and the reply parameters.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	The server side of the pseudo-device protocol.
 *
 *----------------------------------------------------------------------
 */

int
ServeRequest(clientStreamID, myRequestBuf, opTable)
    int clientStreamID;
    Address myRequestBuf;
    IntProc *opTable;
{
    register ReturnStatus status;
    register Pdev_Request *requestPtr;
    Pdev_Reply reply;
    Pdev_BufPtrs bufPtrs;
    register int operation;
    int numBytes;
    char replyBuf[REPLY_BUF_SIZE];
    register char *requestData;
    register Address requestBuf;
    register int i;

    /*
     * Read the current pointers for the request buffer.
     */

    numBytes = read(clientStreamID, (Address) &bufPtrs, sizeof(Pdev_BufPtrs));
    if (numBytes != sizeof(Pdev_BufPtrs)) {
	panic("%s; errno %d, count %d",
		"ServeRequest had trouble reading request buffer pointers",
		errno, numBytes);
    }
    if (bufPtrs.magic != PDEV_BUF_PTR_MAGIC) {
	panic("%s: %d", "ServeRequest got bad pointer magic number",
		bufPtrs.magic);
    }
    /*
     * While there are still requests in the buffer, service them.
     */
    requestBuf = bufPtrs.requestAddr;
    while (bufPtrs.requestFirstByte < bufPtrs.requestLastByte) {
	requestPtr = (Pdev_Request *)&requestBuf[bufPtrs.requestFirstByte];
	if (requestPtr->hdr.magic != PDEV_REQUEST_MAGIC) {
	    panic("ServeRequest, bad request magic # 0x%x\n",
			    requestPtr->hdr.magic);
	}
	requestData = (Address)((int)requestPtr + sizeof(Pdev_Request));

	/*
	 * Switch out the to the handler for the pdev operation.
	 */
	operation = requestPtr->hdr.operation;
	status = (*opTable[(int)operation])(clientStreamID,
		requestPtr, requestData, replyBuf, &reply.selectBits);

	if (delay > 0) {
	    for (i=delay<<1 ; i>0 ; i--) ;
	}

	if (operation != PDEV_WRITE_ASYNC) {
	    /*
	     * Set up the reply and tell the kernel about it.
	     */
    
	    reply.magic = PDEV_REPLY_MAGIC;
	    reply.status = SUCCESS;
	    reply.replySize = requestPtr->hdr.replySize;
	    reply.replyBuf = replyBuf;
	    status = Fs_IOControl(clientStreamID, IOC_PDEV_REPLY,
				    sizeof(Pdev_Reply),
				    (Address) &reply, 0, NULL);
	    if (status != SUCCESS) {
		panic("%s; status \"%s\"",
			"ServeRequest couldn't send reply",
			Stat_GetMsg(status));
	    }
	}
	bufPtrs.requestFirstByte += requestPtr->hdr.messageSize;
    }
    Fs_IOControl(clientStreamID, IOC_PDEV_SET_PTRS,
			sizeof(Pdev_BufPtrs), (Address)&bufPtrs,
			0, NULL);
    return(operation);
}

/*
 *----------------------------------------------------------------------
 *
 * Serve --
 *
 *	Listen for requests from client's, returning after all clients
 *	have closed their streams.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Handle all requests by clients.
 *
 *----------------------------------------------------------------------
 */

void
Serve(data)
    ClientData data;
{
    ServerState *statePtr;
    int client;
    ReturnStatus status;
    int *selectMask;
    int numFinishedClients;
    int numReady;
    int operation;

    statePtr = (ServerState *)data;
    selectMask = (int *)malloc(statePtr->selectMaskBytes);
    numFinishedClients = 0;
    do {
	bcopy((Address)statePtr->selectMask, (Address)selectMask,
		statePtr->selectMaskBytes);
	status = Fs_Select(statePtr->numClients, NULL, selectMask,
				NULL, NULL, &numReady);
	for (client=0 ; client < statePtr->numClients ; client++) {
	    /*
	     * Look for the each client's bit in the select mask and read the
	     * corresponding stream for its initial request.
	     */
	    if (Bit_IsSet(statePtr->clientStream[client], selectMask)) {
		/*
		 * Handle the client's request.  If it's a close
		 * then clear the client's bit from the select mask so
		 * don't bother checking it again.
		 */
		operation = ServeRequest(statePtr->clientStream[client],
			     statePtr->request[client],
			     statePtr->opTable);
		if (operation == PDEV_CLOSE ||
		    operation == -1) {
		    fprintf(stderr, "Client %d %s...", client,
			(operation == PDEV_CLOSE) ? "closed" : "error" );
		    numFinishedClients++;
		    statePtr->clientState[client] |= CLIENT_FINISHED;
		    Bit_Clear(statePtr->clientStream[client],
			    statePtr->selectMask);
		} else if ((operation == PDEV_READ) && selectP) {
		    /*
		     * If the select flag is set, then we must
		     * remember to simulate input for the client
		     * every so often.  This tests regular blocking
		     * reads, and selects by the client.  This goes
		     * with the fact that we only return FS_WRITABLE
		     * if the select flag is set.
		     */
		    Time time;
		    int selectBits;
		    time.seconds = 0;
		    time.microseconds = 400;
		    Sync_WaitTime(time);
		    printf("Waking up client\n");
		    selectBits = FS_READABLE|FS_WRITABLE;
		    Fs_IOControl(statePtr->clientStream[client],
			    IOC_PDEV_READY, sizeof(int), &selectBits,
			    0, NULL);
		}
	    }
	}
    } while (numFinishedClients < statePtr->numClients);
    fprintf(stderr, "\n");
}

/*
 *----------------------------------------------------------------------
 *
 * ServeOne --
 *
 *	A service loop for one client.  More bare-bones test used
 *	for timing.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Handle all requests one client.
 *
 *----------------------------------------------------------------------
 */

void
ServeOne(data)
    ClientData data;
{
    register ServerState *statePtr;
    register int client;
    ReturnStatus status;
    int *selectMask;
    int numFinishedClients;
    int numReady;
    int operation;

    statePtr = (ServerState *)data;
    client = 0;
    do {
	operation = ServeRequest(statePtr->clientStream[client],
		     statePtr->request[client],
		     statePtr->opTable);
	if (operation == PDEV_CLOSE ||
	    operation == -1) {
	    fprintf(stderr, "Client %d %s...", client,
		(operation == PDEV_CLOSE) ? "closed" : "error" );
	    break;
	} else if ((operation == PDEV_READ) && selectP) {
	    /*
	     * If the select flag is set, then we must
	     * remember to simulate input for the client
	     * every so often.  This tests regular blocking
	     * reads, and selects by the client.  This goes
	     * with the fact that we only return FS_WRITABLE
	     * if the select flag is set.
	     */
	    Time time;
	    int selectBits;
	    time.seconds = 0;
	    time.microseconds = 400;
	    Sync_WaitTime(time);
	    printf("Waking up client\n");
	    selectBits = FS_READABLE|FS_WRITABLE;
	    Fs_IOControl(statePtr->clientStream[client],
		    IOC_PDEV_READY, sizeof(int), &selectBits,
		    0, NULL);
	}
    } while (1);
    fprintf(stderr, "\n");
}

/*
 *----------------------------------------------------------------------
 *
 * NullProc --
 *
 *	The do-nothing service procedure.
 *
 * Results:
 *	SUCCESS
 *
 * Side effects:
 *	Zeroes out the reply buffer.
 *
 *----------------------------------------------------------------------
 */

ReturnStatus
NullProc(streamID, requestPtr, requestBuf, replyBuf, selectBitsPtr)
    int streamID;
    Pdev_Request *requestPtr;
    Address requestBuf;
    Address replyBuf;
    int *selectBitsPtr;
{
    if (requestPtr->hdr.replySize > 0) {
	bzero(replyBuf, requestPtr->hdr.replySize);
    }
    return(SUCCESS);
}

/*
 *----------------------------------------------------------------------
 *
 * ServeOpen --
 *
 *	React to an Open request.  This initializes the
 *	select state to both readable and writable.
 *
 * Results:
 *	SUCCESS
 *
 * Side effects:
 *	Print statement.
 *
 *----------------------------------------------------------------------
 */

ReturnStatus
ServeOpen(streamID, requestPtr, requestBuf, replyBuf, selectBitsPtr)
    int streamID;
    Pdev_Request *requestPtr;
    Address requestBuf;
    Address replyBuf;
    int *selectBitsPtr;
{
    fprintf(stderr, "Open request, streamID %d\n", streamID);
    *selectBitsPtr = FS_READABLE | FS_WRITABLE;
    return(SUCCESS);
}

/*
 *----------------------------------------------------------------------
 *
 * ServeRead --
 *
 *	Return data for a read request.  This plays a game with the
 *	client if the select flag (-s) is set:  every other read
 *	gets blocked in order to test IOC_PDEV_READY.
 *
 * Results:
 *	SUCCESS
 *
 * Side effects:
 *	Zeroes out the reply buffer.
 *
 *----------------------------------------------------------------------
 */

ReturnStatus
ServeRead(streamID, requestPtr, requestBuf, replyBuf, selectBitsPtr)
    int streamID;
    Pdev_Request *requestPtr;
    Address requestBuf;
    Address replyBuf;
    int *selectBitsPtr;
{
    if (selectP && !blocked) {
	blocked = TRUE;
	*selectBitsPtr = FS_WRITABLE;
	return(FS_WOULD_BLOCK);
    } else {
	if (requestPtr->hdr.replySize > 0) {
	    bzero(replyBuf, requestPtr->hdr.replySize);
	    replyBuf[0] = 'z';
	}
	blocked = FALSE;
	if (! selectP) {
	    *selectBitsPtr = FS_WRITABLE | FS_READABLE;
	} else {
	    *selectBitsPtr = FS_WRITABLE;
	}
	return(SUCCESS);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ServeWrite --
 *
 *	Handle a write request.
 *
 * Results:
 *	SUCCESS
 *
 * Side effects:
 *	Sets up the select bits.
 *
 *----------------------------------------------------------------------
 */

ReturnStatus
ServeWrite(streamID, requestPtr, requestBuf, replyBuf, selectBitsPtr)
    int streamID;
    Pdev_Request *requestPtr;
    Address requestBuf;
    Address replyBuf;
    int *selectBitsPtr;
{
    *selectBitsPtr = FS_WRITABLE;
    if (! selectP) {
	*selectBitsPtr |= FS_READABLE;
    }
    requestPtr->hdr.replySize = sizeof(int);
    *(int *)replyBuf = requestPtr->hdr.requestSize;
    return(SUCCESS);
}

/*
 *----------------------------------------------------------------------
 *
 * ServeIOControl --
 *
 *	Handle an IOControl.  This acts like an echo now.
 *
 * Results:
 *	SUCCESS
 *
 * Side effects:
 *	Copies the request buffer to the reply buffer.
 *
 *----------------------------------------------------------------------
 */

ReturnStatus
ServeIOControl(streamID, requestPtr, requestBuf, replyBuf, selectBitsPtr)
    int streamID;
    Pdev_Request *requestPtr;
    Address requestBuf;
    Address replyBuf;
    int *selectBitsPtr;
{
    switch (requestPtr->param.ioctl.command) {
	case IOC_PDEV_SET_BUF: {
	    /*
	     * Let the client trigger our test of the mid-flight
	     * setbuf call.
	     */
	    Pdev_SetBufArgs setBuf;

	    setBuf.requestBufAddr = (Address)malloc(REQUEST_BUF_SIZE);
	    setBuf.requestBufSize = REQUEST_BUF_SIZE;
	    setBuf.readBufAddr = (Address)NULL;
	    setBuf.readBufSize = 0;
	    Fs_IOControl(streamID, IOC_PDEV_SET_BUF,
			    sizeof(Pdev_SetBufArgs), (Address)&setBuf, 0, NULL);
    
	}
    }
    *selectBitsPtr = FS_WRITABLE;
    if (! selectP) {
	*selectBitsPtr |= FS_READABLE;
    }
    return(SUCCESS);
}
