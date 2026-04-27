// SPDX-License-Identifier: MPL-2.0
//
// Do NOT modify or remove this copyright and license
//
// Copyright (c) 2012-2026 Seagate Technology LLC and/or its Affiliates, All Rights Reserved
//
// This software is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// ******************************************************************************************
//
// \file scan_json.h
// \brief This file defines types and functions related to the JSON-based output for Drive Scan.

#pragma once

#include "jsonformat_common.h"

#if defined(__cplusplus)
extern "C"
{
#endif

    M_PARAM_RO(3)
    M_PARAM_RO(4)
    M_PARAM_WO(5)
    OPENSEA_JSONFORMAT_API void create_JSON_Output_For_Scan(unsigned int          flags,
                                                            eVerbosityLevels      scanVerbosity,
                                                            const char* M_NONNULL utilityName,
                                                            const char* M_NONNULL buildVersion,
                                                            char* M_NONNULL* M_NULLABLE jsonFormat);

#if defined(__cplusplus)
}
#endif
