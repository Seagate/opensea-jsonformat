//
// cdl_config_file.h
//
// Do NOT modify or remove this copyright and confidentiality notice.
//
// Copyright (c) 2012-2024 Seagate Technology LLC and/or its Affiliates, All Rights Reserved
//
// The code contained herein is CONFIDENTIAL to Seagate Technology LLC
// and may be covered under one or more Non-Disclosure Agreements.
// All or portions are also trade secret.
// Any use, modification, duplication, derivation, distribution or disclosure
// of this code, for any reason, not expressly authorized is prohibited.
// All other rights are expressly reserved by Seagate Technology LLC.
//
// *****************************************************************************

// \file device_statistics_json.h
// \brief This file defines types and functions related to the JSON-based Device Statistics log file process.

#pragma once

#include "common_types.h"
#include "common_public.h"
#include "device_statistics.h"
#include "seagate_operations.h"
#include "jsonformat_common.h"

#if defined (__cplusplus)
extern "C"
{
#endif

    OPENSEA_JSONFORMAT_API eReturnValues create_JSON_File_For_Device_Statistics(tDevice *device, ptrDeviceStatistics deviceStatictics, ptrSeagateDeviceStatistics seagateDeviceStatistics, bool seagateDeviceStatisticsAvailable, char** jsonFormat);

#if defined (__cplusplus)
}
#endif