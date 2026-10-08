/**
 ******************************************************************************
 * @file    NET_TcpServerTest.cpp
 * @author  Shangjie Zheng
 * @brief   End-to-end tests for the TCP server, over real sockets on 127.0.0.1.
 *          This file tests:
 *           + PING, pipelined commands, commands split across sends
 *           + Several clients sharing one store, errors not closing the connection
 *           + Lines longer than PWP_MAX_LINE_LENGTH closing the connection
 *           + Big replies beyond the write-buffer limit (backpressure)
 *           + Half-closed clients still receiving every reply
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
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <thread>

#include "middlewares/pump-mid-pwp/PWP_Codec.hpp"

namespace pumpkin::services {
namespace {

/**
 * @brief Opens a TCP connection to 127.0.0.1 on the given port.
 *
 * @return The connected socket, or -1 on failure.
 */
int net_connectTo(std::uint16_t u16_Port) {
    const int fd_Socket = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_Socket == -1) {
        return -1;
    }
    /* If the server never answers, recv() gives up after 2 seconds instead of hanging the test */
    timeval str_Timeout{};
    str_Timeout.tv_sec = 2;
    setsockopt(fd_Socket, SOL_SOCKET, SO_RCVTIMEO, &str_Timeout, sizeof(str_Timeout));

    sockaddr_in str_Address{};
    str_Address.sin_family = AF_INET;
    str_Address.sin_port   = htons(u16_Port);
    inet_pton(AF_INET, "127.0.0.1", &str_Address.sin_addr);
    if (connect(fd_Socket, reinterpret_cast<sockaddr*>(&str_Address), sizeof(str_Address)) == -1) {
        close(fd_Socket);
        return -1;
    }
    return fd_Socket;
}

/** @brief Sends all of the text, even if the OS accepts it in several pieces. */
void net_sendAll(int fd_Socket, const std::string& sz_Text) {
    std::size_t siz_Total = 0;
    while (siz_Total < sz_Text.size()) {
        const ssize_t ssiz_Sent =
            send(fd_Socket, sz_Text.data() + siz_Total, sz_Text.size() - siz_Total, 0);
        if (ssiz_Sent <= 0) {
            return;
        }
        siz_Total += static_cast<std::size_t>(ssiz_Sent);
    }
}

/** @brief Reads until the given number of complete lines arrived (or the 2 s timeout passes). */
std::string net_readLines(int fd_Socket, int int_LineCount) {
    std::string  sz_Received;
    std::int64_t s64_Lines = 0;
    while (s64_Lines < int_LineCount) {
        std::array<char, 4096> arr_Chunk;
        const ssize_t          ssiz_Read = recv(fd_Socket, arr_Chunk.data(), arr_Chunk.size(), 0);
        if (ssiz_Read <= 0) {
            break;
        }
        const std::size_t siz_Read = static_cast<std::size_t>(ssiz_Read);
        s64_Lines += std::count(arr_Chunk.data(), arr_Chunk.data() + siz_Read, '\n');
        sz_Received.append(arr_Chunk.data(), siz_Read);
    }
    return sz_Received;
}

/* Runs a server on a free port in a background thread for the duration of each test. */
class TcpServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(m_obj_server.start());
        m_thr_server = std::thread([this] { m_obj_server.run(); });
    }

    void TearDown() override {
        m_obj_server.stop();
        if (m_thr_server.joinable()) {
            m_thr_server.join();
        }
    }

    int connectClient() {
        return net_connectTo(m_obj_server.port());
    }

    core::KvStore m_obj_store;
    TcpServer     m_obj_server{0, m_obj_store};
    std::thread   m_thr_server;
};

TEST_F(TcpServerTest, PingReturnsPong) {
    const int fd_Client = connectClient();
    ASSERT_NE(fd_Client, -1);
    net_sendAll(fd_Client, "PING\n");
    EXPECT_EQ(net_readLines(fd_Client, 1), "PONG\n");
    close(fd_Client);
}

TEST_F(TcpServerTest, ConnectionStaysOpenBetweenCommands) {
    const int fd_Client = connectClient();
    ASSERT_NE(fd_Client, -1);

    /* Two separate request/reply round trips on the same connection */
    net_sendAll(fd_Client, "SET name pumpkin\n");
    EXPECT_EQ(net_readLines(fd_Client, 1), "OK\n");
    net_sendAll(fd_Client, "GET name\n");
    EXPECT_EQ(net_readLines(fd_Client, 1), "VALUE pumpkin\n");
    close(fd_Client);
}

TEST_F(TcpServerTest, PipelinedCommandsAreAnsweredInOrder) {
    const int fd_Client = connectClient();
    ASSERT_NE(fd_Client, -1);
    net_sendAll(fd_Client, "SET name pumpkin\nGET name\nDEL name\nGET name\n");
    EXPECT_EQ(net_readLines(fd_Client, 4), "OK\nVALUE pumpkin\nINT 1\nNIL\n");
    close(fd_Client);
}

TEST_F(TcpServerTest, CommandSplitAcrossSendsIsHandled) {
    const int fd_Client = connectClient();
    ASSERT_NE(fd_Client, -1);
    net_sendAll(fd_Client, "SET na");
    net_sendAll(fd_Client, "me pumpkin\nGET name\n");
    EXPECT_EQ(net_readLines(fd_Client, 2), "OK\nVALUE pumpkin\n");
    close(fd_Client);
}

