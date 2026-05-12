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
// \file jsonformat_common.c
// \brief This file defines types and functions related to the JSON-based output.

#include "jsonformat_common.h"
#include "io_utils.h"
#include "memory_safety.h"
#include "string_utils.h"
#include "time_utils.h"

#define MAX_LOG_NAME_STRING_LENGHT   31
#define MAX_BUILD_DATE_STRING_LENGHT 12

M_PARAM_WO(1)
M_PARAM_RO(2)
M_PARAM_RO(3)
M_PARAM_RO(4)
M_PARAM_RO(5)
OPENSEA_JSONFORMAT_API void create_Node_For_Utility_Version(json_object* M_NONNULL rootObject,
                                                            const char* M_NONNULL  utilityName,
                                                            const char* M_NONNULL  buildVersion,
                                                            const char* M_NONNULL  logName,
                                                            const char* M_NONNULL  jsonVersion)
{
    json_object* jsonNode = json_object_new_object();

    json_object_object_add(jsonNode, "Utility Name", json_object_new_string(utilityName));
    json_object_object_add(jsonNode, "Build Version", json_object_new_string(buildVersion));
    DECLARE_ZERO_INIT_ARRAY(char, buildDateString, MAX_BUILD_DATE_STRING_LENGHT);
#if defined(BUILD_TIMESTAMP)
    // Use the date passed from the Makefile/Meson (Reproducible)
    if (0 != safe_strcpy(buildDateString, MAX_BUILD_DATE_STRING_LENGHT, BUILD_TIMESTAMP))
    {
        perror("Error copying build date for output. Likely truncation");
    }
#else
    // Fallback to the compiler's current date (Not reproducible)
    if (0 != safe_strcpy(buildDateString, MAX_BUILD_DATE_STRING_LENGHT, __DATE__))
    {
        perror("Error copying build date for output. Likely truncation");
    }
#endif
    json_object_object_add(jsonNode, "Build Date", json_object_new_string(buildDateString));

    struct tm logTime;
    M_INITIALIZE_STRUCTURE(&logTime, sizeof(struct tm));
    time_t currentTime = time(NULL);
    DECLARE_ZERO_INIT_ARRAY(char, currentTimeString, CURRENT_TIME_STRING_LENGTH);
    strftime(currentTimeString, CURRENT_TIME_STRING_LENGTH, "%Y%m%dT%H%M%S", safe_localtime(&currentTime, &logTime));
    json_object_object_add(jsonNode, "Run as Date", json_object_new_string(currentTimeString));

    DECLARE_ZERO_INIT_ARRAY(char, logNameString, MAX_LOG_NAME_STRING_LENGHT);
    if (0 != safe_strcpy(logNameString, MAX_LOG_NAME_STRING_LENGHT, logName))
    {
        perror("Error copying log name for output. Likely truncation");
    }
    json_object_object_add(jsonNode, logNameString, json_object_new_string(jsonVersion));

    json_object_object_add(rootObject, "Utility Information", jsonNode);
}

M_PARAM_WO(1)
M_PARAM_RO(2)
OPENSEA_JSONFORMAT_API void create_Node_For_Drive_Information(json_object* M_NONNULL   rootObject,
                                                              const tDevice* M_NONNULL device)
{
    json_object* jsonNode = json_object_new_object();

    json_object_object_add(jsonNode, "Model Name", json_object_new_string(device->drive_info.product_identification));
    json_object_object_add(jsonNode, "Serial Number", json_object_new_string(device->drive_info.serialNumber));
    json_object_object_add(jsonNode, "Firmware Version", json_object_new_string(device->drive_info.product_revision));

    json_object_object_add(rootObject, "Drive Information", jsonNode);
}
