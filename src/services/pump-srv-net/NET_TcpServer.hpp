/**
 ******************************************************************************
 * @file    NET_TcpServer.hpp
 * @author  Shangjie Zheng
 * @brief   Single-threaded TCP server: public interface.
 *          This file declares:
 *           + NetConnId_t / NetReceiveHandler_t: a client's id and the app's
 *             "client has input" callback
 *           + TcpServer: accepts clients on its configured address and answers PWP
 *             (Pumpkin Wire Protocol) requests
 *           + NET_Init / NET_RunServer / NET_StopServer / NET_DeinitServer: the
 *             app's single server, owned by NET (front door)
 *           + NET_Receive / NET_Send / NET_Close / NET_GetPort: what the handler uses
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

#include "services/pump-srv-net/NET_ServerConfig.hpp"
#include "services/pump-srv-net/NET_TcpSocket.hpp"

namespace pumpkin::services {

using NetConnId_t         = uint64_t;
using NetReceiveHandler_t = void (*)(NetConnId_t u64_ConnId);

class TcpServer {
public:
    /**
     * @brief Creates a server. Nothing is opened until start() is called.
     *
     * @param str_NetConfig The network configuration containing the IP and port to bind to.
     */
    explicit TcpServer(const NetServerConfig& str_NetConfig);

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
     * @param fn_OnReceive Called whenever a client has data waiting and there is room to reply.
     *
     * @return P_OK if it stopped because stop() was called; -P_ENOTINIT if start() was never
     *         called; -P_EINVARG for a NULL handler; -P_EIO if poll() failed.
     */
    int run(NetReceiveHandler_t fn_OnReceive);

    /**
     * @brief Asks run() to return. Safe to call from another thread.
     */
    void stop();

    /**
     * @brief Gives the bytes a client has sent that nobody has used yet. The caller removes
     *        what it uses (e.g. one complete request) and leaves the rest for later.
     *
     * @param u64_ConnId The connection.
     * @param ptr_Buff   Receives a pointer to those bytes. Only valid until the handler returns.
     *
     * @return P_OK; -P_EINVARG for a NULL pointer or a connection that no longer exists.
     */
    int receive(NetConnId_t u64_ConnId, std::string** ptr_InBuff);

    /**
     * @brief Queues bytes for a client; they are sent as soon as the socket accepts them.
     *
     * @param u64_ConnId The connection.
     * @param sz_Data    The bytes to send.
     *
     * @return P_OK; -P_EINVARG for a connection that no longer exists or is closing.
     */
    int send(NetConnId_t u64_ConnId, const std::string& sz_Data);

    /**
     * @brief Closes a connection once everything already queued is sent. Anything the client
     *        sent that was not used yet is dropped.
     *
     * @param u64_ConnId The connection.
     *
     * @return P_OK; -P_EINVARG for a connection that no longer exists.
     */
    int close(NetConnId_t u64_ConnId);

    /**
     * @brief Gets the port the server is listening on.
     *
     * @return The port number the server is listening on.
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
    int  readFrom(NetConnId_t u64_ConnId, Connection& str_Connection);
    int  writeTo(NetConnId_t u64_ConnId, Connection& str_Connection);
    void handleInput(NetConnId_t u64_ConnId, Connection& str_Connection);
    void closeFinishedConnections(NetConnId_t u64_ConnId);

    std::string                       m_sz_bindIp;
    uint16_t                          m_u16_port;
    TcpSocket                         m_obj_listen;
    std::map<NetConnId_t, Connection> m_map_connections;  // key Connections by NetConnId_t
    NetConnId_t                       m_u64_nextConnId = 1;
    NetReceiveHandler_t               m_fn_onReceive   = nullptr;
    std::atomic<bool>                 m_atm_stopRequested{false};
};

// ================================================================================================
// GLOBAL SCOPE
// ================================================================================================

/**
 * @brief Creates the app's server with the given settings and opens its listening socket.
 *
 * @param str_NetConfig The address and port to listen on (port 0 = any free port).
 * @param fn_OnReceive  Called whenever a client has data waiting (see NetReceiveHandler_t).
 *
 * @return P_OK on success; -P_EALREADY if a server already exists; -P_EINVARG for a NULL handler
 *         or a bad IP address; another negative P_E* code if the socket could not be opened.
 */
int NET_Init(const NetServerConfig& str_NetConfig, NetReceiveHandler_t fn_OnReceive);

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
 * @brief Gives the bytes a client has sent that nobody has used yet (see TcpServer::receive()).
 *
 * @param u64_ConnId The connection the handler was called for.
 * @param ptr_InBuff Receives a pointer to those bytes. Only valid until the handler returns.
 *
 * @return P_OK; -P_ENOTINIT if no server exists; -P_EINVARG for a NULL pointer or an unknown
 *         connection.
 */
int NET_Receive(NetConnId_t u64_ConnId, std::string** ptr_InBuff);

/**
 * @brief Queues bytes for a client (see TcpServer::send()). Call it only from the receive handler.
 *
 * @param u64_ConnId The connection.
 * @param sz_Data    The bytes to send.
 *
 * @return P_OK; -P_ENOTINIT if no server exists; -P_EINVARG for an unknown or closing connection.
 */
int NET_Send(NetConnId_t u64_ConnId, const std::string& sz_Data);

/**
 * @brief Closes a client once its queued bytes are sent.
 *
 * @param u64_ConnId The connection to close.
 *
 * @return P_OK; -P_ENOTINIT if no server exists; -P_EINVARG for an unknown connection.
 */
int NET_Close(NetConnId_t u64_ConnId);

/**
 * @brief Gets the port the app's server is listening on.
 *
 * @param ptr_Port Receives the port.
 *
 * @return P_OK; -P_ENOTINIT if no server exists; -P_EINVARG for a NULL pointer.
 */
int NET_GetPort(uint16_t* ptr_Port);

/**
 * @brief Destroys the app's server, closing every socket. Call it before main() returns, while the
 *        logger still exists. Harmless if no server exists.
 */
void NET_DeinitServer();

}  // namespace pumpkin::services

#endif  // PUMPKIN_NET_TCPSERVER_HPP_