TEST_F(TcpServerTest, ClientsShareTheSameStore) {
    const int fd_Writer = connectClient();
    const int fd_Reader = connectClient();
    ASSERT_NE(fd_Writer, -1);
    ASSERT_NE(fd_Reader, -1);
    net_sendAll(fd_Writer, "SET name pumpkin\n");
    EXPECT_EQ(net_readLines(fd_Writer, 1), "OK\n");
    net_sendAll(fd_Reader, "GET name\n");
    EXPECT_EQ(net_readLines(fd_Reader, 1), "VALUE pumpkin\n");
    close(fd_Writer);
    close(fd_Reader);
}

TEST_F(TcpServerTest, InvalidCommandKeepsConnectionOpen) {
    const int fd_Client = connectClient();
    ASSERT_NE(fd_Client, -1);
    net_sendAll(fd_Client, "HELLO\nPING\n");
    EXPECT_EQ(net_readLines(fd_Client, 2), "ERR Unknown command: 'HELLO'\nPONG\n");
    close(fd_Client);
}

TEST_F(TcpServerTest, TooLongLineClosesConnection) {
    const int fd_Client = connectClient();
    ASSERT_NE(fd_Client, -1);

    /* One "line" longer than the limit, with no newline */
    const std::string sz_Huge(middlewares::PWP_MAX_LINE_LENGTH + 1, 'A');
    net_sendAll(fd_Client, sz_Huge);

    /* The server must close the connection: recv() returns 0 (closed) or -1 (reset) */
    std::array<char, 16> arr_Buffer;
    const ssize_t        ssiz_Read = recv(fd_Client, arr_Buffer.data(), arr_Buffer.size(), 0);
    EXPECT_LE(ssiz_Read, 0);
    close(fd_Client);
}

/* Bug 6: blank lines get no reply at all */
TEST_F(TcpServerTest, BlankLinesGetNoReply) {
    const int fd_Client = connectClient();
    ASSERT_NE(fd_Client, -1);
    net_sendAll(fd_Client, "\n   \n\t\r\nPING\n");
    EXPECT_EQ(net_readLines(fd_Client, 1), "PONG\n");
    close(fd_Client);
}

TEST_F(TcpServerTest, BigPipelinedRepliesAllArriveDespiteWriteLimit) {
    const int fd_Client = connectClient();
    ASSERT_NE(fd_Client, -1);

    /* 40 GETs of a 60 KB value produce ~2.4 MB of replies, more than NET_MAX_WRITE_BUFFER (1 MiB).
       Send every command first and only then start reading */
    const std::string sz_Value(60000, 'v');
    const int         int_GetCount = 40;
    std::string       sz_Commands  = "SET big " + sz_Value + "\n";
    for (int int_I = 0; int_I < int_GetCount; ++int_I) {
        sz_Commands += "GET big\n";
    }
    net_sendAll(fd_Client, sz_Commands);

    std::string sz_Expected = "OK\n";
    for (int int_I = 0; int_I < int_GetCount; ++int_I) {
        sz_Expected += "VALUE " + sz_Value + "\n";
    }
    EXPECT_EQ(net_readLines(fd_Client, int_GetCount + 1), sz_Expected);
    close(fd_Client);
}

/* Bug 3: half-close */
TEST_F(TcpServerTest, HalfClosedClientStillGetsAllReplies) {
    const int fd_Client = connectClient();
    ASSERT_NE(fd_Client, -1);

    /* Like `printf ... | nc -N`: send everything, then close only our sending direction */
    net_sendAll(fd_Client, "PING\nSET a 1\nGET a\n");
    shutdown(fd_Client, SHUT_WR);

    EXPECT_EQ(net_readLines(fd_Client, 3), "PONG\nOK\nVALUE 1\n");
    close(fd_Client);
}

/* Bug 3: half-close while replies are held back by the write-buffer limit */
TEST_F(TcpServerTest, HalfClosedClientStillGetsAllBigReplies) {
    const int fd_Client = connectClient();
    ASSERT_NE(fd_Client, -1);

    const std::string sz_Value(60000, 'v');
    net_sendAll(fd_Client, "SET big " + sz_Value + "\n");
    EXPECT_EQ(net_readLines(fd_Client, 1), "OK\n");

    /* ~2.4 MB of replies are still pending when the client stops sending */
    const int   int_GetCount = 40;
    std::string sz_Commands;
    for (int int_I = 0; int_I < int_GetCount; ++int_I) {
        sz_Commands += "GET big\n";
    }
    net_sendAll(fd_Client, sz_Commands);
    shutdown(fd_Client, SHUT_WR);

    std::string sz_Expected;
    for (int int_I = 0; int_I < int_GetCount; ++int_I) {
        sz_Expected += "VALUE " + sz_Value + "\n";
    }
    EXPECT_EQ(net_readLines(fd_Client, int_GetCount), sz_Expected);
    close(fd_Client);
}

/* Bug 3: after the replies, the server closes too (no leaked connection) */
TEST_F(TcpServerTest, HalfClosedClientIsClosedAfterReplies) {
    const int fd_Client = connectClient();
    ASSERT_NE(fd_Client, -1);
    net_sendAll(fd_Client, "PING\n");
    shutdown(fd_Client, SHUT_WR);

    EXPECT_EQ(net_readLines(fd_Client, 1), "PONG\n");

    /* After the last reply, the server closes its side too: recv() reports end of stream */
    std::array<char, 16> arr_Buffer;
    const ssize_t        ssiz_Read = recv(fd_Client, arr_Buffer.data(), arr_Buffer.size(), 0);
    EXPECT_EQ(ssiz_Read, 0);
    close(fd_Client);
}

}  // namespace
}  // namespace pumpkin::services
