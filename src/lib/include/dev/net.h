/*
 * net.h --
 *
 *	This file defines the device-dependent I/O control commands and
 *	related structures for 4.3 BSD Unix socket emulation.
 *
 * Copyright 1987 Regents of the University of California
 * All rights reserved.
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * $Header: net.h,v 1.1 88/06/21 12:07:44 ouster Exp $ SPRITE (Berkeley)
 */

#ifndef _NET_IOC
#define _NET_IOC

#include "sprite.h"
#include "netInet.h"

#define IOC_NET (4<<16)

/*
 * The I/O control commands:
 *  IOC_NET_LISTEN		- make into a passive socket.
 *  IOC_NET_ACCEPT_CONN_1	- accept a pending connection request.
 *  IOC_NET_ACCEPT_CONN_2	- change a new socket in the connect request.
 *  IOC_NET_GET_LOCAL_ADDR	- get the local <address, port> of the socket.
 *  IOC_NET_SET_LOCAL_ADDR	- set the local <address, port> of the socket.
 *  IOC_NET_CONNECT		- try to connect to a remote host.
 *  IOC_NET_GET_REMOTE_ADDR	- get the remote <address,port> of the socket.
 *  IOC_NET_GET_OPTION		- get the current value of an option.
 *  IOC_NET_SET_OPTION		- set the value of an option.
 *  IOC_NET_RECV_FLAGS		- flags to modify behavior of next read.
 *  IOC_NET_RECV_FROM		- <address,port> of last read packet.
 *  IOC_NET_SEND_INFO		- flags to modify behavior of next write.
 *  IOC_NET_SHUTDOWN		- shutdown the socket.
 *  IOC_NET_SET_PROTOCOL	- change the protocol for a socket.
 *  IOC_NET_IS_OOB_DATA_NEXT	- returns TRUE if out-of-band data will be
 *				  read by the next read call.
 *  IOC_NET_STATS		- turn on/off statistics collection.
 *
 */

#define IOC_NET_LISTEN			(IOC_NET | 1)
#define IOC_NET_ACCEPT_CONN_1		(IOC_NET | 2)
#define IOC_NET_ACCEPT_CONN_2		(IOC_NET | 3)
#define IOC_NET_GET_LOCAL_ADDR		(IOC_NET | 4)
#define IOC_NET_SET_LOCAL_ADDR		(IOC_NET | 5)
#define IOC_NET_CONNECT			(IOC_NET | 6)
#define IOC_NET_GET_REMOTE_ADDR		(IOC_NET | 7)
#define IOC_NET_GET_OPTION		(IOC_NET | 8)
#define IOC_NET_SET_OPTION		(IOC_NET | 9)
#define IOC_NET_RECV_FLAGS		(IOC_NET | 10)
#define IOC_NET_RECV_FROM		(IOC_NET | 11)
#define IOC_NET_SEND_INFO		(IOC_NET | 12)
#define IOC_NET_SHUTDOWN		(IOC_NET | 13)
#define IOC_NET_SET_PROTOCOL		(IOC_NET | 14)
#define IOC_NET_IS_OOB_DATA_NEXT	(IOC_NET | 15)
#define IOC_NET_STATS			(IOC_NET | 16)

/*
 * Types of sockets:  (same as 4.3BSD)
 *
 *	NET_SOCK_STREAM		- reliable byte stream, connection-based.
 *	NET_SOCK_DGRAM		- unreliable datagram.
 *	NET_SOCK_RAW		- access to raw protocol interface.
 *	NET_SOCK_RDM		- reliably-delivered message.
 *	NET_SOCK_SEQPACKET	- sequenced packet stream.
 *
 * (These values are from 4.3BSD's <sys/socket.h> file.)
 */

#define NET_SOCK_STREAM		1
#define NET_SOCK_DGRAM		2
#define NET_SOCK_RAW		3
#define NET_SOCK_RDM		4
#define NET_SOCK_SEQPACKET	5

/*
 * The maximum number of connections that can be queued for a stream
 * socket. This value is used with IOC_NET_LISTEN.
 */
#define NET_MAX_NUM_CONNECTS	5

/*
 * The level number for a socket when using IOC_NET_GET_OPTION and 
 * IOC_NET_SET_OPTION.
 */

#define NET_OPT_LEVEL_SOCKET	0xffff

