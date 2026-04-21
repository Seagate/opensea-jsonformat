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
// \file error_lba_json.c
// \brief This file defines types and functions related to the JSON-based output for Error LBA.

#include "error_lba_json.h"
#include "logs.h"
#include "memory_safety.h"
#include "secure_file.h"
#include "string_utils.h"

#define MAX_TIME_UNIT_STRING_LENGHT 10

static const char* get_Repair_Status_String(eRepairStatus status)
{
    const char* statusString = "Not Repaired";
    switch (status)
    {
    case REPAIRED:
        statusString = "Repaired";
        break;
    case REPAIR_FAILED:
        statusString = "Repair Failed";
        break;
    case REPAIR_NOT_REQUIRED:
        statusString = "Repair Not Required";
        break;
    case UNABLE_TO_REPAIR_ACCESS_DENIED:
        statusString = "Access Denied";
        break;
    case NOT_REPAIRED:
    default:
        statusString = "Not Repaired";
        break;
    }
    return statusString;
}

eReturnValues create_JSON_LBA_Error_List(constPtrErrorLBA LBAs, uint16_t numberOfErrors,
                                         json_object* jObject)
{
    eReturnValues ret = SUCCESS;

    if (LBAs == M_NULLPTR || numberOfErrors == 0)
    {
        return BAD_PARAMETER;
    }

    // Create JSON array for bad LBAs
    json_object* jBadLBAsArray = json_object_new_array();
    if (jBadLBAsArray == M_NULLPTR)
    {
        print_str("Error in creating JSON array for bad LBAs!\n");
        return MEMORY_FAILURE;
    }

    bool showAccessDeniedNote = false;

    // Iterate through each error and add to JSON array
    for (uint64_t errorIter = 1; errorIter <= numberOfErrors; errorIter++)
    {
        json_object* jErrorEntry = json_object_new_object();
        if (jErrorEntry == M_NULLPTR)
        {
            json_object_put(jBadLBAsArray);
            print_str("Error in creating JSON object for error entry!\n");
            return MEMORY_FAILURE;
        }

        // Get repair status string
        eRepairStatus status = LBAs[errorIter - 1].repairStatus;
        const char* repairString = get_Repair_Status_String(status);

        // Check if access denied
        if (status == UNABLE_TO_REPAIR_ACCESS_DENIED)
        {
            showAccessDeniedNote = true;
        }

        // Add defect number, LBA, and repair status to JSON object
        json_object_object_add(jErrorEntry, "Defect Number", json_object_new_uint64(errorIter));
        json_object_object_add(jErrorEntry, "Defect LBA", json_object_new_uint64(LBAs[errorIter - 1].errorAddress));
        json_object_object_add(jErrorEntry, "Repair Status", json_object_new_string(repairString));

        // Add error entry to array
        json_object_array_add(jBadLBAsArray, jErrorEntry);
    }

    // Add the bad LBAs array to the main JSON object
    json_object_object_add(jObject, "BAD LBAs", jBadLBAsArray);

    // Add access denied note if needed
    if (showAccessDeniedNote)
    {
        json_object* jNote = json_object_new_object();
        if (jNote != M_NULLPTR)
        {
            json_object_object_add(jNote, "Title", json_object_new_string("Access Denied"));
            json_object_object_add(jNote, "Message",
                                   json_object_new_string("Some LBAs could not be repaired because access to them was "
                                                          "denied. This may happen when a secondary drive with a file "
                                                          "system installed on it is recognized by the current host OS, "
                                                          "but the current host doesn't have permission to change the "
                                                          "contents of the second drive."));
            json_object_object_add(jObject, "Access Denied Note", jNote);
        }
    }

    return ret;
}

eReturnValues create_JSON_File_For_Error_LBA(constPtrErrorLBA LBAs, uint16_t numberOfErrors, const char* logPath)
{
    eReturnValues ret = SUCCESS;

    if (LBAs == M_NULLPTR || numberOfErrors == 0 || logPath == M_NULLPTR)
    {
        return BAD_PARAMETER;
    }

    // Create a new JSON object
    json_object* rootObj = json_object_new_object();
    if (rootObj == M_NULLPTR)
    {
        print_str("Error in creating JSON object!\n");
        return MEMORY_FAILURE;
    }

    // Create the bad LBAs list in JSON format
    ret = create_JSON_LBA_Error_List(LBAs, numberOfErrors, rootObj);
    if (ret != SUCCESS)
    {
        json_object_put(rootObj);
        return ret;
    }

    // Convert JSON object to formatted string
    const char* jstr = json_object_to_json_string_ext(rootObj, JSON_C_TO_STRING_PRETTY);

    // Write formatted JSON string to a file
    ret = write_String_To_File(logPath, jstr);
    if (ret == SUCCESS)
    {
        printf("Successfully created JSON file for Error LBA list at %s.\n", logPath);
    }
    else
    {
        print_str("Error in writing the JSON string to the file!\n");
    }

    // Free the JSON object
    json_object_put(rootObj);

    return ret;
}
