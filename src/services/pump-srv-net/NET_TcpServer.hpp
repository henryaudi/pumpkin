/**
 ******************************************************************************
 * @file    NET_TcpServer.hpp
 * @author  Shangjie Zheng
  * @brief   Single-threaded TCP server: public interface.
 *          This file declares:
 *           + TcpServer: accepts clients on its configured address and answers PWP
 *             (Pumpkin Wire Protocol) requests
 *           + NET_Init / NET_RunServer / NET_StopServer / NET_DeinitServer: the
 *             app's single server, owned by NET (front door)
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

#ifndef PUMPKIN_NET_TCPSERVER_HPP_
#define PUMPKIN_NET_TCPSERVER_HPP_

#include <atomic>
#include <cstdint>
#include <map>
#include <string>

#include "core/pump-core-eng/ENG_KvStore.hpp"
#include "services/pump-srv-net/NET_ServerConfig.hpp"
#include "services/pump-srv-net/NET_TcpSocket.hpp"

namespace pumpkin::services {

class TcpServer {
public:
    /**
     * @brief Creates a server. Nothing is opened until start() is called.
     *
     * @param str_NetConfig The network configuration containing the IP and port to bind to.
     * @param obj_Store The store that commands run against. Must outlive the server instance.
     */
    TcpServer(const NetServerConfig& str_NetConfig, core::KvStore& obj_Store);

    /**
     * @brief Closes the listening socket and all client connections (the new OOP style enables
     *        delegating resource destruction by objects themselves).
     */
    ~TcpServer() = default;

    TcpServer(const TcpServer&)            = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    /**
     * @brief Opens the listening socket. If it fails, nothing is left open, so start() can be
     *        safely retried.
     *
     * @return P_OK on success; -P_EALREADY if already started; -P_EADDRINUSE / -P_EACCES if bind()
     *         failed; -P_EINVARG for a bad IP address; another negative P_E* code if a different
     *         socket call failed.
     */
    int start();

    /**
     * @brief Serves clients until stop() is called.
     *
     * @return P_OK if it stopped because stop() was called; -P_ENOTINIT if start() was never
     *         called; -P_EIO if poll() failed.
     */
    int run();

    /**
     * @brief Asks run() to return. Safe to call from another thread.
     */
    void stop();

    /**
     * @brief Gets the port the server listens on.
     *
     * @return The TCP port the server is listening on.
     */
    uint16_t port() const;

private:
    struct Connection {
        TcpSocket   obj_Socket;
        std::string sz_ReadBuffer;
        std::string sz_WriteBuffer;
        bool        bol_PeerClosed      = false;
        bool        bol_CloseAfterWrite = false;
    };

    int  openListenSocket();
    void acceptConnections();
    int  readFrom(int fd_Client, Connection& str_Connection);
    int  writeTo(Connection& str_Connection);
    void processPendingLines(Connection& str_Connection);
    void closeConnection(int fd_Client);

    std::string               m_sz_bindIp;
    uint16_t                  m_u16_port;
    core::KvStore&            m_obj_store;
    TcpSocket                 m_obj_listen;
    std::map<int, Connection> m_map_connections;  // maps client socket fd to Connection object
    std::atomic<bool>         m_atm_stopRequested{false};
};

// ================================================================================================
// GLOBAL SCOPE
// ================================================================================================

/**
 * @brief Creates the app's server from NET's current settings and opens its listening socket.
 *
 * @param ptr_Store Store that commands runs against.
 *
 * @return P_OK on success; negative P_E* code on failure.
 */
int NET_Init(core::KvStore* ptr_Store);

/**
 * @brief Serves clients until NET_StopServer() is called.
 *
 * @return P_OK once stopped; -P_ENOTINIT if NET_Init() didn't succeed; -P_EIO if poll()
 *         failed.
 */
int NET_RunServer();

/**
 * @brief Asks NET_RunServer() to return. Safe to call from a signal handler or another thread,
 *        and harmless if no server exists.
 */
void NET_StopServer();

/**
 * @brief Destroys the app's server, closing every socket. Call it before main() returns, while the
 *        logger still exists. Harmless if no server exists.
 */
void NET_DeinitServer();

}  // namespace pumpkin::services

#endif  // PUMPKIN_NET_TCPSERVER_HPP_