/*
 * Option names for IOC_NET_GET_OPTION / SET_OPTION ioctls.
 *
 * Note: These values must match the SO_ values defined in the 4.3BSD
 * <sys/socket.h> file.
 *
 *   NET_OPT_DEBUG 		- Turn on debugging info recording.
 *   NET_OPT_REUSE_ADDR		- Allow local address reuse.
 *   NET_OPT_KEEP_ALIVE		- Keep connections alive.
 *   NET_OPT_DONT_ROUTE		- Just use interface addresses.
 *   NET_OPT_BROADCAST		- Permit sending of broadcast msgs.
 *   NET_OPT_USE_LOOPBACK	- Bypass hardware when possible.
 *   NET_OPT_LINGER		- Linger on close if data present.
 *   NET_OPT_OOB_INLINE		- Leave received OOB data in line.
 *   NET_OPT_SEND_BUF_SIZE	- Send buffer size.
 *   NET_OPT_RECV_BUF_SIZE	- Receive buffer size.
 *   NET_OPT_SEND_LOWAT		- Send low-water mark.
 *   NET_OPT_RECV_LOWAT		- Receive low-water mark.
 *   NET_OPT_SEND_TIMEOUT	- Send timeout.
 *   NET_OPT_RECV_TIMEOUT	- Receive timeout.
 *   NET_OPT_ERROR		- Get error status and clear.
 *   NET_OPT_TYPE		- Get the socket type.
 */
#define	NET_OPT_DEBUG		0x0001		
#define	NET_OPT_RESERVED_1	0x0002		
#define	NET_OPT_REUSE_ADDR	0x0004		
#define	NET_OPT_KEEP_ALIVE	0x0008		
#define	NET_OPT_DONT_ROUTE	0x0010		
#define	NET_OPT_BROADCAST	0x0020		
#define	NET_OPT_USE_LOOPBACK	0x0040		
#define	NET_OPT_LINGER		0x0080		
#define	NET_OPT_OOB_INLINE	0x0100		
#define NET_OPT_SEND_BUF_SIZE	0x1001		
#define NET_OPT_RECV_BUF_SIZE	0x1002		
#define NET_OPT_SEND_LOWAT	0x1003		
#define NET_OPT_RECV_LOWAT	0x1004		
#define NET_OPT_SEND_TIMEOUT	0x1005		
#define NET_OPT_RECV_TIMEOUT	0x1006		
#define	NET_OPT_ERROR		0x1007		
#define NET_OPT_TYPE		0x1008

/*
 * TCP socket options:
 *
 * NET_OPT_TCP_NO_DELAY		- don't delay sending data by trying to 
 *				  coalesce packets.
 * NET_OPT_TCP_MAX_SEG_SIZE	- maximum segment size.
 *
 * Note: the following values must agree with 4.3BSD's <netinet/tcp.h>
 */
#define NET_OPT_TCP_NO_DELAY		0x1
#define NET_OPT_TCP_MAX_SEG_SIZE	0x2

/*
 * Structure used for manipulating the NET_OPT_LINGER option.
 */
typedef struct	{
	Boolean	onOff;		/* option on/off */
	int	linger;		/* linger time */
} Net_LingerInfo;


/*
 * Values for the flags data in the IOC_NET_RECV_INFO ioctl and
 * the flags field in the Inet_SendInfo struct.
 *
 * NET_OUT_OF_BAND	next read wants out-of-band data or next send
 *			 has out-of-band data to send.
 * NET_PEEK		next read wants to not consume the data in the 
 *			 read queue.
 *
 * Note: These values must match the MSG_ values defined in the 4.3BSD
 * <sys/socket.h> file.
 *
 */

#define NET_OUT_OF_BAND		0x1
#define NET_PEEK		0x2

/*
 * Information sent to the inet server in the IOC_NET_SEND_INFO ioctl.
 * It is used to send the flags and optionally, an address to send the
 * data to. 
 */
typedef struct {
    int	flags;			/* Defined above. */
    Boolean	addressValid;	/* If TRUE, the address field is to be used. */
    union {
	Net_InetSocketAddr	inet; /* Where to send the data. */
	/* 
	 * Other address family socket addresses go here...
	 */
    } address;
} Net_SendInfo;

/*
 * Commands for IOC_NET_STATS ioctl. High-order 16 bits are interpreted
 * as the stat dump file version number.
 */
#define NET_STATS_RESET	1
#define NET_STATS_DUMP	2

#endif _NET_IOC
