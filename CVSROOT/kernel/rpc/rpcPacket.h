/*
 * rpcPacket.h --
 *
 * This defines the RPC packet header format.  This header comes just
 * after the packet transport header.  It contains information about the
 * remote procedure call and is followed by parameters and data for the
 * RPC.
 *
 * Copyright (C) 1987 Regents of the University of California
 * All rights reserved.
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * This packet format no longer agrees with the old-old sprite-unix
 * hybrid (header "fsp.h")
 *
 * sccsid "Sprite $Header$"
 */

#ifndef _RPCPACKET
#define _RPCPACKET

#ifdef KERNEL
#include <rpcTypes.h>
#else
#include <kernel/rpcTypes.h>
#endif /* KERNEL */

/*
 * The Rpc header.
 */
typedef struct RpcHdr {
    unsigned int	version;	/* This is a combined version number
					 * and byte-order indicator.  See
					 * the defines below. */
    unsigned int	flags;		/* Protocol flags, explained below */
    int			clientID;	/* Client Sprite host id */
    int			serverID;	/* Server Sprite host id */
    int			channel;	/* The client channel number */
    int			serverHint;	/* Server index hint. The server
					 * machine updates this on every
					 * packet it sends to a client.  The
					 * channel being used for the RPC
					 * should preserve the value sent
					 * and return it (in this field) with
					 * future messages to the server */
    unsigned int	bootID;		/* Client boot timestamp.  This changes
					 * each time a host reboots. */
    unsigned int	ID;		/* ID/sequence number. This ID is the
					 * same on all packets pertaining
					 * to the same RPC. This increases
					 * until the origniating host reboots */
    unsigned int	delay;		/* Interfragment delay info */
    unsigned int	numFrags;	/* Number of fragments in packet (<=16).
					 * If the packet is complete, ie. no
					 * fragmenting, then this field should
					 * be ZERO */
    unsigned int	fragMask;	/* Fragment bitmask ID. The I'th frag
					 * has the I'th bit of this mask
					 * set.  If no fragments then this
					 * field should be ZERO.  On partial
					 * acknowledgments this contains
					 * the receiver's summary bitmask */
    int			command;	/* Rpc command (see rpcCall.h) or
					 * error code if RPC_ERROR is set */
    int			paramSize;	/* Size of the parameter area */
    int			dataSize;	/* Size of the data data area */
    int			paramOffset;	/* This is the starting offset for the
					 * block of parameter bytes sent in
					 * this fragment.  This is zero for
					 * unfragmented sends and for the first
					 * fragment of a group */
    int			dataOffset;	/* Offset for the data area. */
} RpcHdr;				/* 64 BYTES */


/*
 * Version number / byte-ordering word.
 *	RPC_NATIVE_VERSION	Version number in native format
 *	RPC_SWAPPED_VERSION	Version number if originating on a Vax byte
 *				ordered host and read on a MC680xx host,
 *				or vice versa.
 *	To change the version number increment the right-most byte
 *	in the native version.
 */

/*
 *	Version 1:	6-13-88		Original.
 *	Version 2:	6-19-88		For cleaning up parameters in
 *					preparation for byte-swapping.
 *	Version 3:	6-8-89		New FS parameters.  New ethernet
 *					packet type (not 0x500 !!)
 */
#define	RPC_NATIVE_VERSION	0x0f0e0003
#define RPC_SWAPPED_VERSION	0x03000e0f

/*
 * These are the version variables actually used in the kernel. You can
 * change them early in the boot if you want a different version number.
 */

extern int 	rpc_NativeVersion;
extern int	rpc_SwappedVersion;

