/**
 ******************************************************************************
 * @file    CMS_Manager.cpp
 * @author  Shangjie Zheng
 * @brief   Config Management Service (CMS): implementation.
 *          This file provides functions to:
 *           + Describe every command-line option in one table
 *           + Convert option values from text and pass them to module setters
 *           + Print the usage, generated from the same table
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

#include "services/pump-srv-cms/CMS_Manager.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "common/pump-cmn-log/LOG_Logger.hpp"
#include "common/pump-cmn-stat/STAT_Status.hpp"
#include "services/pump-srv-net/NET_ServerConfig.hpp"

LOG_MODULE_REGISTER(CMS, LOG_LEVEL_DBG)

namespace pumpkin::services {

namespace {
// ================================================================================================
// PRIVATE SCOPE
// ================================================================================================
constexpr size_t CMS_MAX_PORT_DIGITS = 5;  // Maximum number of digits in a port number (0-65535)
constexpr int    CMS_MAX_PORT_VALUE  = 65535;  // Maximum valid port number

struct CmsOption {
    const char* sz_Flag;
    const char* sz_ValueHint;
    const char* sz_Help;

    /**
     * @brief Apply the option value to the corresponding module setting.
     *
     * @param sz_Value The value to be applied to the module setting.
     *
     * @return P_OK on success, or -P_E* code
     */
    int (*fnc_Apply)(const char* sz_Value);

    /**
     * @brief Validate the option value before applying it.
     *
     * @param sz_Value The value to be validated.
     *
     * @return P_OK if the value is valid, or -P_E* code if invalid.
     */
    int (*fnc_Validate)(const char* sz_Value);
};

int cms_parsePort(const char* sz_Text, uint16_t* ptr_Port) {
    if (sz_Text == nullptr || sz_Text[0] == '\0') {
        return -P_EINVARG;
    }

    /* Extract the port number from the text */
    int    int_Port      = 0;
    size_t siz_NumDigits = 0;
    for (const char* ptr_Char = sz_Text; *ptr_Char != '\0'; ptr_Char++) {
        if (*ptr_Char < '0' || *ptr_Char > '9') {
            return -P_EINVARG;
        }

        /* Check if the number of digits exceeds the maximum allowed */
        if (++siz_NumDigits > CMS_MAX_PORT_DIGITS) {
            return -P_ERANGE;
        }

        int_Port = int_Port * 10 + (*ptr_Char - '0');
    }

    /* ... and validate its range */
    if (int_Port > CMS_MAX_PORT_VALUE) {
        return -P_ERANGE;
    }

    *ptr_Port = static_cast<uint16_t>(int_Port);
    return P_OK;
}

int cms_applyPort(const char* sz_Val) {
    uint16_t  u16_Port = 0;
    const int int_Ret  = cms_parsePort(sz_Val, &u16_Port);
    return (int_Ret < 0) ? int_Ret : NET_SetPort(u16_Port);
}

int cms_applyBindIp(const char* sz_Val) {
    return NET_SetBindIp(sz_Val);  // NET validates IP address internally
}

int cms_applyLogLevel(const char* sz_Val) {
    return LOG_Init(sz_Val);  // LOG validates log level internally
}

int cms_validatePort(const char* sz_Val) {
    uint16_t u16_Unused = 0;
    return cms_parsePort(sz_Val, &u16_Unused);
}

int cms_validateBindIp(const char* sz_Val) {
    in_addr str_Address{};
    return (inet_pton(AF_INET, sz_Val, &str_Address) == 1) ? P_OK : -P_EINVARG;
}

int cms_validateLogLevel(const char* sz_Val) {
    /* Same names LOG_Init() accepts. TODO: ask LOG instead, once it has a validator */
    constexpr const char* CMS_LOG_LEVELS[] = {"none", "critical", "error", "warn",
                                              "info", "debug",    "trace"};
    for (const char* sz_Level : CMS_LOG_LEVELS) {
        if (std::strcmp(sz_Val, sz_Level) == 0) {
            return P_OK;
        }
    }
    return -P_EINVARG;
}

/**
 * @brief Array of all command-line options the program accepts.
 */
constexpr CmsOption CMS_OPTIONS[] = {
    /* bind IP address */
    {"--bind", "<ipv4>", "address to listen on (default 127.0.0.1; 0.0.0.0 = all interfaces)",
     cms_applyBindIp},

    /* port number */
    {"--port", "<0-65535>", "port to listen on (default 7379; 0 = any free port)", cms_applyPort},

    /* log level */
    {"--loglevel", "<level>", "none, critical, error, warn, info, debug, trace (default info)",
     cms_applyLogLevel},
};

