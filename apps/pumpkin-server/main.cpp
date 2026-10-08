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

#include <cstdint>
#include <cstdlib>
#include <iostream>

#include "core/pump-core-eng/ENG_KvStore.hpp"
#include "services/pump-srv-net/NET_TcpServer.hpp"

namespace {

/* Port used when none was given on the command line */
constexpr std::uint16_t SRV_DEFAULT_PORT = 7379;

}  // namespace

int main(int argc, char* argv[]) {
    std::uint16_t u16_Port = SRV_DEFAULT_PORT;
    if (argc > 1) {
        const int int_Requested = std::atoi(argv[1]);
        if (int_Requested <= 0 || int_Requested > 65535) {
            std::cerr << "usage: pumpkin-server [port]\n";
            return 1;
        }
        u16_Port = static_cast<std::uint16_t>(int_Requested);
    }

    pumpkin::core::KvStore       obj_Store;
    pumpkin::services::TcpServer obj_Server(u16_Port, obj_Store);

    if (!obj_Server.start()) {
        return 1;
    }

    std::cout << "pumpkin-server listening on 127.0.0.1:" << obj_Server.port() << std::endl;
    obj_Server.run();
    return 0;
}
