/**
 ******************************************************************************
 * @file    NET_TcpSocket.hpp
 * @author  Shangjie Zheng
 * @brief   TCP socket that owns its file descriptor: public interface.
 *          This file declares:
 *           + TcpSocket: one IPv4 TCP socket, closed automatically when the
 *             object is destroyed; move-only, so one fd always has one owner
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 Shangjie Zheng.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 ******************************************************************************
 */

#ifndef PUMPKIN_NET_TCPSOCKET_HPP_
#define PUMPKIN_NET_TCPSOCKET_HPP_

#include <cstddef>
#include <cstdint>

namespace pumpkin::services {

class TcpSocket {
public:
    /**
     * @brief Default constructor for TcpSocket.
     */
    TcpSocket() = default;

    /**
     * @brief Closes the file descriptor, if one is open.
     */
    ~TcpSocket();

    TcpSocket(const TcpSocket&)            = delete;  // Forbid copy construction
    TcpSocket& operator=(const TcpSocket&) = delete;  // Forbid copy assignment

    /**
     * @brief Take over another socket's fd; the other socket will be left empty.
     *
     * @param obj_Other The TcpSocket to move from. After the move, obj_Other will be left empty.
     */
    TcpSocket(TcpSocket&& obj_Other) noexcept;

    /**
     * @brief Closes this socket's fd (if any), then takes over the other socket's fd.
     *
     * @param obj_Other The TcpSocket to move from. After the move, obj_Other will be left empty.
     */
    TcpSocket& operator=(TcpSocket&& obj_Other) noexcept;

    /**
     * @brief Creates the OS socket (IPv4, TCP).
     *
     * @return P_OK; -P_EALREADY if already open; a negative P_E* code if socket() failed.
     */
    int open();

    /**
     * @brief Closes the file descriptor. Safe to call on an empty socket, and more than once.
     */
    void close();

    /**
     * @brief Allows binding a port that is still in TIME_WAIT, so a server can restart at once.
     */
    int setReuseAddress();

    /**
     * @brief Makes accept/recv/send return immediately instead of waiting.
     */
    int setNonBlocking();

    /**
     * @brief Gives the socket its local address.
     *
     * @param sz_Ip    IPv4 address in dotted form (e.g., "127.0.0.1").
     * @param u16_Port Port; 0 lets the OS pick a free one.
     *
     * @return P_OK; -P_EINVARG for an invalid address; -P_EADDRINUSE, -P_EACCES or another
     *         P_E* code if bind() failed.
     */
    int bind(const char* sz_Ip, uint16_t u16_Port);

    /**
     * @brief Starts accepting connections (backlog SOMAXCONN).
     */
    int listen();

    /**
     * @brief Accepts one waiting connection.
     *
     * @param ptr_Client Receives the new connection. Left empty (not open) if, on a non-blocking
     *                   socket, nobody is waiting right now.
     *
     * @return P_OK (check ptr_Client->isOpen()); -P_EINVARG for a NULL pointer; a negative P_E*
     *         code if accept() failed.
     */
    int accept(TcpSocket* ptr_Client);

    /**
     * @brief Connects to a server. Blocking unless setNonBlocking() was called.
     *
     * @param sz_Ip    IPv4 address in dotted form (e.g., "127.0.0.1").
     * @param u16_Port Port to connect to.
     *
     * @return P_OK; -P_EINVARG for an invalid address; a negative P_E* code if connect() failed.
     */
    int connect(const char* sz_Ip, uint16_t u16_Port);

    /**
     * @brief Receives up to siz_Capacity bytes.
     *
     * @param ptr_Dest       Where the bytes go.
     * @param siz_Capacity   Bytes available at ptr_Dest.
     * @param ptr_Received   Number of bytes received; 0 if nothing is available right now.
     * @param ptr_PeerClosed Set to true if the peer closed its side (end of stream).
     *
     * @return P_OK; -P_EINVARG for a NULL pointer; a negative P_E* code if recv() failed.
     */
    int recv(char* ptr_Dest, size_t siz_Capacity, size_t* ptr_Received, bool* ptr_PeerClosed);

    /**
     * @brief Sends up to siz_Length bytes. May send fewer: the caller keeps the rest.
     *
     * @param ptr_Data   Data to send.
     * @param siz_Length Number of bytes to send.
     * @param ptr_Sent   Number of bytes actually sent; 0 if the socket can't take more right now.
     *
     * @return P_OK; -P_EINVARG for a NULL pointer; a negative P_E* code if send() failed.
     */
    int send(const char* ptr_Data, size_t siz_Length, size_t* ptr_Sent);

    /**
     * @brief Gets the local port the socket is bound to (the real one, after binding port 0).
     *
     * @return P_OK; -P_EINVARG for a NULL pointer; a negative P_E* code if getsockname() failed.
     */
    int localPort(uint16_t* ptr_Port) const;

    /**
     * @brief Checks if the socket is open.
     *
     * @return True if the socket is open; false otherwise.
     */
    bool isOpen() const;

    /**
     * @brief The raw file descriptor, for poll(). -1 if not open. Do not close() it yourself.
     *
     * @return The raw file descriptor, or -1 if the socket is not open.
     */
    int fd() const;

private:
    int m_fd_socket = -1;  // owned file descriptor, -1 when not open

    /**
     * @brief Takes ownership of an fd that is already open (used by accept()).
     *
     * @param fd_Socket The file descriptor to take ownership of. Must be already open.
     */
    explicit TcpSocket(int fd_Socket);
};

}  // namespace pumpkin::services

#endif  // PUMPKIN_NET_TCPSOCKET_HPP_
