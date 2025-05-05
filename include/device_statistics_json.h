// SPDX-License-Identifier: MPL-2.0
//
// Do NOT modify or remove this copyright and license
//
// Copyright (c) 2012-2025 Seagate Technology LLC and/or its Affiliates, All Rights Reserved
//
// This software is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// ******************************************************************************************
//
// \file device_statistics_json.h
// \brief This file defines types and functions related to the JSON-based outptu for Device Statistics log.

#pragma once

#include "common_public.h"
#include "common_types.h"
#include "device_statistics.h"
#include "jsonformat_common.h"
#include "seagate_operations.h"

#if defined(__cplusplus)
extern "C"
{
#endif

    M_NONNULL_PARAM_LIST(1, 2)
    M_PARAM_RO(1)
    M_PARAM_RO(2)
    M_PARAM_RO(3)
    M_PARAM_WO(5)
    OPENSEA_JSONFORMAT_API eReturnValues
    create_JSON_Output_For_Device_Statistics(tDevice*                   device,
                                             ptrDeviceStatistics        deviceStatictics,
                                             ptrSeagateDeviceStatistics seagateDeviceStatistics,
                                             bool                       seagateDeviceStatisticsAvailable,
                                             char**                     jsonFormat);

#if defined(__cplusplus)
}
#endif