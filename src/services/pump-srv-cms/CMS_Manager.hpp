/**
 ******************************************************************************
 * @file    CMS_Manager.hpp
 * @author  Shangjie Zheng
 * @brief   Config Management Service (CMS): public interface.
 *          This file declares:
 *           + CMS_Init: reads the command line once and hands each setting to the
 *             module that owns it, through that module's setters
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

#ifndef CMS_MANAGER_HPP_
#define CMS_MANAGER_HPP_

namespace pumpkin::services {

    /**
     * @brief Applies the command line to every module
     *
     * @details Each option takes one value. Settings are applied as they are read; on any error the
     *          reason and the usage are printed and the caller is expected to exit. Unknown options
     *          and bare arguments are errors.
     *
     * @param argc The number of command line arguments
     * @param argv The array of command line arguments
     *
     * @return int Returns P_OK on success, -P_E* on failure
     */
    int CMS_Init(int argc, char* argv[]);
}

#endif  // CMS_MANAGER_HPP_
