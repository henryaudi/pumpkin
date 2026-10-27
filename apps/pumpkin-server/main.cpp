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
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0. See the LICENSE file in the
 * root directory of this repository for the full license text.
 ******************************************************************************
 */

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "common/pump-cmn-log/LOG_Logger.hpp"
#include "common/pump-cmn-stat/STAT_Status.hpp"
#include "core/pump-core-eng/ENG_KvStore.hpp"
#include "services/pump-srv-cms/CMS_Manager.hpp"
#include "services/pump-srv-net/NET_TcpServer.hpp"

LOG_MODULE_REGISTER(SRV, LOG_LEVEL_INF)

namespace {

/**
 * @brief Called by the OS on Ctrl+C (SIGINT) or `kill` (SIGTERM): asks the server to stop.
 *
 * @details A signal handler may only do async-signal-safe work; NET_StopServer() only sets an
 *          atomic flag, and NET_RunServer() returns within NET_POLL_TIMEOUT_MS.
 *
 * @param int_Signal The signal number (unused).
 */
void srv_handleStopSignal([[maybe_unused]] int int_Signal) {
    pumpkin::services::NET_StopServer();
}
}  // namespace

int main(int argc, char* argv[]) {
    pumpkin::core::KvStore obj_Store;

    LOG_Init("info");
    if (pumpkin::services::CMS_Init(argc, argv) < 0) {
        return EXIT_FAILURE;
    }
    if (pumpkin::services::NET_Init(&obj_Store) < 0) {
        return EXIT_FAILURE;
    }

    std::signal(SIGINT, srv_handleStopSignal);   // Register signal handler for Ctrl+C
    std::signal(SIGTERM, srv_handleStopSignal);  // Register signal handler for `kill` command

    const int int_Ret = pumpkin::services::NET_RunServer();

    /* Run server out of scope */
    LOG_INF("shutting down");
    pumpkin::services::NET_DeinitServer();

    return (int_Ret == P_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}
