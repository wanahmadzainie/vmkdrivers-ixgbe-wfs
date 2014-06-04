/***************************************************************************
 * Copyright 2007 - 2012 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Sockets                                                        */ /**
 * \defgroup Socket Network Socket Interfaces
 * @{
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_SOCKET_H_
#define _VMKAPI_SOCKET_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Opaque socket handle.
 */
typedef struct vmk_SocketInt *vmk_Socket;

/*
 * Address families
 */
/** \brief IPv4 */
#define VMK_SOCKET_AF_INET       2

/*
 * Socket types
 */
/** \brief Streaming */
#define VMK_SOCKET_SOCK_STREAM   1

/** \brief Datagrams */
#define VMK_SOCKET_SOCK_DGRAM    2

/** \brief Raw datagrams */
#define VMK_SOCKET_SOCK_RAW      3

/*
 * Flags for vmk_SocketSendTo() and vmk_SocketRecvFrom()
 */
/** \brief Send/receive of this message should not block */
#define	VMK_SOCKET_MSG_DONTWAIT	    0x80

/*
 * Socket option levels
 */
/** \brief Operate on the socket itself */
#define VMK_SOCKET_SOL_SOCKET       0xffff

/*
 * Socket-level socket options
 */
/** \brief Allow local address reuse */
#define	VMK_SOCKET_SO_REUSEADDR     0x0004
/** \brief Keep connections alive */
#define	VMK_SOCKET_SO_KEEPALIVE     0x0008
/** \brief Just use interface addresses */
#define	VMK_SOCKET_SO_DONTROUTE     0x0010
/** \brief Linger on close if data present */
#define	VMK_SOCKET_SO_LINGER	    0x0080
/** \brief Timestamp received dgram traffic */
#define	VMK_SOCKET_SO_TIMESTAMP     0x0400
/** \brief Use non-blocking socket semantics */
#define VMK_SOCKET_SO_NONBLOCKING   0x1015
/** \brief Bind socket to a vmknic
 *  \note Note that the TCP/IP stack will only transmit the packet if the routing
 *        decision indicates that it can be sent out of the specified vmknic.
 */
#define VMK_SOCKET_SO_BINDTOVMK     0x1016

/*
 * Values for the vmk_SocketShutdown()'s "how" parameter
 */
/** \brief Further receives will be disallowed */
#define VMK_SOCKET_SHUT_RD       0
/** \brief Further sends will be disallowed. */
#define VMK_SOCKET_SHUT_WR       1
/** \brief Further sends and receives will be disallowed. */
#define VMK_SOCKET_SHUT_RDWR     2

/**
 * \brief
 * Abstract socket network address
 *
 * A protocol-specific address is used in actual practice.
 */
typedef struct vmk_SocketAddress {
   vmk_uint8   sa_len;
   vmk_uint8   sa_family;
   vmk_uint8   sa_data[254];
} VMK_ATTRIBUTE_PACKED vmk_SocketAddress;

/**
 * \brief
 * Data structure for setting the VMK_SOCKET_SO_LINGER option
 */
typedef struct vmk_SocketLingerData {
   /** \brief Whether linger is enabled or not */
   vmk_uint32   enabled;
   /** \brief Linger duration (in seconds) */
   vmk_uint32   duration;
} VMK_ATTRIBUTE_PACKED vmk_SocketLingerData;

