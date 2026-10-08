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
    if (int_Flags < 0) {
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
    if (m_fd_listen < 0) {
        net_printError("socket");
        return false;
    }

    /* Allow restarting the server right away on the same port */
    const int int_Enable = 1;
    if (setsockopt(m_fd_listen, SOL_SOCKET, SO_REUSEADDR, &int_Enable, sizeof(int_Enable)) < 0) {
        net_printError("setsockopt");
        return false;
    }

    /* Bind the listening socket to the specified IP address and port */
    sockaddr_in str_Address{};
    str_Address.sin_family = AF_INET;  // IPv4
    str_Address.sin_port   = htons(m_u16_port);
    if (inet_pton(AF_INET, NET_DEFAULT_IP, &str_Address.sin_addr) != 1) {
        std::cerr << "pumpkin-server: invalid IP address: " << NET_DEFAULT_IP << "\n";
        return false;
    }
    if (bind(m_fd_listen, reinterpret_cast<sockaddr*>(&str_Address), sizeof(str_Address)) < 0) {
        net_printError("bind");
        return false;
    }

    /* Start listening for incoming connections */
    if (listen(m_fd_listen, SOMAXCONN) < 0) {
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
    if (getsockname(m_fd_listen, reinterpret_cast<sockaddr*>(&str_Address), &u32_Length) < 0) {
        net_printError("getsockname");
        return false;
    }
    m_u16_port = ntohs(str_Address.sin_port);
    return true;
}

void TcpServer::run() {
    while (!m_atm_stopRequested) {
        /* Build the list of sockets to watch */
        std::vector<pollfd> vec_PollFds;
        vec_PollFds.push_back(pollfd{m_fd_listen, POLLIN, 0});
        for (const auto& pair_Entry : m_map_connections) {
            const Connection& str_Connection = pair_Entry.second;
            int               int_Events     = 0;

            /* If unsent replies are below the limit, ask poll() to notify us when the client sends
               more commands. Otherwise stop reading until it catches up (backpressure) */
            if (!str_Connection.bol_PeerClosed && !str_Connection.bol_CloseAfterWrite &&
                str_Connection.sz_WriteBuffer.size() < NET_MAX_WRITE_BUFFER) {
                int_Events |= POLLIN;
            }
            /* If there's still content in the write buffer, ask poll() to tell us when the socket
               can send without waiting */
            if (!str_Connection.sz_WriteBuffer.empty()) {
                int_Events |= POLLOUT;
            }
            /* Add this connection to the list of sockets to watch */
            vec_PollFds.push_back(pollfd{pair_Entry.first, static_cast<short>(int_Events), 0});
        }

        /* Sleep until a socket is ready or the timeout passes */
        const int int_Ready =
            poll(vec_PollFds.data(), static_cast<nfds_t>(vec_PollFds.size()), NET_POLL_TIMEOUT_MS);
        if (int_Ready < 0) {
            if (errno == EINTR) {
                continue;  // Interrupted by signal, retry poll()
            }
            net_printError("poll");
            return;  // Exit the run loop on poll error
        }

        /* Handle ready sockets */
        std::vector<int> vec_SocketsToClose;
        for (const pollfd& str_PollFd : vec_PollFds) {
            if (str_PollFd.revents == 0) {
                continue;
            }
            if (str_PollFd.fd == m_fd_listen) {
                acceptConnections();
                continue;
            }

            Connection& str_Connection = m_map_connections.at(str_PollFd.fd);
            bool        bol_KeepOpen   = (str_PollFd.revents & (POLLERR | POLLNVAL)) == 0;

            /* Read from the client if it's readable or hung up waiting for recv (POLLHUP) */
            if (bol_KeepOpen && !str_Connection.bol_CloseAfterWrite &&
                (str_PollFd.revents & (POLLIN | POLLHUP)) != 0) {
                bol_KeepOpen = readFrom(str_PollFd.fd, str_Connection);
            }
            /* Write to the client if there's data in the write buffer */
            if (bol_KeepOpen && !str_Connection.sz_WriteBuffer.empty()) {
                bol_KeepOpen = writeTo(str_PollFd.fd, str_Connection);
            }
            /* Close the connection if the peer has closed it or we are supposed to close after
             * writing, and the write buffer is empty */
            if (bol_KeepOpen &&
                (str_Connection.bol_PeerClosed || str_Connection.bol_CloseAfterWrite) &&
                str_Connection.sz_WriteBuffer.empty()) {
                bol_KeepOpen = false;
            }
            if (!bol_KeepOpen) {
                vec_SocketsToClose.push_back(str_PollFd.fd);
            }
        }

        for (const int fd_Client : vec_SocketsToClose) {
            closeConnection(fd_Client);
        }
    }
}

void TcpServer::stop() {
    m_atm_stopRequested = true;
}

std::uint16_t TcpServer::port() const {
    return m_u16_port;
}

// ================================================================================================
// PRIVATE SCOPE
// ================================================================================================
void TcpServer::acceptConnections() {
    /* Several clients may be waiting to connect, accept til there are none left */
    while (true) {
        const int fd_Client = accept(m_fd_listen, nullptr, nullptr);

        if (fd_Client < 0) {
            if (!net_isTryAgain()) {
                net_printError("accept");
            }
            return;
        }

        if (!net_setSocketNonBlocking(fd_Client)) {
            net_printError("fcntl");
            close(fd_Client);
            return;
        }

        /* Successfully accepted and configured a new client socket */
        m_map_connections[fd_Client] = Connection{};
    }
}

bool TcpServer::readFrom(int fd_Client, Connection& str_Connection) {
    std::array<char, NET_READ_CHUNK_SIZE> arr_Chunks;
    const ssize_t ssiz_Received = recv(fd_Client, &arr_Chunks[0], arr_Chunks.size(), 0);
    if (ssiz_Received == 0) {  // EOF
        str_Connection.bol_PeerClosed = true;
        return true;
    }
    if (ssiz_Received < 0) {
        return net_isTryAgain();
    }

    /* Append the received data to the connection's read buffer */
    str_Connection.sz_ReadBuffer.append(arr_Chunks.data(), static_cast<std::size_t>(ssiz_Received));

    /* Process any complete lines that have been received per write buffer allows */
    processPendingLines(str_Connection);

    /* Bytes after the last newline are unfinished line. If it is already too long, drop this client
       (complete lines that are only waiting for write-buffer space don't count) */
    const std::size_t siz_LastNewLineIdx = str_Connection.sz_ReadBuffer.rfind('\n');
    const std::size_t siz_Unfinished =
        (siz_LastNewLineIdx == std::string::npos)
            ? str_Connection.sz_ReadBuffer.size()
            : str_Connection.sz_ReadBuffer.size() - (siz_LastNewLineIdx + 1);

    if (siz_Unfinished > middlewares::PWP_MAX_LINE_LENGTH) {
        /* Tell the client why, then close once the error has been sent. The rest of its input is
         * dropped - the connection is closing anyway */
        str_Connection.sz_ReadBuffer.clear();
        str_Connection.sz_WriteBuffer += middlewares::PWP_EncodeReply(
            middlewares::Reply{middlewares::ReplyType::REPLY_ERROR, "line too long", 0});
        str_Connection.bol_CloseAfterWrite = true;
    }

    return true;
}

bool TcpServer::writeTo(int fd_Client, Connection& str_Connection) {
    const ssize_t ssiz_Sent = send(fd_Client, str_Connection.sz_WriteBuffer.data(),
                                   str_Connection.sz_WriteBuffer.size(), 0);
    if (ssiz_Sent < 0) {
        return net_isTryAgain();
    }

    /* send() may accept only part of the data: drop what was sent, keep the rest */
    str_Connection.sz_WriteBuffer.erase(0, static_cast<std::size_t>(ssiz_Sent));

    /* send() freed space in the write buffer: run any commands still waiting in the read buffer
       and queue their replies */
    processPendingLines(str_Connection);

    return true;
}

void TcpServer::processPendingLines(Connection& str_Connection) {
    /* Answer complete lines until no line is left, or until the unsent replies reach the limit.
       Lines not answered yet stay in sz_ReadBuffer until writeTo() free space. */
    while (str_Connection.sz_WriteBuffer.size() < NET_MAX_WRITE_BUFFER) {
        const std::optional<std::string> opt_Line =
            middlewares::PWP_ExtractLine(str_Connection.sz_ReadBuffer);
        if (!opt_Line.has_value()) {
            return;
        }
        str_Connection.sz_WriteBuffer +=
            middlewares::CDE_ProcessLine(opt_Line.value(), m_obj_store);
    }
}

void TcpServer::closeConnection(int fd_Client) {
    close(fd_Client);  // close the socket file descriptor
    m_map_connections.erase(fd_Client);
}

}  // namespace pumpkin::services
