/**
 ******************************************************************************
 * @file    main.cpp
 * @author  Shangjie Zheng
 * @brief   Entry point of the pumpkin-server executable.
 *          This file provides:
 *           + main(): starts the server (currently a placeholder that prints
 *           +   the program name)
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

#include <atomic>
#include <charconv>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>

#include "core/pump-core-eng/ENG_KvStore.hpp"
#include "services/pump-srv-net/NET_TcpServer.hpp"

namespace {

constexpr std::uint16_t SRV_DEFAULT_PORT = 7379;  // Default port for the pumpkin-server
std::atomic<pumpkin::services::TcpServer*> srv_atm_server{nullptr};

/**
 * @brief Called by the OS on Ctrl+C (SIGINT) or `kill` (SIGTERM): asks the server to stop.
 *
 * @details A signal handler may only do async-signal-safe work. stop() just sets an atomic flag;
 *          run() notices it within NET_POLL_TIMEOUT_MS and returns normally.
 *
 * @param int_Signal The signal number (unused).
 */
void srv_handleStopSignal([[maybe_unused]] int int_Signal) {
    pumpkin::services::TcpServer* ptr_Server = srv_atm_server.load();
    if (ptr_Server != nullptr) {
        ptr_Server->stop();
    }
}

/**
 * @brief Parses a TCP port number.
 *
 * @param sz_Text The text to parse, e.g. argv[1].
 *
 * @return The port, or std::nullopt unless the text is only digits and at most 65535.
 */
std::optional<std::uint16_t> srv_parsePort(const std::string& sz_Text) {
    /* 65535 is the maximum valid port number */
    if (sz_Text.empty() || sz_Text.size() > 5) {
        return std::nullopt;
    }

    for (const char ch_Char : sz_Text) {
        if (ch_Char < '0' || ch_Char > '9') {
            return std::nullopt;
        }
    }

    const int int_Port = std::stoi(sz_Text);
    if (int_Port > 65535) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(int_Port);
}

}  // namespace

int main(int argc, char* argv[]) {
    std::uint16_t u16_Port = SRV_DEFAULT_PORT;
    if (argc > 1) {
        const std::optional<std::uint16_t> opt_Port = srv_parsePort(argv[1]);
        if (!opt_Port.has_value()) {
            std::cerr << "usage: pumpkin-server [port]   (port 0-65535; 0 = any free port)\n";
            return 1;
        }
        u16_Port = opt_Port.value();
    }

    pumpkin::core::KvStore       obj_Store;
    pumpkin::services::TcpServer obj_Server(u16_Port, obj_Store);

    if (!obj_Server.start()) {
        return 1;
    }

    srv_atm_server.store(&obj_Server);
    std::signal(SIGINT, srv_handleStopSignal);
    std::signal(SIGTERM, srv_handleStopSignal);

    std::cout << "pumpkin-server listening on 127.0.0.1:" << obj_Server.port() << std::endl;
    const bool bol_StoppedCleanly = obj_Server.run();

    srv_atm_server.store(nullptr);  // Clear the global server pointer after stopping
    std::cout << "pumpkin-server: shutting down" << std::endl;

    return bol_StoppedCleanly ? 0 : 1;
}
