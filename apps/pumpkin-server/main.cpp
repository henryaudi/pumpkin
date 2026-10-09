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
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

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
 * @param sz_Text  The text to parse, e.g. argv[1].
 * @param u16_Port Receives the port on success; unchanged on failure.
 *
 * @return 0 on success; -EINVAL if the text is not only digits; -ERANGE if it is above 65535.
 */
int srv_parsePort(const std::string& sz_Text, std::uint16_t& u16_Port) {
    if (sz_Text.empty()) {
        return -EINVAL;  // Invalid argument
    }

    /* Digits only */
    for (const char ch_Char : sz_Text) {
        if (ch_Char < '0' || ch_Char > '9') {
            return -EINVAL;
        }
    }

    if (sz_Text.size() > 5) {  // Cannot accept more than 5 digits for a port number
        return -ERANGE;
    }

    const int int_Port = std::stoi(sz_Text);
    if (int_Port < 0 || int_Port > 65535) {
        return -ERANGE;
    }

    u16_Port = static_cast<std::uint16_t>(int_Port);
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::uint16_t u16_Port = SRV_DEFAULT_PORT;
    if (argc > 1) {
        const int int_Result = srv_parsePort(argv[1], u16_Port);
        if (int_Result < 0) {
            std::cerr << "pumpkin-server: invalid port '" << argv[1]
                      << "': " << std::strerror(-int_Result) << "\n"
                      << "usage: pumpkin-server [port]   (port 0-65535; 0 = any free port)\n";
            return EXIT_FAILURE;
        }
    }

    pumpkin::core::KvStore       obj_Store;
    pumpkin::services::TcpServer obj_Server(u16_Port, obj_Store);

    const int int_StartResult = obj_Server.start();
    if (int_StartResult < 0) {
        if (int_StartResult == -EADDRINUSE) {
            std::cerr << "pumpkin-server: port " << u16_Port
                      << " is in use; pick another port, or 0 for any free port\n";
        }
        return EXIT_FAILURE;
    }

    srv_atm_server.store(&obj_Server);
    std::signal(SIGINT, srv_handleStopSignal);
    std::signal(SIGTERM, srv_handleStopSignal);

    std::cout << "pumpkin-server listening on 127.0.0.1:" << obj_Server.port() << std::endl;
    const int int_RunResult = obj_Server.run();

    srv_atm_server.store(nullptr);  // Clear the global server pointer after stopping
    std::cout << "pumpkin-server: shutting down" << std::endl;

    return (int_RunResult == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