/*
 * The flags field is used to type packets and for flags that
 * modify the packet type.
 *
 * Packet Types:
 *	RPC_REQUEST a request message sent by the client to the server.
 *
 *	RPC_REPLY a reply message returned to the client from the server.
 *
 *	RPC_ACK an explicit acknowledgment message for a request or reply.
 *	If the RPC_LASTFRAG flag is present then this is a partial
 *	acknowledgment and the summary bitmask is in the fragMask field.
 *
 *	RPC_ECHO a special packet that just is bounced off the server by
 *	the dispatcher.  A server process is not involved.
 *
 *	RPC_NACK a negative acknowledgement from the server saying that it's
 *	alive, but can't handle the request yet (no server process for it, etc).
 *
 * Packet Flags:
 *	RPC_PLSACK demands an explicit acknowledgment message from the receiver.
 *
 *	RPC_LASTFRAG is present on the last fragment of a batch being sent,
 *	which is not always the last fragment in a message because a partial
 *	partial re-send may only involve some fragments in the middle
 *	of the message.  This flag set on partial acknowledgments.
 *
 *	RPC_CLOSE only valid on type RPC_ACK messages.  This means the server
 *	is requesting acknowledgement of its last reply so it can reassign
 *	the server process to an active client channel.  When combined with
 *	RPC_SERVER, this means the client has successfully gotten its
 *	last reply.
 *
 *	RPC_ERROR indicates the service routine had an error, in this
 *	case the command field contains an error code.
 *
 *	RPC_NOT_ACTIVE means that the originating host is not fully alive.
 *	This means the packet is ignored by the recovery module, but still good.
 *
 *	RPC_FAST means this rpc packet comes from a server than was rebooted
 *	with a "fast boot" and therefor no recovery is necessary if we
 *	thought it was dead.
 *
 *	RPC_SERVER_RECOV means that the server is driving crash recovery
 *	and the clients must wait until it contacts them in order to reopen
 *	their files.
 *
 * Flags only valid in trace records:
 *	RPC_SERVER the packet is bound for the server side of the rpc system.
 *
 *	RPC_LAST_REC the trace record is the oldest trace record in the
 *	circular buffer.
 *
 *	RPC_FLAGS is gross, because it has two non-contiguous flag areas.
 *	This is because the RPC_TYPE fields are smack in the middle of
 *	the flags integer, and we ran out of space for regular flags.
 */
#define RPC_NO_FLAGS		0x0
#define RPC_FLAGS		0xff00ff
#define RPC_PLSACK		0x000001
#define RPC_LASTFRAG		0x000002
#define RPC_CLOSE		0x000004
#define RPC_ERROR		0x000008
#define RPC_SERVER		0x000010
#define RPC_LAST_REC		0x000020
#define RPC_NOT_ACTIVE		0x000040
#define	RPC_FAST		0x000080
/* Skip over bits used in RPC_TYPE, below. */
#define	RPC_SERVER_RECOV	0x010000

#define RPC_TYPE		0x00ff00
#define RPC_REQUEST		0x000100
#define RPC_ACK			0x000200
#define RPC_REPLY		0x000400
#define RPC_ECHO		0x000800
#define	RPC_NACK		0x001000

/*
 * Items related to Fragmenting.
 *
 * The maximum size supported by fragmentation is 16K of data plus
 * 1k of parameters.  This large chunk is broken into fragments.
 * The maximum size of a datagram is dependent on the transport medium.
 */
#define RPC_MAX_NUM_FRAGS	16
#define RPC_MAX_FRAG_SIZE	1024
#define RPC_MAX_DATASIZE	(RPC_MAX_NUM_FRAGS * RPC_MAX_FRAG_SIZE)
#define RPC_MAX_PARAMSIZE	(1 * RPC_MAX_FRAG_SIZE)
#define RPC_MAX_SIZE		(RPC_MAX_DATASIZE+RPC_MAX_PARAMSIZE)

/*
 * An array of bitmasks is kept so the dispatcher can quickly determine if
 * a message is complete.  Indexed by the total number of fragments in the
 * packet, the array contains a complete bitmask for that many fragments.
 */
extern unsigned int rpcCompleteMask[];

/*
 * Machines have some minimum delay that they can handle between successive
 * packets before they start to drop them.  The current notion of this delay
 * for other hosts is kept in rpcDelay.  This machine's currently acceptable
 * delay is kept in rpcMyDelay, while its max speed at generating packets is
 * kept in rpcOutputRate.  This information is used to insert, if neccesary,
 * delays between transmission of fragments.
 */
extern short rpcDelay[];		/* Other host's inter-frag delay */
extern unsigned short rpcMyDelay;	/* This host's inter-frag delay */
extern unsigned short rpcOutputRate;		/* This machines output rate */

/*
 * Forward Declarations.
 */
extern ReturnStatus RpcOutput _ARGS_((int spriteID, register RpcHdr *rpcHdrPtr, RpcBufferSet *message, RpcBufferSet *fragment, unsigned int dontSendMask, Sync_Semaphore *mutexPtr));

extern void RpcGetMachineDelay _ARGS_((unsigned short *myDelayPtr, unsigned short *outputRatePtr));

#endif /* not _RPCPACKET */
