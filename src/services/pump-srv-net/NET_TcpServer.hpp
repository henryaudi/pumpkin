/**
 ******************************************************************************
 * @file    NET_TcpServer.hpp
 * @author  Shangjie Zheng
 * @brief   Single-threaded TCP server: public interface.
 *          This file declares:
 *           + TcpServer: accepts clients on 127.0.0.1 and answers PWP (Pumpkin
 *             Wire Protocol) requests.
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

#ifndef PUMPKIN_NET_TCPSERVER_HPP_
#define PUMPKIN_NET_TCPSERVER_HPP_

#include <atomic>
#include <cstdint>
#include <map>
#include <string>

#include "core/pump-core-eng/ENG_KvStore.hpp"

namespace pumpkin::services {

class TcpServer {
public:
    /**
     * @brief Creates a server. Nothing is opened until start() is called.
     *
     * @param u16_Port  TCP port to listen on. 0 lets the OS choose a free port (see port()).
     * @param obj_Store The store that commands run against. Must outlive the server instance.
     */
    TcpServer(std::uint16_t u16_Port, core::KvStore& obj_Store);

    /**
     * @brief Closes the listening socket and all client connections.
     */
    ~TcpServer();

    TcpServer(const TcpServer&)            = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    /**
     * @brief Opens the listening socket.
     *
     * @return true on success; false if the socket could not be opened.
     */
    bool start();

    /**
     * @brief Serves clients until stop() is called.
     *
     * @return true if it stopped because stop() was called; false on a fatal poll() error.
     */
    bool run();

    /**
     * @brief Asks run() to return. Safe to call from another thread.
     */
    void stop();

    /**
     * @brief Gets the port the server listens on.
     *
     * @return The TCP port the server is listening on.
     */
    std::uint16_t port() const;

private:
    struct Connection {
        std::string sz_ReadBuffer;
        std::string sz_WriteBuffer;
        bool        bol_PeerClosed      = false;
        bool        bol_CloseAfterWrite = false;
    };

    void acceptConnections();
    bool readFrom(int fd_Client, Connection& str_Connection);
    bool writeTo(int fd_Client, Connection& str_Connection);
    void processPendingLines(Connection& str_Connection);
    void closeConnection(int fd_Client);

    std::uint16_t             m_u16_port;
    core::KvStore&            m_obj_store;
    int                       m_fd_listen = -1;   // listening socket file descriptor (fd)
    std::map<int, Connection> m_map_connections;  // open connections by socket fd
    std::atomic<bool>         m_atm_stopRequested{false};
};

}  // namespace pumpkin::services

#endif  // PUMPKIN_NET_TCPSERVER_HPP_
