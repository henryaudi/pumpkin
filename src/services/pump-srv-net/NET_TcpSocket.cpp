/**
 ******************************************************************************
 * @file    NET_TcpSocket.cpp
 * @author  Shangjie Zheng
 * @brief   TCP socket that owns its file descriptor: implementation.
 *          This file provides functions to:
 *           + Open, configure, bind, listen, accept and connect a socket
 *           + Send and receive bytes, treating "try again" as "nothing yet"
 *           + Close the file descriptor exactly once
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

#include "services/pump-srv-net/NET_TcpSocket.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>

#include "common/pump-cmn-stat/STAT_Status.hpp"

namespace pumpkin::services {

namespace {
// ================================================================================================
// PRIVATE SCOPE
// ================================================================================================
/**
 * @brief Checks whether errno means "nothing to do right now, try again later".
 *
 * @return true for EAGAIN / EWOULDBLOCK / EINTR, false for a real error.
 */
bool net_isTryAgain() {
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
}

int net_makeAddress(const char* sz_Ip, uint16_t u16_Port, sockaddr_in* ptr_Address) {
    if (sz_Ip == nullptr) {
        return -P_EINVARG;
    }

    *ptr_Address            = sockaddr_in{};
    ptr_Address->sin_family = AF_INET;  // IPv4
    ptr_Address->sin_port   = htons(u16_Port);
    return (inet_pton(AF_INET, sz_Ip, &ptr_Address->sin_addr) == 1) ? P_OK : -P_EINVARG;
}
}  // namespace

// ================================================================================================
// GLOBAL SCOPE
// ================================================================================================
TcpSocket::TcpSocket(int fd_Socket) : m_fd_socket(fd_Socket) {}

TcpSocket::~TcpSocket() {
    close();
}

TcpSocket::TcpSocket(TcpSocket&& obj_Other) noexcept : m_fd_socket(obj_Other.m_fd_socket) {
    obj_Other.m_fd_socket = -1;
}

TcpSocket& TcpSocket::operator=(TcpSocket&& obj_Other) noexcept {
    if (this != &obj_Other) {
        close();
        m_fd_socket           = obj_Other.m_fd_socket;
        obj_Other.m_fd_socket = -1;
    }
    return *this;
}

int TcpSocket::open() {
    if (isOpen()) {
        return -P_EALREADY;
    }
    m_fd_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    return (m_fd_socket < 0) ? -STAT_MapErrno(errno) : P_OK;
}

void TcpSocket::close() {
    if (isOpen()) {
        ::close(m_fd_socket);
        m_fd_socket = -1;
    }
}

int TcpSocket::setReuseAddress() {
    if (!isOpen()) {
        return -P_ENOTINIT;
    }

    const int int_Enable = 1;
    if (::setsockopt(m_fd_socket, SOL_SOCKET, SO_REUSEADDR, &int_Enable, sizeof(int_Enable)) < 0) {
        return -STAT_MapErrno(errno);
    }

    return P_OK;
}

int TcpSocket::setNonBlocking() {
    if (!isOpen()) {
        return -P_ENOTINIT;
    }

    const int int_SocketFlags = ::fcntl(m_fd_socket, F_GETFL, 0);
    if (int_SocketFlags < 0) {
        return -STAT_MapErrno(errno);
    }
    if (::fcntl(m_fd_socket, F_SETFL, int_SocketFlags | O_NONBLOCK) < 0) {
        return -STAT_MapErrno(errno);
    }

    return P_OK;
}

int TcpSocket::bind(const char* sz_Ip, uint16_t u16_Port) {
    if (!isOpen()) {
        return -P_ENOTINIT;
    }

    sockaddr_in str_Address{};
    const int   int_Ret = net_makeAddress(sz_Ip, u16_Port, &str_Address);
    if (int_Ret < 0) {
        return int_Ret;
    }

    if (::bind(m_fd_socket, reinterpret_cast<sockaddr*>(&str_Address), sizeof(str_Address)) < 0) {
        return -STAT_MapErrno(errno);
    }

    return P_OK;
}

