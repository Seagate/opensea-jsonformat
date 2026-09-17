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
// \file scan_json.c
// \brief This file defines types and functions related to the JSON-based output for Drive Scan.

#include "scan_json.h"
#include "io_utils.h"
#include "string_utils.h"

#define COMBINE_SCAN_JSON_VERSIONS_(x, y, z) #x "." #y "." #z
#define COMBINE_SCAN_JSON_VERSIONS(x, y, z)  COMBINE_SCAN_JSON_VERSIONS_(x, y, z)

#define SCAN_JSON_MAJOR_VERSION              1
#define SCAN_JSON_MINOR_VERSION              0
#define SCAN_JSON_PATCH_VERSION              0

#define SCAN_JSON_VERSION                                                                                              \
    COMBINE_SCAN_JSON_VERSIONS(SCAN_JSON_MAJOR_VERSION, SCAN_JSON_MINOR_VERSION, SCAN_JSON_PATCH_VERSION)

#define MAX_DRIVE_NODE_NAME_LENGTH      21
#define MAX_UINT32_TO_DEC_STRING_LENGHT 11

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

M_NODISCARD OPENSEA_JSONFORMAT_API eReturnValues create_JSON_Output_For_Scan(unsigned int          flags,
                                                                             eVerbosityLevels      scanVerbosity,
                                                                             const char* M_NONNULL utilityName,
                                                                             const char* M_NONNULL buildVersion,
                                                                             char**                jsonFormat)
{
    if (jsonFormat == M_NULLPTR)
    {
        return BAD_PARAMETER;
    }

    *jsonFormat = M_NULLPTR;

    json_object* rootNode = json_object_new_object();
    if (rootNode == M_NULLPTR)
    {
        return MEMORY_FAILURE;
    }

    if (create_Node_For_Utility_Version(rootNode, utilityName, buildVersion, "Drive Scan", SCAN_JSON_VERSION) !=
        SUCCESS)
    {
        json_object_put(rootNode);
        return MEMORY_FAILURE;
    }

    json_object* driveListNode = json_object_new_object();
    if (driveListNode == M_NULLPTR)
    {
        json_object_put(rootNode);
        return MEMORY_FAILURE;
    }
    if (add_JSON_Object(rootNode, "Drives Information", driveListNode) != 0)
    {
        json_object_put(rootNode);
        return MEMORY_FAILURE;
    }

    // get the device list
    uint32_t       deviceCount    = UINT32_C(0);
    scanDriveInfo* scanDeviceList = M_NULLPTR;
    eReturnValues  ret            = get_Devs_For_Scan_And_Print(flags, scanVerbosity, &deviceCount, &scanDeviceList);

    // add total drives number
    DECLARE_ZERO_INIT_ARRAY(char, totalDrivesValue, MAX_UINT32_TO_DEC_STRING_LENGHT);
    M_IGNORE_SAFE_INT_CALL(
        snprintf_err_handle(totalDrivesValue, MAX_UINT32_TO_DEC_STRING_LENGHT, "%" PRIu32 "", deviceCount),
        "maximum uint32_t decimal text fits in MAX_UINT32_TO_DEC_STRING_LENGHT");
    if (add_JSON_Object(driveListNode, "Total Drives", json_object_new_string(totalDrivesValue)) != 0)
    {
        safe_free(C_CAST(void**, &scanDeviceList));
        json_object_put(rootNode);
        return MEMORY_FAILURE;
    }

    if (ret == SUCCESS || ret == WARN_NOT_ALL_DEVICES_ENUMERATED)
    {
        if (deviceCount > 0)
        {
            if (scanDeviceList == M_NULLPTR)
            {
                json_object_put(rootNode);
                return MEMORY_FAILURE;
            }

            // create array node for drive list
            json_object* driveListArray = json_object_new_array();
            if (driveListArray == M_NULLPTR)
            {
                safe_free(C_CAST(void**, &scanDeviceList));
                json_object_put(rootNode);
                return MEMORY_FAILURE;
            }
            if (add_JSON_Object(driveListNode, "Drive List", driveListArray) != 0)
            {
                safe_free(C_CAST(void**, &scanDeviceList));
                json_object_put(rootNode);
                return MEMORY_FAILURE;
            }

            for (uint32_t devIter = UINT32_C(0); devIter < deviceCount; ++devIter)
            {
                json_object* driveNode = json_object_new_object();
                if (driveNode == M_NULLPTR)
                {
                    safe_free(C_CAST(void**, &scanDeviceList));
                    json_object_put(rootNode);
                    return MEMORY_FAILURE;
                }

                if (add_JSON_Object(driveNode, "Vendor", json_object_new_string(scanDeviceList[devIter].vendor)) != 0 ||
                    add_JSON_Object(driveNode, "Handle",
                                    json_object_new_string(scanDeviceList[devIter].displayHandle)) != 0 ||
                    add_JSON_Object(driveNode, "Model Number",
                                    json_object_new_string(scanDeviceList[devIter].modelNumber)) != 0 ||
                    add_JSON_Object(driveNode, "Serial Number",
                                    json_object_new_string(scanDeviceList[devIter].serialNumber)) != 0 ||
                    add_JSON_Object(driveNode, "FwRev",
                                    json_object_new_string(scanDeviceList[devIter].firmwareVersion)) != 0)
                {
                    json_object_put(driveNode);
                    safe_free(C_CAST(void**, &scanDeviceList));
                    json_object_put(rootNode);
                    return MEMORY_FAILURE;
                }

                // create new node, name it Drive ? and then add driveNode in this node
                json_object* node = json_object_new_object();
                if (node == M_NULLPTR)
                {
                    json_object_put(driveNode);
                    safe_free(C_CAST(void**, &scanDeviceList));
                    json_object_put(rootNode);
                    return MEMORY_FAILURE;
                }
                DECLARE_ZERO_INIT_ARRAY(char, driveNodeName, MAX_DRIVE_NODE_NAME_LENGTH);
                M_IGNORE_SAFE_INT_CALL(
                    snprintf_err_handle(driveNodeName, MAX_DRIVE_NODE_NAME_LENGTH, "Drive %" PRIu32, (devIter + 1)),
                    "maximum uint32_t drive number fits in MAX_DRIVE_NODE_NAME_LENGTH");
                if (add_JSON_Object(node, driveNodeName, driveNode) != 0)
                {
                    json_object_put(node);
                    safe_free(C_CAST(void**, &scanDeviceList));
                    json_object_put(rootNode);
                    return MEMORY_FAILURE;
                }

                // Add it into array
                if (add_JSON_Array_Element(driveListArray, node) != 0)
                {
                    safe_free(C_CAST(void**, &scanDeviceList));
                    json_object_put(rootNode);
                    return MEMORY_FAILURE;
                }
            }
        }
        else
        {
            if (add_JSON_Object(driveListNode, "Error", json_object_new_string("No devices found")) != 0)
            {
                safe_free(C_CAST(void**, &scanDeviceList));
                json_object_put(rootNode);
                return MEMORY_FAILURE;
            }
        }
    }
    else if (ret == PERMISSION_DENIED)
    {
        if (add_JSON_Object(driveListNode, "Error",
                            json_object_new_string("Permission to access all devices was denied")) != 0)
        {
            safe_free(C_CAST(void**, &scanDeviceList));
            json_object_put(rootNode);
            return MEMORY_FAILURE;
        }
    }
    else if (ret == DEVICE_BUSY)
    {
        if (add_JSON_Object(driveListNode, "Error",
                            json_object_new_string("All devices reported as busy at this time")) != 0)
        {
            safe_free(C_CAST(void**, &scanDeviceList));
            json_object_put(rootNode);
            return MEMORY_FAILURE;
        }
    }
    else
    {
        if (add_JSON_Object(driveListNode, "Error",
                            json_object_new_string("Unable to get number of devices from OS")) != 0)
        {
            safe_free(C_CAST(void**, &scanDeviceList));
            json_object_put(rootNode);
            return MEMORY_FAILURE;
        }
    }
    safe_free(C_CAST(void**, &scanDeviceList));

    // Convert JSON object to formatted string
    const char* jstr =
        json_object_to_json_string_ext(rootNode, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);

    // copy the json output into string
    if (asprintf(jsonFormat, "%s", jstr) < 0)
    {
        json_object_put(rootNode);
        return MEMORY_FAILURE;
    }

    // Free the JSON object
    json_object_put(rootNode);

    return SUCCESS;
}