/**
 * @brief Find a CmsOption by its command-line flag.
 *
 * @param sz_Flag The command-line flag to search for.
 *
 * @return Pointer to the corresponding CmsOption if found, or nullptr if not found.
 */
const CmsOption* cms_findOption(const char* sz_Flag) {
    for (const CmsOption& str_Opt : CMS_OPTIONS) {
        if (std::strcmp(str_Opt.sz_Flag, sz_Flag) == 0) {
            return &str_Opt;
        }
    }
    return nullptr;
}

/**
 * @brief Check if a string consists entirely of digits. e.g. a port given without --port.
 *
 * @param sz_Text The string to check.
 *
 * @return true if the string contains only digits, false otherwise.
 */
bool cms_isAllDigits(const char* sz_Text) {
    if (sz_Text[0] == '\0') {
        return false;
    }

    for (const char* ptr_Char = sz_Text; *ptr_Char != '\0'; ptr_Char++) {
        if (*ptr_Char < '0' || *ptr_Char > '9') {
            return false;
        }
    }

    return true;
}

/**
 * @brief Prints the usage to stderr, generated from CMS_OPTIONS.
 */
void cms_printUsage(const char* sz_Program) {
    std::fprintf(stderr, "usage: %s [options]\n", sz_Program);
    for (const CmsOption& str_Option : CMS_OPTIONS) {
        const std::string sz_Left = std::string(str_Option.sz_Flag) + " " + str_Option.sz_ValueHint;
        std::fprintf(stderr, "  %-22s %s\n", sz_Left.c_str(), str_Option.sz_Help);
    }
}
}  // namespace

// ================================================================================================
// GLOBAL SCOPE
// ================================================================================================
int CMS_Init(int argc, char* argv[]) {
    if (argc > 0 && argv == nullptr) {
        return -P_EINVARG;
    }
    const char* sz_Program = (argc > 0) ? argv[0] : "pumpkin-server";

    /* Check every flag and value. Nothing is changed here, so a bad argument will be caught without
     * affecting program state */
    int int_Idx = 1;
    while (int_Idx < argc) {
        const char*      sz_Flag    = argv[int_Idx];
        const CmsOption* ptr_Option = cms_findOption(sz_Flag);

        if (ptr_Option == nullptr) {
            if (cms_isAllDigits(sz_Flag)) {
                LOG_ERR("unexpected argument '%s'; did you mean --port %s?", sz_Flag, sz_Flag);
            } else {
                LOG_ERR("unknown argument '%s'", sz_Flag);
            }
            cms_printUsage(sz_Program);
            return -P_EINVARG;
        }

        if (int_Idx + 1 >= argc) {  // The last element was option flag, missing its value
            LOG_ERR("%s needs a value %s", ptr_Option->sz_Flag, ptr_Option->sz_ValueHint);
            cms_printUsage(sz_Program);
            return -P_EINVARG;
        }

        /* The value is missing, or if the next word is another option flag */
        if (int_Idx + 1 >= argc || cms_findOption(argv[int_Idx + 1]) != nullptr) {
            LOG_ERR("%s needs a value %s", ptr_Option->sz_Flag, ptr_Option->sz_ValueHint);
            cms_printUsage(sz_Program);
            return -P_EINVARG;
        }

        /* Extract the option value */
        const char* sz_Value = argv[int_Idx + 1];
        const int   int_Ret  = ptr_Option->fnc_Validate(sz_Value);
        if (int_Ret < 0) {
            LOG_ERR("invalid value '%s' for %s: %s", sz_Value, ptr_Option->sz_Flag,
                    STAT_ToString(-int_Ret));
            cms_printUsage(sz_Program);
            return int_Ret;
        }

        int_Idx += 2;  // Move past the option flag and its value
    }

    /* Everything is valid, apply it. */
    int_Idx = 1;
    while (int_Idx < argc) {
        const CmsOption* ptr_Option = cms_findOption(argv[int_Idx]);
        const char*      sz_Value   = argv[int_Idx + 1];

        const int int_Ret = ptr_Option->fnc_Apply(sz_Value);
        if (int_Ret < 0) {
            LOG_ERR("could not apply %s %s: %s", ptr_Option->sz_Flag, sz_Value,
                    STAT_ToString(-int_Ret));
            return int_Ret;
        }
        LOG_DBG("%s = %s", ptr_Option->sz_Flag, sz_Value);

        int_Idx += 2;  // Move past the option flag and its value
    }

    return P_OK;
}

}  // namespace pumpkin::services
