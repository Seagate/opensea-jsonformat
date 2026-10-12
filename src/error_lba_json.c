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
#include "io_utils.h"
#include "logs.h"
#include "memory_safety.h"
#include "secure_file.h"
#include "string_utils.h"

#define COMBINE_ERROR_LBA_JSON_VERSIONS_(x, y, z) #x "." #y "." #z
#define COMBINE_ERROR_LBA_JSON_VERSIONS(x, y, z)  COMBINE_ERROR_LBA_JSON_VERSIONS_(x, y, z)

#define ERROR_LBA_JSON_MAJOR_VERSION              1
#define ERROR_LBA_JSON_MINOR_VERSION              0
#define ERROR_LBA_JSON_PATCH_VERSION              0

#define ERROR_LBA_JSON_VERSION                                                                                         \
    COMBINE_ERROR_LBA_JSON_VERSIONS(ERROR_LBA_JSON_MAJOR_VERSION, ERROR_LBA_JSON_MINOR_VERSION,                        \
                                    ERROR_LBA_JSON_PATCH_VERSION)
#define MAX_TIME_UNIT_STRING_LENGHT 10

M_NODISCARD static eReturnValues add_JSON_Object(json_object* parent, const char* key, json_object* child)
{
    if (child == M_NULLPTR)
    {
        return MEMORY_FAILURE;
    }
    if (json_object_object_add(parent, key, child) != 0)
    {
        json_object_put(child);
        return MEMORY_FAILURE;
    }
    return SUCCESS;
}

M_NODISCARD static eReturnValues add_JSON_Array_Element(json_object* parent, json_object* child)
{
    if (child == M_NULLPTR)
    {
        return MEMORY_FAILURE;
    }
    if (json_object_array_add(parent, child) != 0)
    {
        json_object_put(child);
        return MEMORY_FAILURE;
    }
    return SUCCESS;
}

M_NODISCARD eReturnValues create_JSON_LBA_Error_List(constPtrErrorLBA LBAs,
                                                     uint16_t         numberOfErrors,
                                                     json_object*     jObject)
{
    if (LBAs == M_NULLPTR || numberOfErrors == 0 || jObject == M_NULLPTR)
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
        eRepairStatus status       = LBAs[errorIter - 1].repairStatus;
        const char*   repairString = get_Repair_Status_String(status);

        // Check if access denied
        if (status == UNABLE_TO_REPAIR_ACCESS_DENIED)
        {
            showAccessDeniedNote = true;
        }

        // Add defect number, LBA, and repair status to JSON object
        if (add_JSON_Object(jErrorEntry, "Defect Number", json_object_new_uint64(errorIter)) != 0 ||
            add_JSON_Object(jErrorEntry, "Defect LBA", json_object_new_uint64(LBAs[errorIter - 1].errorAddress)) != 0 ||
            add_JSON_Object(jErrorEntry, "Repair Status", json_object_new_string(repairString)) != 0)
        {
            json_object_put(jErrorEntry);
            json_object_put(jBadLBAsArray);
            return MEMORY_FAILURE;
        }

        // Add error entry to array
        if (add_JSON_Array_Element(jBadLBAsArray, jErrorEntry) != 0)
        {
            json_object_put(jBadLBAsArray);
            return MEMORY_FAILURE;
        }
    }

    // Add the bad LBAs array to the main JSON object
    if (add_JSON_Object(jObject, "BAD LBAs", jBadLBAsArray) != 0)
    {
        return MEMORY_FAILURE;
    }

    // Add access denied note if needed
    if (showAccessDeniedNote)
    {
        json_object* jNote = json_object_new_object();
        if (jNote == M_NULLPTR)
        {
            return MEMORY_FAILURE;
        }
        if (add_JSON_Object(jNote, "Title", json_object_new_string("Access Denied")) != 0 ||
            add_JSON_Object(jNote, "Message",
                            json_object_new_string("Some LBAs could not be repaired because access to them was "
                                                   "denied. This may happen when a secondary drive with a file "
                                                   "system installed on it is recognized by the current host OS, "
                                                   "but the current host doesn't have permission to change the "
                                                   "contents of the second drive.")) != 0)
        {
            json_object_put(jNote);
            return MEMORY_FAILURE;
        }
        if (add_JSON_Object(jObject, "Access Denied Note", jNote) != 0)
        {
            return MEMORY_FAILURE;
        }
    }

    return SUCCESS;
}

M_NODISCARD eReturnValues create_JSON_Output_For_Error_LBA(const tDevice*   device,
                                                           constPtrErrorLBA LBAs,
                                                           uint16_t         numberOfErrors,
                                                           char**           jsonFormat,
                                                           const char*      utilityName,
                                                           const char*      buildVersion)
{
    eReturnValues ret = SUCCESS;

    if (device == M_NULLPTR || LBAs == M_NULLPTR || numberOfErrors == 0 || jsonFormat == M_NULLPTR)
    {
        return BAD_PARAMETER;
    }

    *jsonFormat = M_NULLPTR;

    // Create a new JSON object
    json_object* rootObj = json_object_new_object();

    if (rootObj == M_NULLPTR)
        return MEMORY_FAILURE;

    if (create_Node_For_Utility_Version(rootObj, utilityName, buildVersion, "LBA ERROR LIST", ERROR_LBA_JSON_VERSION) !=
            SUCCESS ||
        create_Node_For_Drive_Information(rootObj, device) != SUCCESS)
    {
        json_object_put(rootObj);
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
    // copy the json output into string
    if (asprintf(jsonFormat, "%s", jstr) < 0)
    {
        json_object_put(rootObj);
        return MEMORY_FAILURE;
    }

    // Free the JSON object
    json_object_put(rootObj);

    return ret;
}
