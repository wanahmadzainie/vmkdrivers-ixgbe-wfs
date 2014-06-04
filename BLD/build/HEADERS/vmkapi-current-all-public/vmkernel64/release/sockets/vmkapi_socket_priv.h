/***************************************************************************
 * Copyright 2007 - 2012 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * vmkapi_socket_priv.h                                           */ /**
 * \addtogroup Socket
 * @{
 * \defgroup SocketPrv Network Socket Interfaces (private portion)
 * @{
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_SOCKET_PRIV_H_
#define _VMKAPI_SOCKET_PRIV_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 * Address families
 */
/** \brief  Kernel/User IPC */
#define VMK_SOCKET_AF_VMKLINK    27
/** \brief  IPv6 */
#define VMK_SOCKET_AF_INET6      28

/*
 * Protocol families
 */
#define VMK_SOCKET_PF_VMKLINK    VMK_SOCKET_AF_VMKLINK
#define VMK_SOCKET_PF_INET6      VMK_SOCKET_AF_INET6

/* Max length of connection backlog */
#define VMK_SOCKET_SOMAXCONN     128

/*
 * Socket send/receive flags
 */
/** \brief Process out-of-band data */
#define	VMK_SOCKET_MSG_OOB          0x1
/** \brief Peek at incoming message */
#define	VMK_SOCKET_MSG_PEEK	    0x2
/** \brief Send without using routing tables */
#define	VMK_SOCKET_MSG_DONTROUTE    0x4
/** \brief Data completes record */
#define	VMK_SOCKET_MSG_EOR          0x8
/** \brief Data discarded before delivery */
#define	VMK_SOCKET_MSG_TRUNC	    0x10
/** \brief Control data lost before delivery */
#define	VMK_SOCKET_MSG_CTRUNC	    0x20
/** \brief Wait for full request or error */
#define	VMK_SOCKET_MSG_WAITALL	    0x40
/** \brief data completes connection */
#define	VMK_SOCKET_MSG_EOF          0x100
/** \brief FIONBIO mode, used by fifofs */
#define	VMK_SOCKET_MSG_NBIO         0x4000
/** \brief used in sendit() */
#define	VMK_SOCKET_MSG_COMPAT       0x8000

/*
 * Socket-level socket options
 */
/** \brief Turn on debugging info recording */
#define	VMK_SOCKET_SO_DEBUG         0x0001
/** \brief Socket has had listen() */
#define	VMK_SOCKET_SO_ACCEPTCON     0x0002
/** \brief Permit sending of broadcast msgs */
#define	VMK_SOCKET_SO_BROADCAST	    0x0020
/** \brief Bypass hardware when possible */
#define	VMK_SOCKET_SO_USELOOPBACK   0x0040
/** \brief Leave received OOB data in line */
#define	VMK_SOCKET_SO_OOBINLINE     0x0100
/** \brief Allow local address & port reuse */
#define	VMK_SOCKET_SO_REUSEPORT     0x0200
/** \brief There is an accept filter */
#define	VMK_SOCKET_SO_ACCEPTFILTER  0x1000
/** \brief Specify IPv4 next hop for socket */
#define VMK_SOCKET_SO_NEXTHOP       0x1019

/*
 * Socket ioctl commands
 */
/** \brief Set interface gateway address */
#define VMK_SOCKET_IOCTL_CMD_SET_IF_GWADDR 62
/** \brief Get interface gateway address */
#define VMK_SOCKET_IOCTL_CMD_GET_IF_GWADDR 63

/** \brief Length of the interface name for Ioctl data */
#define VMK_SOCKET_IOCTL_DATA_IF_NAME_LEN 16

/** \brief Data structure for some vmk_SocketIoctl calls */
typedef struct vmk_SocketIoctlAddrData {
   /** \brief Name of the interface for the ioctl */
   char              ifName[VMK_SOCKET_IOCTL_DATA_IF_NAME_LEN];
   /** \brief Socket address information */
   vmk_SocketAddress addr;
} vmk_SocketIoctlAddrData;