/*
 ***********************************************************************
 * vmk_SocketAddrToString --                                      */ /**
 *
 * \ingroup Socket
 * \brief Convert an address into a simple string for a particular
 *        address family.
 *
 * \note This function will not block.
 *
 * \param[in]  addr           Address to translate to a string.
 * \param[out] buffer         Buffer to place the converted string into.
 * \param[in]  bufferLength   Length of the buffer in bytes.
 *
 * \retval VMK_OK             Success. 
 * \retval VMK_BAD_PARAM      Bad input addr or buffer 
 * \retval VMK_NOT_SUPPORTED  Unknown address family. 
 *
 * \note This call does *not* do any sort of network lookup. It merely
 *       converts an address into a human-readable format. In most
 *       cases the converted string is simply a numeric string.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketAddrToString(const vmk_SocketAddress *addr,
                                        char *buffer,
                                        int bufferLength);

/*
 ***********************************************************************
 * vmk_SocketStringToAddr --                                      */ /**
 *
 * \ingroup Socket
 * \brief Convert an address into a simple string for a particular
 *        address family.
 *
 * \note This function will not block.
 *
 * \param[in]  addressFamily   Address family that the string address
 *                             will be converted into.
 * \param[in]     buffer       Buffer containing the string to convert.
 * \param[in]     bufferLength Length of the buffer in bytes.
 * \param[in,out] addr         Address the string is converted into.
 *
 * \retval VMK_OK             Success. 
 * \retval VMK_BAD_PARAM      Bad input addr or buffer 
 * \retval VMK_NOT_SUPPORTED  Unknown address family. 

 * \note This call does *not* do any sort of network lookup. It merely
 *       converts a simple human-readable string into a address. In most
 *       cases, this is simply a conversion of a numeric string into
 *       a network address.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketStringToAddr(int addressFamily,
                                        const char *buffer,
                                        int bufferLength,
                                        vmk_SocketAddress *addr);

/*
 ***********************************************************************
 * vmk_SocketCreate --                                            */ /**
 *
 * \ingroup Socket
 * \brief Create a new unbound socket.
 *
 * \note This function will not block.
 * \note The default behavior is to create a blocking socket. If
 *       nonblocking behavior is required then the
 *       VMK_SOCKET_SO_NONBLOCKING socket option must be set.
 *
 * \param[in]  domain      Protocol family for this socket.
 * \param[in]  type        Type of communication on this socket
 * \param[in]  protocol    Specific protocol to use from address family
 * \param[out] socket      Newly created socket
 *
 * \retval VMK_OK               Success. 
 * \retval VMK_BAD_PARAM        Bad input parameter. 
 * \retval VMK_NO_MODULE_HEAP   The module's heap is not set.
 * \retval VMK_NO_MEMORY        Unable to allocate memory for socket. 
 * \retval VMK_EPROTONOSUPPORT  Maps to BSD error code EPROTONOSUPPORT.
 * \retval VMK_BAD_PARAM_TYPE   Maps to BSD error code EPROTOTYPE.
 * \retval VMK_NO_BUFFERSPACE   Maps to BSD error code ENOBUFS.
 * \retval VMK_EOPNOTSUPP       Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_NO_ACCESS        Maps to BSD error code EACCESS.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       socket(2).
 *
 * \note Specific domain (VMK_SOCKET_AF_*), type (VMK_SOCKET_SOCK_*),
 *       and protocol (VMK_SOCKET_*PROTO*) values are implementation
 *       dependent, an application can determine if a specific domain
 *       and type is supported by trying to create a socket with zero
 *       protocol value.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketCreate(int domain,
                                  int type,
                                  int protocol,
                                  vmk_Socket *socket);

/*
 ***********************************************************************
 * vmk_SocketBind --                                              */ /**
 *
 * \ingroup Socket
 * \brief Bind a socket to a network address endpoint.
 *
 * \note This function will not block.
 *
 * \param[in] socket          Socket to bind to the network address
 * \param[in] address         Information describing the network address
 *                            that the socket will be bound to.
 * \param[in] addressLength   Length in bytes of the network address
 *                            information.
 *
 * \retval VMK_BAD_PARAM       Socket was already bound
 * \retval VMK_NOT_SUPPORTED   Unknown socket type.
 * \retval VMK_NO_MODULE_HEAP  This module's heap is not set.
 * \retval VMK_EOPNOTSUPP      Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_BAD_PARAM       Maps to BSD error code EINVAL.
 * \retval VMK_EADDRNOTAVAIL   Maps to BSD error code EADDRNOTAVAIL.
 * \retval VMK_ADDRFAM_UNSUPP  Maps to BSD error code EAFNOSUPPORT.
 * \retval VMK_EADDRINUSE      Maps to BSD error code EADDRINUSE.
 * \retval VMK_NO_ACCESS       Maps to BSD error code EACCESS.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       bind(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketBind(vmk_Socket socket,
                                vmk_SocketAddress *address,
                                int addressLength);

/*
 ***********************************************************************
 * vmk_SocketConnect --                                           */ /**
 *
 * \ingroup Socket
 * \brief Connect to a network address
 *
 * \note This function may block if the socket is blocking socket
 * (this is the default behavior). If nonblocking behavior is required
 * then the VMK_SOCKET_SO_NONBLOCKING socket option must be set.
 *
 * \param[in] socket          Socket to connect through.
 * \param[in] address         The network address info for the address
 *                            to connect to.
 * \param[in] addressLength   The length of the address info.
 *
 * \retval VMK_NOT_SUPPORTED      Unknown socket type.
 * \retval VMK_EALREADY           Socket already connected.
 * \retval VMK_EINPROGRESS        Socket is nonblocking and connection
 *                                is still in progress.
 * \retval VMK_EOPNOTSUPP         Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_ALREADY_CONNECTED  Maps to BSD error code EISCONN.
 * \retval VMK_BAD_PARAM          Maps to BSD error code EINVAL.
 * \retval VMK_EADDRNOTAVAIL      Maps to BSD error code EADDRNOTAVAIL.
 * \retval VMK_ADDRFAM_UNSUPP     Maps to BSD error code EAFNOSUPPORT.
 * \retval VMK_EADDRINUSE         Maps to BSD error code EADDRINUSE.
 * \retval VMK_NO_ACCESS          Maps to BSD error code EACCESS.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       connect(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketConnect(vmk_Socket socket,
                                   vmk_SocketAddress *address,
                                   int addressLength);

/*
 ***********************************************************************
 * vmk_SocketShutdown --                                          */ /**
 *
 * \ingroup Socket
 * \brief Shutdown part or all of a connection on a socket
 *
 * \note This function may block if VMK_SOCKET_SO_LINGER has been set
 *       otherwise it will not block.
 *
 * \param[in] socket     Socket to query.
 * \param[in] how        Data direction(s) to shutdown on the socket.
 *
 * \retval VMK_NOT_SUPPORTED  Unknown socket type.
 * \retval VMK_EOPNOTSUPP     Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_ECONNRESET     Maps to BSD error code ECONNRESET.
 * \retval VMK_ENOTCONN       Maps to BSD error code ENOTCONN.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       shutdown(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketShutdown(vmk_Socket socket,
                                    int how);

/*
 ***********************************************************************
 * vmk_SocketClose --                                             */ /**
 *
 * \ingroup Socket
 * \brief Destroy an existing socket.
 *
 * \note This function may block if VMK_SOCKET_SO_LINGER has been set
 *       otherwise will not block.
 *
 * \param[in] socket Socket to close
 *
 * \retval VMK_OK             Success. 
 * \retval VMK_BAD_PARAM      Input socket is invalid. 
 * \retval VMK_NO_MODULE_HEAP The module's heap is not set.
 * \retval VMK_BUSY           Socket is already closing.
 * \retval VMK_NOT_SUPPORTED  Unknown socket type.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketClose(vmk_Socket socket);

/*
 ***********************************************************************
 * vmk_SocketGetSockOpt --                                        */ /**
 *
 * \ingroup Socket
 * \brief Get the option information from a socket
 *
 * \note This function will not block.
 *
 * \param[in]  socket   Socket to get the option info from.
 * \param[in]  level    Level of communication infrastructure from which
 *                      to get the socket option.
 * \param[in]  option   The option to get the information about.
 * \param[out] optval   Data that is currently set on the option.
 * \param[out] optlen   The length of option data.
 *
 * \retval VMK_OK             Success. 
 * \retval VMK_BAD_PARAM      Input socket is invalid. 
 * \retval VMK_NOT_SUPPORTED  Unknown socket type.
 * \retval VMK_NOT_SUPPORTED  Maps to BSD error code ENOPROTOOPT.
 * \retval VMK_EOPNOTSUPP     Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_BAD_PARAM      Maps to BSD error code EINVAL.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       getsockopt(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketGetSockOpt(vmk_Socket socket,
                                      int level,
                                      int option,
                                      void *optval,
                                      int *optlen);

/*
 ***********************************************************************
 * vmk_SocketSetSockOpt --                                        */ /**
 *
 * \ingroup Socket
 * \brief Set the option information on a socket
 *
 * \note This function will not block.
 *
 * \param[in] socket    Socket to set the option info on.
 * \param[in] level     Level of communication infrastructure from which
 *                      to set the socket option.
 * \param[in] option    The option to set the information about.
 * \param[in] optval    Data to set on the option.
 * \param[in] optlen    The length of the option data.
 *
 * \retval VMK_OK             Success. 
 * \retval VMK_BAD_PARAM      Input socket is invalid. 
 * \retval VMK_NOT_SUPPORTED  Unknown socket type.
 * \retval VMK_NOT_SUPPORTED  Maps to BSD error code ENOPROTOOPT.
 * \retval VMK_EOPNOTSUPP     Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_BAD_PARAM      Maps to BSD error code EINVAL.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       setsockopt(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketSetSockOpt(vmk_Socket socket,
                                      int level,
                                      int option,
                                      const void *optval,
                                      int optlen);

/*
 ***********************************************************************
 * vmk_SocketSendTo --                                            */ /**
 *
 * \ingroup Socket
 * \brief Send data to a network address.
 *
 * \note This function may block if the VMK_SOCKET_MSG_DONTWAIT flag is
 *       not set or the socket is a blocking socket.
 *
 * \param[in]  socket         Socket to send the data through.
 * \param[in]  flags          Settings for this send transaction.
 * \param[in]  address        Address information describing the
 *                            data's destination.
 * \param[in]  data           Pointer to data buffer to send.
 * \param[in]  len            Length in bytes of the data buffer to send.
 * \param[out] bytesSent      Number of bytes actually sent.
 *
 * \retval VMK_OK                 Success.
 * \retval VMK_NOT_SUPPORTED      Unknown socket type.
 * \retval VMK_BAD_PARAM          Unsupported flags setting.
 * \retval VMK_EOPNOTSUPP         Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_ENOTCONN           Maps to BSD error code ENOTCONN.
 * \retval VMK_EDESTADDRREQ       Maps to BSD error code EDESTADDRREQ.
 * \retval VMK_MESSAGE_TOO_LONG   Maps to BSD error code EMSGSIZE.
 * \retval VMK_WOULD_BLOCK        Maps to BSD error code EAGAIN.
 * \retval VMK_NO_BUFFERSPACE     Maps to BSD error code ENOBUFS.
 * \retval VMK_EHOSTUNREACH       Maps to BSD error code EHOSTUNREACH.
 * \retval VMK_ALREADY_CONNECTED  Maps to BSD error code EISCONN.
 * \retval VMK_ECONNREFUSED       Maps to BSD error code ECONNREFUSED.
 * \retval VMK_EHOSTDOWN          Maps to BSD error code EHOSTDOWN.
 * \retval VMK_ENETDOWN           Maps to BSD error code ENETDOWN.
 * \retval VMK_EADDRNOTAVAIL      Maps to BSD error code EADDRNOTAVAIL.
 * \retval VMK_BROKEN_PIPE        Maps to BSD error code EPIPE.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       sendto(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketSendTo(vmk_Socket socket,
                                  int flags,
                                  vmk_SocketAddress *address,
                                  void *data,
                                  int len,
                                  int *bytesSent);

/*
 ***********************************************************************
 * vmk_SocketRecvFrom --                                          */ /**
 *
 * \ingroup Socket
 * \brief Receive data from a network address.
 *
 * \note This function may block if the VMK_SOCKET_MSG_DONTWAIT flag is
 *       not set or the socket is a blocking socket.
 *
 * \param[in]     socket         Socket to receive the data through.
 * \param[in]     flags          Settings for this receive transaction.
 * \param[in]     address        The source address information the
 *                               messages should be received from,
 *                               or NULL if this is not necessary for
 *                               the socket's protocol or settings.
 * \param[in,out] addressLength  Length in bytes of the address
 *                               information.
 * \param[in]     data           Pointer to data buffer to receive to.
 * \param[in]     len            Length in bytes of the data buffer.
 * \param[out]    bytesReceived  Number of bytes actually received.
 *
 * \retval VMK_NOT_SUPPORTED     Receive on unbound VMKLINK socket is
 *                               not supported.
 * \retval VMK_EOPNOTSUPP        Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_ENOTCONN          Maps to BSD error code ENOTCONN.
 * \retval VMK_MESSAGE_TOO_LONG  Maps to BSD error code EMSGSIZE.
 * \retval VMK_WOULD_BLOCK       Maps to BSD error code EAGAIN.
 * \retval VMK_ECONNRESET        Maps to BSD error code ECONNRESET.
 * \retval VMK_INVALID_ADDRESS   Maps to BSD error code EFAULT.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       recvfrom(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketRecvFrom(vmk_Socket socket,
                                    int flags,
                                    vmk_SocketAddress *address,
                                    int *addressLength,
                                    void *data,
                                    int len,
                                    int *bytesReceived);

/*
 ***********************************************************************
 * vmk_SocketGetSockName --                                       */ /**
 *
 * \ingroup Socket
 * \brief Get the socket's local endpoint network address information.
 *
 * \note This function will not block.
 *
 * \param[in] socket             Socket to query.
 * \param[out] address           The network address info for the socket
 *                               local endpoint.
 * \param[in,out] addressLength  The length of the address info.
 *
 * \retval VMK_OK             Success.
 * \retval VMK_BAD_PARAM      Bad input socket or address.
 * \retval VMK_NOT_SUPPORTED  Unknown socket family.
 * \retval VMK_EOPNOTSUPP     Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_BAD_PARAM      Maps to BSD error code EINVAL.
 * \retval VMK_NO_MEMORY      Maps to BSD error code ENOMEM.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       getsockname(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketGetSockName(vmk_Socket socket,
                                       vmk_SocketAddress *address,
                                       int *addressLength);

/*
 ***********************************************************************
 * vmk_SocketGetPeerName --                                       */ /**
 *
 * \ingroup Socket
 * \brief Get the socket's far endpoint network address information.
 *
 * \note This function will not block.
 *
 * \param[in]  socket            Socket to query.
 * \param[out] address           The network address info for the
 *                               socket remote endpoint.
 * \param[in,out] addressLength  The length of the address info.
 *
 * \retval VMK_OK             Success.
 * \retval VMK_BAD_PARAM      Bad input socket or address.
 * \retval VMK_NOT_SUPPORTED  Unknown socket family.
 * \retval VMK_EOPNOTSUPP     Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_NO_MEMORY      Maps to BSD error code ENOMEM.
 * \retval VMK_ENOTCONN       Maps to BSD error code ENOTCONN.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       getpeername(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketGetPeerName(vmk_Socket socket,
                                       vmk_SocketAddress *address,
                                       int *addressLength);
#endif /* _VMKAPI_SOCKET_H_ */
/** @} */