int TcpSocket::listen() {
    if (!isOpen()) {
        return -P_ENOTINIT;
    }
    return (::listen(m_fd_socket, SOMAXCONN) < 0) ? -STAT_MapErrno(errno) : P_OK;
}

int TcpSocket::accept(TcpSocket* ptr_Client) {
    if (ptr_Client == nullptr) {
        return -P_EINVARG;
    }
    if (!isOpen()) {
        return -P_ENOTINIT;
    }

    const int fd_Client = ::accept(m_fd_socket, nullptr, nullptr);
    if (fd_Client < 0) {
        return net_isTryAgain() ? P_OK : -STAT_MapErrno(errno);
    }

    *ptr_Client = TcpSocket(fd_Client);
    return P_OK;
}

int TcpSocket::connect(const char* sz_Ip, uint16_t u16_Port) {
    if (!isOpen()) {
        return -P_ENOTINIT;
    }

    /* Create the address */
    sockaddr_in str_Address{};
    const int   int_Ret = net_makeAddress(sz_Ip, u16_Port, &str_Address);
    if (int_Ret < 0) {
        return int_Ret;
    }

    /* Connect to the address */
    if (::connect(m_fd_socket, reinterpret_cast<sockaddr*>(&str_Address), sizeof(str_Address)) <
        0) {
        return -STAT_MapErrno(errno);
    }

    return P_OK;
}

int TcpSocket::recv(char* ptr_Dest, size_t siz_Capacity, size_t* ptr_Received,
                    bool* ptr_PeerClosed) {
    if (ptr_Dest == nullptr || ptr_Received == nullptr || ptr_PeerClosed == nullptr) {
        return -P_EINVARG;
    }

    *ptr_Received   = 0;
    *ptr_PeerClosed = false;
    if (!isOpen()) {
        return -P_ENOTINIT;
    }

    /* Receive data from m_fd_socket load to ptr_Dest */
    const ssize_t ssiz_Received = ::recv(m_fd_socket, ptr_Dest, siz_Capacity, 0);
    if (ssiz_Received < 0) {
        return net_isTryAgain() ? P_OK : -STAT_MapErrno(errno);
    }
    if (ssiz_Received == 0) {
        *ptr_PeerClosed = true;  // end of the stream: the peer has closed the connection.
        return P_OK;
    }

    *ptr_Received = static_cast<size_t>(ssiz_Received);
    return P_OK;
}

int TcpSocket::send(const char* ptr_Data, size_t siz_Length, size_t* ptr_Sent) {
    if (ptr_Data == nullptr || ptr_Sent == nullptr) {
        return -P_EINVARG;
    }

    *ptr_Sent = 0;
    if (!isOpen()) {
        return -P_ENOTINIT;
    }

    const ssize_t ssiz_Sent = ::send(m_fd_socket, ptr_Data, siz_Length, 0);
    if (ssiz_Sent < 0) {
        return net_isTryAgain() ? P_OK : -STAT_MapErrno(errno);  // socket full: 0 sent, no error
    }

    *ptr_Sent = static_cast<size_t>(ssiz_Sent);
    return P_OK;
}

int TcpSocket::localPort(uint16_t* ptr_Port) const {
    if (ptr_Port == nullptr) {
        return -P_EINVARG;
    }
    if (!isOpen()) {
        return -P_ENOTINIT;
    }

    sockaddr_in str_Address{};
    socklen_t   u32_Length = sizeof(str_Address);
    if (::getsockname(m_fd_socket, reinterpret_cast<sockaddr*>(&str_Address), &u32_Length) < 0) {
        return -STAT_MapErrno(errno);
    }

    *ptr_Port = ntohs(str_Address.sin_port);
    return P_OK;
}

bool TcpSocket::isOpen() const {
    return m_fd_socket >= 0;
}

int TcpSocket::fd() const {
    return m_fd_socket;
}

}  // namespace pumpkin::services