/*
 ***********************************************************************
 * vmk_SocketListen --                                            */ /**
 *
 * \ingroup SocketPrv
 * \brief Setup a socket to allow connections.
 *
 * \note This function will not block.
 *
 * \param[in] socket    Socket on which to allow connections.
 * \param[in] backlog   Max number of connections that are allowed to
 *                      wait to be accepted on a connection.
 *
 * \retval VMK_NOT_SUPPORTED   Unknown socket type.
 * \retval VMK_EOPNOTSUPP      Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_ENOTCONN        Maps to BSD error code ENOTCONN.
 * \retval VMK_BAD_PARAM       Maps to BSD error code EINVAL.
 * \retval VMK_WOULD_BLOCK     Maps to BSD error code EAGAIN.
 * \retval VMK_EADDRNOTAVAIL   Maps to BSD error code EADDRNOTAVAIL.
 * \retval VMK_ADDRFAM_UNSUPP  Maps to BSD error code EAFNOSUPPORT.
 * \retval VMK_EADDRINUSE      Maps to BSD error code EADDRINUSE.
 * \retval VMK_NO_ACCESS       Maps to BSD error code EACCESS.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       listen(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketListen(vmk_Socket socket,
                                  int backlog);

/*
 ***********************************************************************
 * vmk_SocketAccept --                                            */ /**
 *
 * \ingroup SocketPrv
 * \brief Setup a socket to allow connections.
 *
 * \note This function may block if not a nonblocking socket (default).
 *
 * \note This call may block.
 *
 * \param[in]  socket            Socket on which to allow connections.
 * \param[in]  canBlock          Should this call block?
 * \param[out] address           The network address info of the remote
 *                               connecting network entity.
 * \param[in,out] addressLength  The length of the address info.
 * \param[out] newSocket         A new socket to communicate with the
 *                               connecting network entity.
 * 
 * \retval VMK_NOT_SUPPORTED    Unknown socket type.
 * \retval VMK_BAD_PARAM        Socket not in listen.
 * \retval VMK_NO_MODULE_HEAP   This module's heap is not set.
 * \retval VMK_WOULD_BLOCK      Socket is nonblocking.
 * \retval VMK_EOPNOTSUPP       Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_NO_MEMORY        Maps to BSD error code ENOMEM.
 * \retval VMK_ECONNABORTED     Maps to BSD error code ECONNABORTED.
 * \retval VMK_BAD_PARAM        Maps to BSD error code EINVAL.
 * \retval VMK_INVALID_ADDRESS  Maps to BSD error code EFAULT.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       accept(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketAccept(vmk_Socket socket,
                                  vmk_Bool canBlock,
                                  vmk_SocketAddress *address,
                                  int *addressLength,
                                  vmk_Socket *newSocket);

/*
 ***********************************************************************
 * vmk_SocketIoctl --                                             */ /**
 *
 * \ingroup SocketPrv
 * \brief Issue an ioctl to the socket.
 *
 * \note This function does not block.
 *
 * \param[in]     socket        Socket to issue ioctl on
 * \param[in]     command       ioctl command VMK_SOCKET_IOCTL_*
 * \param[in,out] data          Input/output data for the ioctl command
 * 
 * \retval VMK_NOT_SUPPORTED    Unknown socket type.
 * \retval VMK_NO_MODULE_HEAP   This module's heap is not set.
 * \retval VMK_EOPNOTSUPP       Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_NO_MEMORY        Maps to BSD error code ENOMEM.
 * \retval VMK_BAD_PARAM        Maps to BSD error code EINVAL.
 * \retval VMK_INVALID_ADDRESS  Maps to BSD error code EFAULT.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       accept(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketIoctl(vmk_Socket socket,
                                 int command,
                                 void *data);
#endif /* _VMKAPI_SOCKET_PRIV_H_ */
/** @} */
/** @} */
