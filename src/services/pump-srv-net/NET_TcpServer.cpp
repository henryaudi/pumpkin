/**
 ******************************************************************************
 * @file    NET_TcpServer.cpp
 * @author  Shangjie Zheng
 * @brief   Single-threaded TCP server (non-blocking sockets + poll()).
 *          This file provides functions to:
 *           + Open a listening socket on 127.0.0.1
 *           + Accept clients and read/write their bytes
 *           + Hand each complete line to the Command Dispatch Engine
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

#include "services/pump-srv-net/NET_TcpServer.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <optional>
#include <vector>

#include "middlewares/pump-mid-cde/CDE_Dispatcher.hpp"
#include "middlewares/pump-mid-pwp/PWP_Codec.hpp"

namespace pumpkin::services {

namespace {
// ================================================================================================
// PRIVATE SCOPE
// ================================================================================================

/* How often run() wakes up to check whether stop() was called. */
constexpr int NET_POLL_TIMEOUT_MS = 100;
/* Bytes read per recv call. */
constexpr std::size_t NET_READ_CHUNK_SIZE = 4096;
/* Stop reading from a client once this many bytes are queued for writing (backpressure). */
constexpr std::size_t NET_MAX_WRITE_BUFFER = 1024 * 1024;
/* Default IP address to bind the listening socket to. */
constexpr char NET_DEFAULT_IP[] = "127.0.0.1";

/**
 * @brief Prints "pumpkin-server: <sz_ErrOp>: <strerror(errno)>" to std::cerr.
 *
 * @param sz_ErrOp The operation that failed, e.g. "bind"
 */
void net_printError(const std::string& sz_ErrOp) {
    std::cerr << "pumpkin-server: " << sz_ErrOp << ": " << std::strerror(errno) << "\n";
}

/**
 * @brief Makes recv/send/accept on the socket return immediately instead of waiting.
 *
 * @param fd_Socket The socket to change.
 *
 * @return true on success.
 */
bool net_setSocketNonBlocking(int fd_Socket) {
    /* Get the current socket status flags*/
    const int int_Flags = fcntl(fd_Socket, F_GETFL, 0);
    if (int_Flags == -1) {
        return false;
    }

    /* Set the socket to non-blocking and return */
    return fcntl(fd_Socket, F_SETFL, int_Flags | O_NONBLOCK) != -1;
}

/**
 * @brief Checks whether errno means "nothing to do right now, try again later".
 *
 * @return true for EAGAIN / EWOULDBLOCK / EINTR, false for a real error.
 */
bool net_isTryAgain() {
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
}
}  // namespace

// ================================================================================================
// GLOBAL SCOPE
// ================================================================================================
TcpServer::TcpServer(std::uint16_t u16_Port, core::KvStore& obj_Store)
    : m_u16_port(u16_Port), m_obj_store(obj_Store) {}

TcpServer::~TcpServer() {
    /* Close connections by their fd code */
    for (const auto& pair_Entry : m_map_connections) {
        close(pair_Entry.first);
    }

    /* Close the listening socket if it was opened */
    if (m_fd_listen != -1) {
        close(m_fd_listen);
    }
}

bool TcpServer::start() {
    /* Writing to a client that already disconnected raises SIGPIPE, which kills the whole process
       by default. Ignore it (SIG_IGN), so send() just returns with an error instead */
    std::signal(SIGPIPE, SIG_IGN);

    /* Create the listening socket (IPv4, TCP) */
    m_fd_listen = socket(AF_INET, SOCK_STREAM, 0);
    if (m_fd_listen == -1) {
        net_printError("socket");
        return false;
    }

    /* Allow restarting the server right away on the same port */
    const int int_Enable = 1;
    if (setsockopt(m_fd_listen, SOL_SOCKET, SO_REUSEADDR, &int_Enable, sizeof(int_Enable)) == -1) {
        net_printError("setsockopt");
        return false;
    }

    /* Bind the listening socket to the specified IP address and port */
    sockaddr_in str_Address{};
    str_Address.sin_family = AF_INET;  // IPv4
    str_Address.sin_port   = htons(m_u16_port);
    inet_pton(AF_INET, NET_DEFAULT_IP, &str_Address.sin_addr);
    if (bind(m_fd_listen, reinterpret_cast<sockaddr*>(&str_Address), sizeof(str_Address)) == -1) {
        net_printError("bind");
        return false;
    }

    /* Start listening for incoming connections */
    if (listen(m_fd_listen, SOMAXCONN) == -1) {
        net_printError("listen");
        return false;
    }

    /* Set the listening socket to non-blocking mode */
    if (!net_setSocketNonBlocking(m_fd_listen)) {
        net_printError("fcntl");
        return false;
    }

    /* Get port number bound to the socket */
    socklen_t u32_Length = sizeof(str_Address);
    if (getsockname(m_fd_listen, reinterpret_cast<sockaddr*>(&str_Address), &u32_Length) == -1) {
        net_printError("getsockname");
        return false;
    }
    m_u16_port = ntohs(str_Address.sin_port);
    return true;
}
}  // namespace pumpkin::services
