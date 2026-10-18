/**
 ******************************************************************************
 * @file    NET_TcpSocketTest.cpp
 * @author  Shangjie Zheng
 * @brief   Unit tests for TcpSocket (real sockets on 127.0.0.1).
 *          This file tests:
 *           + The fd is closed exactly once: on destruction, close(), or move
 *           + bind / listen / localPort, including port 0 and a taken port
 *           + accept / connect / send / recv, "nothing yet" and end of stream
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

#include "services/pump-srv-net/NET_TcpSocket.hpp"

#include <fcntl.h>
#include <gtest/gtest.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <string>
#include <utility>

#include "common/pump-cmn-stat/STAT_Status.hpp"

namespace pumpkin::services {
namespace {

/**
 * @brief Checks whether a file descriptor is still open in this process.
 */
bool net_isFdOpen(int fd_Socket) {
    return fcntl(fd_Socket, F_GETFD) != -1 || errno != EBADF;
}

/**
 * @brief Opens a listening socket on 127.0.0.1 with an OS-chosen port.
 */
void net_openListener(TcpSocket* ptr_Listener, uint16_t* ptr_Port) {
    ASSERT_EQ(ptr_Listener->open(), P_OK);
    ASSERT_EQ(ptr_Listener->bind("127.0.0.1", 0), P_OK);
    ASSERT_EQ(ptr_Listener->listen(), P_OK);
    ASSERT_EQ(ptr_Listener->localPort(ptr_Port), P_OK);
}

TEST(TcpSocket, NewSocketIsNotOpen) {
    TcpSocket obj_Socket;
    EXPECT_FALSE(obj_Socket.isOpen());
    EXPECT_EQ(obj_Socket.fd(), -1);
    EXPECT_EQ(obj_Socket.listen(), -P_ENOTINIT);
}

TEST(TcpSocket, DestructorClosesTheFd) {
    int fd_Socket = -1;
    {
        TcpSocket obj_Socket;
        ASSERT_EQ(obj_Socket.open(), P_OK);
        fd_Socket = obj_Socket.fd();
        EXPECT_TRUE(net_isFdOpen(fd_Socket));
    }  // obj_Socket destroyed here
    EXPECT_FALSE(net_isFdOpen(fd_Socket));
}

TEST(TcpSocket, OpenTwiceIsAlready) {
    TcpSocket obj_Socket;
    ASSERT_EQ(obj_Socket.open(), P_OK);
    EXPECT_EQ(obj_Socket.open(), -P_EALREADY);
}

TEST(TcpSocket, CloseIsSafeToRepeat) {
    TcpSocket obj_Socket;
    ASSERT_EQ(obj_Socket.open(), P_OK);
    obj_Socket.close();
    obj_Socket.close();  // no double close
    EXPECT_FALSE(obj_Socket.isOpen());
}

TEST(TcpSocket, MoveHandsOverTheFd) {
    TcpSocket obj_First;
    ASSERT_EQ(obj_First.open(), P_OK);
    const int fd_Socket = obj_First.fd();

    TcpSocket obj_Second(std::move(obj_First));
    EXPECT_FALSE(obj_First.isOpen());  // NOLINT: checking the moved-from state on purpose
    EXPECT_EQ(obj_Second.fd(), fd_Socket);
    EXPECT_TRUE(net_isFdOpen(fd_Socket));
}

TEST(TcpSocket, MoveAssignClosesTheOldFd) {
    TcpSocket obj_Target;
    ASSERT_EQ(obj_Target.open(), P_OK);
    const int fd_Old = obj_Target.fd();

    TcpSocket obj_Source;
    ASSERT_EQ(obj_Source.open(), P_OK);
    const int fd_New = obj_Source.fd();

    obj_Target = std::move(obj_Source);
    EXPECT_FALSE(net_isFdOpen(fd_Old));
    EXPECT_EQ(obj_Target.fd(), fd_New);
}

TEST(TcpSocket, PortZeroGetsARealPort) {
    TcpSocket obj_Listener;
    uint16_t  u16_Port = 0;
    net_openListener(&obj_Listener, &u16_Port);
    EXPECT_GT(u16_Port, 0);
}

TEST(TcpSocket, BindingATakenPortFails) {
    TcpSocket obj_First;
    uint16_t  u16_Port = 0;
    net_openListener(&obj_First, &u16_Port);

    TcpSocket obj_Second;
    ASSERT_EQ(obj_Second.open(), P_OK);
    EXPECT_EQ(obj_Second.bind("127.0.0.1", u16_Port), -P_EADDRINUSE);
}

TEST(TcpSocket, InvalidAddressIsRejected) {
    TcpSocket obj_Socket;
    ASSERT_EQ(obj_Socket.open(), P_OK);
    EXPECT_EQ(obj_Socket.bind("not-an-ip", 0), -P_EINVARG);
    EXPECT_EQ(obj_Socket.bind(nullptr, 0), -P_EINVARG);
}

TEST(TcpSocket, NonBlockingAcceptWithNobodyWaiting) {
    TcpSocket obj_Listener;
    uint16_t  u16_Port = 0;
    net_openListener(&obj_Listener, &u16_Port);
    ASSERT_EQ(obj_Listener.setNonBlocking(), P_OK);

    TcpSocket obj_Client;
    EXPECT_EQ(obj_Listener.accept(&obj_Client), P_OK);
    EXPECT_FALSE(obj_Client.isOpen());  // nobody was waiting
}

TEST(TcpSocket, ConnectSendRecvAndEndOfStream) {
    TcpSocket obj_Listener;
    uint16_t  u16_Port = 0;
    net_openListener(&obj_Listener, &u16_Port);

    /* Client connects; the connection waits in the backlog until accepted */
    TcpSocket obj_Client;
    ASSERT_EQ(obj_Client.open(), P_OK);
    ASSERT_EQ(obj_Client.connect("127.0.0.1", u16_Port), P_OK);

    TcpSocket obj_Server;
    ASSERT_EQ(obj_Listener.accept(&obj_Server), P_OK);  // blocking listener: waits for the client
    ASSERT_TRUE(obj_Server.isOpen());

    /* Client → server */
    const std::string sz_Message = "PING\n";
    size_t            siz_Sent   = 0;
    ASSERT_EQ(obj_Client.send(sz_Message.data(), sz_Message.size(), &siz_Sent), P_OK);
    EXPECT_EQ(siz_Sent, sz_Message.size());

    std::array<char, 64> arr_Buffer{};
    size_t               siz_Received   = 0;
    bool                 bol_PeerClosed = false;
    ASSERT_EQ(obj_Server.recv(arr_Buffer.data(), arr_Buffer.size(), &siz_Received, &bol_PeerClosed),
              P_OK);
    EXPECT_FALSE(bol_PeerClosed);
    EXPECT_EQ(std::string(arr_Buffer.data(), siz_Received), sz_Message);

    /* Client closes: the server sees end of stream, not an error */
    obj_Client.close();
    ASSERT_EQ(obj_Server.recv(arr_Buffer.data(), arr_Buffer.size(), &siz_Received, &bol_PeerClosed),
              P_OK);
    EXPECT_TRUE(bol_PeerClosed);
    EXPECT_EQ(siz_Received, 0u);
}

}  // namespace
}  // namespace pumpkin::services
