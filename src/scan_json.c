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

void create_JSON_Output_For_Scan(unsigned int     flags,
                                 eVerbosityLevels scanVerbosity,
                                 const char*      utilityName,
                                 const char*      buildVersion,
                                 char**           jsonFormat)
{
    json_object* rootNode = json_object_new_object();

    create_Node_For_Utility_Version(rootNode, utilityName, buildVersion, "Drive Scan", SCAN_JSON_VERSION);

    json_object* driveListNode = json_object_new_object();

    // get the device list
    uint32_t       deviceCount    = UINT32_C(0);
    scanDriveInfo* scanDeviceList = M_NULLPTR;
    eReturnValues  ret            = get_Devs_For_Scan_And_Print(flags, scanVerbosity, &deviceCount, &scanDeviceList);

    // add total drives number
    DECLARE_ZERO_INIT_ARRAY(char, totalDrivesValue, MAX_UINT32_TO_DEC_STRING_LENGHT);
    snprintf_err_handle(totalDrivesValue, MAX_UINT32_TO_DEC_STRING_LENGHT, "%" PRIu32 "", deviceCount);
    json_object_object_add(driveListNode, "Total Drives", json_object_new_string(totalDrivesValue));

    if (ret == SUCCESS || ret == WARN_NOT_ALL_DEVICES_ENUMERATED)
    {
        if (deviceCount > 0)
        {
            // create array node for drive list
            json_object* driveListArray = json_object_new_array();

            for (uint32_t devIter = UINT32_C(0); devIter < deviceCount; ++devIter)
            {
                json_object* driveNode = json_object_new_object();

                json_object_object_add(driveNode, "Vendor", json_object_new_string(scanDeviceList[devIter].vendor));
                json_object_object_add(driveNode, "Handle",
                                       json_object_new_string(scanDeviceList[devIter].displayHandle));
                json_object_object_add(driveNode, "Model Number",
                                       json_object_new_string(scanDeviceList[devIter].modelNumber));
                json_object_object_add(driveNode, "Serial Number",
                                       json_object_new_string(scanDeviceList[devIter].serialNumber));
                json_object_object_add(driveNode, "FwRev",
                                       json_object_new_string(scanDeviceList[devIter].firmwareVersion));

                // create new node, name it Drive ? and then add driveNode in this node
                json_object* node = json_object_new_object();
                DECLARE_ZERO_INIT_ARRAY(char, driveNodeName, MAX_DRIVE_NODE_NAME_LENGTH);
                snprintf_err_handle(driveNodeName, MAX_DRIVE_NODE_NAME_LENGTH, "Drive %" PRIu32, (devIter + 1));
                json_object_object_add(node, driveNodeName, driveNode);

                // Add it into array
                json_object_array_add(driveListArray, node);
            }

            json_object_object_add(driveListNode, "Drive List", driveListArray);
        }
        else
        {
            json_object_object_add(driveListNode, "Error", json_object_new_string("No devices found"));
        }
    }
    else if (ret == PERMISSION_DENIED)
    {
        json_object_object_add(driveListNode, "Error",
                               json_object_new_string("Permission to access all devices was denied"));
    }
    else if (ret == DEVICE_BUSY)
    {
        json_object_object_add(driveListNode, "Error",
                               json_object_new_string("All devices reported as busy at this time"));
    }
    else
    {
        json_object_object_add(driveListNode, "Error",
                               json_object_new_string("Unable to get number of devices from OS"));
    }
    json_object_object_add(rootNode, "Drives Information", driveListNode);
    safe_free(C_CAST(void**, &scanDeviceList));

    // Convert JSON object to formatted string
    const char* jstr =
        json_object_to_json_string_ext(rootNode, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);

    // copy the json output into string
    if (asprintf(jsonFormat, "%s", jstr) < 0)
    {
        return;
    }

    // Free the JSON object
    json_object_put(rootNode);
}