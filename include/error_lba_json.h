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
// \file error_lba_json.h
// \brief This file defines types and functions related to the JSON-based output for Error LBA.

#pragma once

#include "sector_repair.h"
#include "jsonformat_common.h"

#if defined(__cplusplus)
extern "C"
{
#endif

    M_PARAM_RO(1)
    M_PARAM_RO(3)
    OPENSEA_JSONFORMAT_API eReturnValues create_JSON_LBA_Error_List(constPtrErrorLBA M_NONNULL LBAs,
                                                                      uint16_t numberOfErrors,
                                                                      json_object* M_NONNULL jObject);

    M_PARAM_RO(1)
    M_PARAM_RO(2)
    M_PARAM_RO(4)
    M_PARAM_WO(5)
    M_PARAM_RO(6)
    M_PARAM_RO(7)
    OPENSEA_JSONFORMAT_API eReturnValues create_JSON_Output_For_Error_LBA(const tDevice*  M_NONNULL  device,
                                                                          constPtrErrorLBA M_NONNULL LBAs,
                                                                          uint16_t                   numberOfErrors,
                                                                          char*M_NONNULL* M_NULLABLE jsonFormat,
                                                                          const char*    M_NONNULL   utilityName,
                                                                          const char*    M_NONNULL   buildVersion);

#if defined(__cplusplus)
}
#endif
