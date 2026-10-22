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
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0. See the LICENSE file in the
 * root directory of this repository for the full license text.
 ******************************************************************************
 */

#include "services/pump-srv-net/NET_TcpServer.hpp"

#include <poll.h>

#include <array>
#include <cerrno>
#include <csignal>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "common/pump-cmn-log/LOG_Logger.hpp"
#include "common/pump-cmn-stat/STAT_Status.hpp"
#include "middlewares/pump-mid-cde/CDE_Dispatcher.hpp"
#include "middlewares/pump-mid-pwp/PWP_Codec.hpp"

LOG_MODULE_REGISTER(NET, LOG_LEVEL_DBG)

namespace pumpkin::services {

namespace {
// ================================================================================================
// PRIVATE SCOPE
// ================================================================================================

constexpr int    NET_POLL_TIMEOUT_MS  = 100;
constexpr size_t NET_READ_CHUNK_SIZE  = 4096;
constexpr size_t NET_MAX_WRITE_BUFFER = 1024 * 1024;

std::unique_ptr<TcpServer> net_ptr_server;           // app's front door server
std::atomic<TcpServer*>    net_atm_server{nullptr};  // same server, for NET_StopServer()
}  // namespace

// ================================================================================================
// GLOBAL SCOPE
// ================================================================================================
TcpServer::TcpServer(const NetServerConfig& str_NetConfig, core::KvStore& obj_Store)
    : m_sz_bindIp(str_NetConfig.sz_BindIp),
      m_u16_port(str_NetConfig.u16_Port),
      m_obj_store(obj_Store) {}

int TcpServer::start() {
    if (m_obj_listen.isOpen()) {
        LOG_WRN("start() called on a server that is already started");
        return -P_EALREADY;
    }

    /* Writing to a client that already disconnected raises SIGPIPE, which kills the whole process
       by default. Ignore it (SIG_IGN), so send() just returns with an error instead */
    std::signal(SIGPIPE, SIG_IGN);

    const int int_Result = openListenSocket();
    if (int_Result < 0) {
        m_obj_listen.close();
    }

    return int_Result;
}

int TcpServer::run() {
    /* Not started bro... */
    if (!m_obj_listen.isOpen()) {
        return -P_ENOTINIT;
    }

    while (!m_atm_stopRequested) {
        /* Build the list of sockets to watch */
        std::vector<pollfd> vec_PollFds;
        vec_PollFds.push_back(pollfd{m_obj_listen.fd(), POLLIN, 0});
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
            LOG_ERR("poll() failed: %s (errno %d)", STAT_ToString(STAT_MapErrno(errno)), errno);
            return -P_EIO;  // Exit the run loop on poll error
        }

        /* Handle ready sockets */
        std::vector<int> vec_SocketsToClose;
        for (const pollfd& str_PollFd : vec_PollFds) {
            if (str_PollFd.revents == 0) {
                continue;
            }
            if (str_PollFd.fd == m_obj_listen.fd()) {
                acceptConnections();
                continue;
            }

            Connection& str_Connection = m_map_connections.at(str_PollFd.fd);
            bool        bol_KeepOpen   = (str_PollFd.revents & (POLLERR | POLLNVAL)) == 0;

            /* Read if data arrived (POLLIN) or the peer hung up (POLLHUP) */
            if (bol_KeepOpen && !str_Connection.bol_CloseAfterWrite &&
                (str_PollFd.revents & (POLLIN | POLLHUP)) != 0) {
                const int int_Result = readFrom(str_PollFd.fd, str_Connection);
                if (int_Result < 0) {
                    LOG_DBG("client fd %d: read failed: %s", str_PollFd.fd,
                            STAT_ToString(-int_Result));
                }
                bol_KeepOpen = (int_Result == P_OK);
            }
            /* Write to the client if there's data in the write buffer */
            if (bol_KeepOpen && !str_Connection.sz_WriteBuffer.empty()) {
                const int int_Result = writeTo(str_Connection);
                if (int_Result < 0) {
                    LOG_DBG("client fd %d: write failed: %s", str_PollFd.fd,
                            STAT_ToString(-int_Result));
                }
                bol_KeepOpen = (int_Result == P_OK);
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

    return P_OK;
}

void TcpServer::stop() {
    m_atm_stopRequested = true;
}

uint16_t TcpServer::port() const {
    return m_u16_port;
}

// ================================================================================================
// PRIVATE SCOPE
// ================================================================================================
int TcpServer::openListenSocket() {
    /* Create the listening socket (IPv4, TCP) */
    int int_Ret = m_obj_listen.open();
    if (int_Ret < 0) {
        LOG_ERR("socket() failed: %s (errno %d)", STAT_ToString(-int_Ret), errno);
        return int_Ret;
    }

    /* Allow restarting the server right away on the same port */
    int_Ret = m_obj_listen.setReuseAddress();
    if (int_Ret < 0) {
        LOG_ERR("setsockopt() failed: %s (errno %d)", STAT_ToString(-int_Ret), errno);
        return int_Ret;
    }

    /* Bind the listening socket to the specified IP address and port */
    int_Ret = m_obj_listen.bind(m_sz_bindIp.c_str(), m_u16_port);
    if (int_Ret == -P_EINVARG) {
        LOG_ERR("invalid IP address %s: %s", m_sz_bindIp.c_str(), STAT_ToString(P_EINVARG));
        return int_Ret;
    }
    if (int_Ret < 0) {
        LOG_ERR("bind() to %s:%hu failed: %s (errno %d)", m_sz_bindIp.c_str(), m_u16_port,
                STAT_ToString(-int_Ret), errno);
        return int_Ret;
    }

    /* Start listening for incoming connections */
    int_Ret = m_obj_listen.listen();
    if (int_Ret < 0) {
        LOG_ERR("listen() failed: %s (errno %d)", STAT_ToString(-int_Ret), errno);
        return int_Ret;
    }

    /* Set the listening socket to non-blocking mode */
    int_Ret = m_obj_listen.setNonBlocking();
    if (int_Ret < 0) {
        LOG_ERR("fcntl() on the listening socket failed: %s (errno %d)", STAT_ToString(-int_Ret),
                errno);
        return int_Ret;
    }

    /* Get the port actually bound to */
    int_Ret = m_obj_listen.localPort(&m_u16_port);
    if (int_Ret < 0) {
        LOG_ERR("getsockname() failed: %s (errno %d)", STAT_ToString(-int_Ret), errno);
        return int_Ret;
    }

    LOG_INF("listening on %s:%hu", m_sz_bindIp.c_str(), m_u16_port);
    if (m_sz_bindIp != NET_DEFAULT_BIND_IP) {
        LOG_WRN(
            "bound to %s, not localhost: other machines can connect, and there is no "
            "authentication yet",
            m_sz_bindIp.c_str());
    }
    return P_OK;
}

void TcpServer::acceptConnections() {
    /* Several clients may be waiting to connect, accept til there are none left */
    while (true) {
        TcpSocket obj_Client;
        const int int_Ret = m_obj_listen.accept(&obj_Client);
        if (int_Ret < 0) {
            LOG_ERR("accept() failed: %s (errno %d)", STAT_ToString(-int_Ret), errno);
            return;
        }
        if (!obj_Client.isOpen()) {
            return;  // No client in the backlog to accept.
        }

        /* Configure the client socket */
        const int fd_Client = obj_Client.fd();
        const int int_Res   = obj_Client.setNonBlocking();
        if (int_Res < 0) {
            LOG_ERR("fcntl() on client fd %d failed: %s (errno %d)", fd_Client,
                    STAT_ToString(-int_Res), errno);
            return;  // obj_Client closes the fd when it goes out of scope.
        }

        /* Successfully accepted and configured a new client socket */
        Connection str_Connection;
        str_Connection.obj_Socket = std::move(obj_Client);
        m_map_connections.emplace(fd_Client, std::move(str_Connection));
        LOG_DBG("client fd %d connected", fd_Client);
    }
}

int TcpServer::readFrom(int fd_Client, Connection& str_Connection) {
    std::array<char, NET_READ_CHUNK_SIZE> arr_Chunks;
    size_t                                siz_Received   = 0;
    bool                                  bol_PeerClosed = false;

    const int int_Ret = str_Connection.obj_Socket.recv(arr_Chunks.data(), arr_Chunks.size(),
                                                       &siz_Received, &bol_PeerClosed);
    if (int_Ret < 0) {
        return int_Ret;
    }
    if (bol_PeerClosed) {
        str_Connection.bol_PeerClosed = true;
        return P_OK;
    }
    if (siz_Received == 0) {
        return P_OK;  // nothing to read right now
    }

    /* Append the received data to the connection's read buffer */
    str_Connection.sz_ReadBuffer.append(arr_Chunks.data(), siz_Received);

    /* Process any complete lines that have been received per write buffer allows */
    processPendingLines(str_Connection);

    /* Bytes after the last newline are unfinished line. If it is already too long, drop this client
       (complete lines that are only waiting for write-buffer space don't count) */
    const size_t siz_LastNewLineIdx = str_Connection.sz_ReadBuffer.rfind('\n');
    const size_t siz_Unfinished =
        (siz_LastNewLineIdx == std::string::npos)
            ? str_Connection.sz_ReadBuffer.size()
            : str_Connection.sz_ReadBuffer.size() - (siz_LastNewLineIdx + 1);

    if (siz_Unfinished > middlewares::PWP_MAX_LINE_LENGTH) {
        LOG_WRN("client fd %d: line longer than %zu bytes, closing", fd_Client,
                middlewares::PWP_MAX_LINE_LENGTH);
        str_Connection.sz_ReadBuffer.clear();
        str_Connection.sz_WriteBuffer += middlewares::PWP_EncodeReply(
            middlewares::Reply{middlewares::ReplyType::REPLY_ERROR, "line too long", 0});
        str_Connection.bol_CloseAfterWrite = true;
    }

    return P_OK;
}

int TcpServer::writeTo(Connection& str_Connection) {
    size_t    siz_NumBytesSent = 0;
    const int int_Ret =
        str_Connection.obj_Socket.send(str_Connection.sz_WriteBuffer.data(),
                                       str_Connection.sz_WriteBuffer.size(), &siz_NumBytesSent);
    if (int_Ret < 0) {
        return int_Ret;
    }

    /* Drop what was sent, keep the rest */
    str_Connection.sz_WriteBuffer.erase(0, siz_NumBytesSent);

    /* send() freed space in the write buffer: run any commands still pending in the read buffer and
       queue their replies */
    processPendingLines(str_Connection);

    return P_OK;
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
    LOG_DBG("client fd %d closed", fd_Client);
    m_map_connections.erase(fd_Client);
}

// ================================================================================================
// GLOBAL SCOPE
// ================================================================================================
int NET_InitServer(core::KvStore* ptr_Store) {
    if (net_ptr_server != nullptr) {
        LOG_WRN("NET_InitServer() called while a server already exists");
        return -P_EALREADY;
    }
    if (ptr_Store == nullptr) {
        LOG_ERR("NET_InitServer() called with a NULL data store");
        return -P_EINVARG;
    }

    /* Instantiate and attempt to start server */
    const NetServerConfig& str_Config    = NET_GetServerConfig();
    auto                   ptr_NewServer = std::make_unique<TcpServer>(str_Config, *ptr_Store);
    const int              int_Ret       = ptr_NewServer->start();
    if (int_Ret < 0) {
        if (int_Ret == -P_EADDRINUSE) {
            LOG_ERR("port %hu is in use; pick another with --port, or --port 0 for any free port",
                    str_Config.u16_Port);
        }
        return int_Ret;
    }

    /* Server started, link it to the global pointer */
    net_ptr_server = std::move(ptr_NewServer);
    net_atm_server.store(net_ptr_server.get());

    return P_OK;
}

int NET_RunServer() {
    if (net_ptr_server == nullptr) {
        return -P_ENOTINIT;
    }

    return net_ptr_server->run();
}

void NET_StopServer() {
    /* Signal the server to stop */
    TcpServer* ptr_AtomServer = net_atm_server.load();
    if (ptr_AtomServer != nullptr) {
        ptr_AtomServer->stop();
    }
}

void NET_DeinitServer() {
    /* Stop the atomic pointer first to avoid race conditions with other threads accessing the
       actual server */
    net_atm_server.store(nullptr);

    /* Stop the actual server */
    net_ptr_server.reset();
}

}  // namespace pumpkin::services
