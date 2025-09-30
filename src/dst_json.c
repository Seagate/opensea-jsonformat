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
// \file smart_attribute_json.c
// \brief This file defines types and functions related to the JSON-based output for SMART Attributes.

#include "dst_json.h"
#include "io_utils.h"
#include "string_utils.h"

#define COMBINE_DST_JSON_VERSIONS_(x, y, z) #x "." #y "." #z
#define COMBINE_DST_JSON_VERSIONS(x, y, z)  COMBINE_DST_JSON_VERSIONS_(x, y, z)

#define DST_JSON_MAJOR_VERSION              1
#define DST_JSON_MINOR_VERSION              0
#define DST_JSON_PATCH_VERSION              0

#define DST_JSON_VERSION                                                                                               \
    COMBINE_DST_JSON_VERSIONS(DST_JSON_MAJOR_VERSION, DST_JSON_MINOR_VERSION, DST_JSON_PATCH_VERSION)

#define MAX_TEST_NAME_LENGTH                   25
#define MAX_UINT8_TO_DEC_STRING_LENGHT         4
#define MAX_UINT64_TO_DEC_STRING_LENGHT        21
#define MAX_DST_EXECUTION_STATUS_STRING_LENGTH 31
#define MAX_SENSE_INFO_STRING_LENGTH           21
#define MAX_UINT8_TO_HEX_STRING_LENGHT         3

static void get_Test_Name(eDriveType driveType, uint8_t testId, char** testName)
{
    safe_memset(*testName, MAX_TEST_NAME_LENGTH, 0, MAX_TEST_NAME_LENGTH);
    if (driveType == ATA_DRIVE)
    {
        switch (testId)
        {
        case 0:
            snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Offline Data Collect");
            break;
        case 1: // short
            snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Short (offline)");
            break;
        case 2: // extended
            snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Extended (offline)");
            break;
        case 3: // conveyance
            snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Conveyance (offline)");
            break;
        case 4: // selective
            snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Selective (offline)");
            break;
        case 0x81: // short
            snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Short (captive)");
            break;
        case 0x82: // extended
            snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Extended (captive)");
            break;
        case 0x83: // conveyance
            snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Conveyance (captive)");
            break;
        case 0x84: // selective
            snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Selective (captive)");
            break;
        default:
            if ((testId >= 0x40 && testId <= 0x7E) || (testId >= 0x90))
            {
                snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Vendor Specific - %" PRIX8 "h", testId);
            }
            else
            {
                snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Unknown - %" PRIX8 "h", testId);
            }
            break;
        }
    }
    else if (driveType == SCSI_DRIVE)
    {
        switch (testId)
        {
        case 0:
            snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Unknown (Not in spec)");
            break;
        case 1: // short
            snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Short (background)");
            break;
        case 2: // extended
            snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Extended (background)");
            break;
        case 5: // short
            snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Short (foreground)");
            break;
        case 6: // extended
            snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Extended (foreground)");
            break;
        default:
            snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Unknown - %" PRIX8 "h", testId);
            break;
        }
    }
    else if (driveType == NVME_DRIVE)
    {
        switch (testId)
        {
        case 0:
            snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Reserved");
            break;
        case 1: // short
            snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Short");
            break;
        case 2: // extended
            snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Extended");
            break;
        case 0x0E: // vendor specific
            snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Vendor Specific");
            break;
        default:
            snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Unknown - %" PRIX8 "h", testId);
            break;
        }
    }
    else
    {
        snprintf_err_handle(*testName, MAX_TEST_NAME_LENGTH, "Unknown - %" PRIX8 "h", testId);
    }
}

static void get_Execution_Status_Name(eDriveType driveType, dstDescriptor dstDescriptor, char** executionStatusString)
{
    safe_memset(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, 0,
                MAX_DST_EXECUTION_STATUS_STRING_LENGTH);

    if (driveType == NVME_DRIVE)
    {
        switch (M_Nibble1(dstDescriptor.selfTestExecutionStatus))
        {
        case 0:
            snprintf_err_handle(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "No Error");
            break;
        case 1:
            snprintf_err_handle(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Aborted by command");
            break;
        case 2:
            snprintf_err_handle(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH,
                                "Aborted by controller reset");
            break;
        case 3:
            snprintf_err_handle(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH,
                                "Aborted by namespace removal");
            break;
        case 4:
            snprintf_err_handle(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH,
                                "Aborted by NVM format");
            break;
        case 5:
            snprintf_err_handle(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Unknown/Fatal Error");
            break;
        case 6:
            snprintf_err_handle(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH,
                                "Unknown Segment Failure");
            break;
        case 7:
            snprintf_err_handle(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH,
                                "Failed on segment %" PRIu8 "", dstDescriptor.segmentNumber);
            break;
        case 8:
            snprintf_err_handle(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH,
                                "Aborted for Unknown Reason");
            break;
        default:
            snprintf_err_handle(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Reserved");
            break;
        }
    }
    else
    {
        switch (M_Nibble1(dstDescriptor.selfTestExecutionStatus))
        {
        case 0:
            snprintf_err_handle(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Success");
            break;
        case 1:
            snprintf_err_handle(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Aborted by host");
            break;
        case 2:
            snprintf_err_handle(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Interrupted by reset");
            break;
        case 3:
            snprintf_err_handle(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH,
                                "Fatal Error - Unknown");
            break;
        case 4:
            snprintf_err_handle(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Unknown Failure Type");
            break;
        case 5:
            snprintf_err_handle(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Electrical Failure");
            break;
        case 6:
            snprintf_err_handle(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Servo/Seek Failure");
            break;
        case 7:
            snprintf_err_handle(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Read Failure");
            break;
        case 8:
            snprintf_err_handle(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Handling Damage");
            break;
        case 0xF:
            snprintf_err_handle(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "In progress");
            break;
        default:
            snprintf_err_handle(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Reserved");
            break;
        }

        if (driveType == ATA_DRIVE)
        {
            uint8_t percentRemaining = M_Nibble0(dstDescriptor.selfTestExecutionStatus) * 10;
            if (percentRemaining > 0)
            {
                DECLARE_ZERO_INIT_ARRAY(char, percentRemainingString, 8);
                snprintf_err_handle(percentRemainingString, 8, " (%" PRIu8 "%%)", percentRemaining);
                safe_strcat(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, percentRemainingString);
            }
        }
    }
}

static void create_JSON_Nodes_For_DST_Log_Entries(json_object* rootObject, tDevice* device, ptrDstLogEntries entries)
{
    json_object* dstLog = json_object_new_object();

    DECLARE_ZERO_INIT_ARRAY(char, totalDSTEntriesValue, MAX_UINT8_TO_DEC_STRING_LENGHT);
    snprintf_err_handle(totalDSTEntriesValue, MAX_UINT8_TO_DEC_STRING_LENGHT, "%" PRIu8 "", entries->numberOfEntries);
    json_object_object_add(dstLog, "Total DST Entries", json_object_new_string(totalDSTEntriesValue));

    if (entries->numberOfEntries > UINT8_C(0))
    {
        // create array for dst log entries
        json_object* dstEntriesList = json_object_new_array();

        // add individual dst log entry in array
        for (uint32_t iter = UINT32_C(0); iter < entries->numberOfEntries; ++iter)
        {
            // create defect node
            json_object* logEntry = json_object_new_object();

            char* testName = M_REINTERPRET_CAST(char*, safe_calloc(MAX_TEST_NAME_LENGTH, sizeof(char)));
            get_Test_Name(device->drive_info.drive_type, entries->dstEntry[iter].selfTestRun, &testName);
            json_object_object_add(logEntry, "Test", json_object_new_string(testName));
            safe_free(&testName);

            DECLARE_ZERO_INIT_ARRAY(char, timestampValue, MAX_UINT64_TO_DEC_STRING_LENGHT);
            snprintf_err_handle(timestampValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "%" PRIu64 "",
                                entries->dstEntry[iter].lifetimeTimestamp);
            json_object_object_add(logEntry, "Timestamp", json_object_new_string(timestampValue));

            char* executionStatusValue =
                M_REINTERPRET_CAST(char*, safe_calloc(MAX_DST_EXECUTION_STATUS_STRING_LENGTH, sizeof(char)));
            get_Execution_Status_Name(device->drive_info.drive_type, entries->dstEntry[iter], &executionStatusValue);
            json_object_object_add(logEntry, "Execution Status", json_object_new_string(executionStatusValue));
            safe_free(&executionStatusValue);

            DECLARE_ZERO_INIT_ARRAY(char, errorLBAValue, MAX_UINT64_TO_DEC_STRING_LENGHT);
            if (entries->dstEntry[iter].lbaOfFailure == UINT64_MAX)
                snprintf_err_handle(errorLBAValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "None");
            else
                snprintf_err_handle(errorLBAValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "%" PRIu64 "",
                                    entries->dstEntry[iter].lbaOfFailure);
            json_object_object_add(logEntry, "Error LBA", json_object_new_string(errorLBAValue));

            DECLARE_ZERO_INIT_ARRAY(char, checkPointValue, MAX_UINT8_TO_HEX_STRING_LENGHT);
            snprintf_err_handle(checkPointValue, MAX_UINT8_TO_HEX_STRING_LENGHT, "%" PRIX8 "",
                                entries->dstEntry[iter].checkPointByte);
            if (device->drive_info.drive_type == NVME_DRIVE)
                json_object_object_add(logEntry, "Segment Number", json_object_new_string(checkPointValue));
            else
                json_object_object_add(logEntry, "Checkpoint", json_object_new_string(checkPointValue));

            DECLARE_ZERO_INIT_ARRAY(char, senseInfoValue, MAX_SENSE_INFO_STRING_LENGTH);
            if (device->drive_info.drive_type == NVME_DRIVE)
            {
#define NVM_STATUS_CODE_STR_LEN 10
                DECLARE_ZERO_INIT_ARRAY(char, sctVal, NVM_STATUS_CODE_STR_LEN);
                DECLARE_ZERO_INIT_ARRAY(char, scVal, NVM_STATUS_CODE_STR_LEN);
                if (entries->dstEntry[iter].nvmeStatus.statusCodeTypeValid)
                {
                    snprintf_err_handle(sctVal, NVM_STATUS_CODE_STR_LEN, "%02" PRIX8 "",
                                        entries->dstEntry[iter].nvmeStatus.statusCodeType);
                }
                else
                {
                    snprintf_err_handle(sctVal, NVM_STATUS_CODE_STR_LEN, "NA");
                }
                if (entries->dstEntry[iter].nvmeStatus.statusCodeValid)
                {
                    snprintf_err_handle(sctVal, NVM_STATUS_CODE_STR_LEN, "%02" PRIX8 "",
                                        entries->dstEntry[iter].nvmeStatus.statusCode);
                }
                else
                {
                    snprintf_err_handle(scVal, NVM_STATUS_CODE_STR_LEN, "NA");
                }
                snprintf_err_handle(senseInfoValue, MAX_SENSE_INFO_STRING_LENGTH, "%s/%s", sctVal, scVal);
            }
            else
            {
                snprintf_err_handle(senseInfoValue, MAX_SENSE_INFO_STRING_LENGTH, "%02" PRIX8 "/%02" PRIX8 "/%02" PRIX8,
                                    entries->dstEntry[iter].scsiSenseCode.senseKey,
                                    entries->dstEntry[iter].scsiSenseCode.additionalSenseCode,
                                    entries->dstEntry[iter].scsiSenseCode.additionalSenseCodeQualifier);
            }
            json_object_object_add(logEntry, "Sense Info", json_object_new_string(senseInfoValue));

            // add this entry into list
            json_object_array_add(dstEntriesList, logEntry);
        }

        // add array in root node
        json_object_object_add(dstLog, "Log Entries", dstEntriesList);
    }

    json_object_object_add(rootObject, "DST Log", dstLog);
}

eReturnValues create_JSON_Output_For_DST(tDevice*         device,
                                         ptrDstLogEntries entries,
                                         const char*      utilityName,
                                         const char*      buildVersion,
                                         char**           jsonFormat)
{
    if (entries == M_NULLPTR)
        return BAD_PARAMETER;

    json_object* rootNode = json_object_new_object();

    if (rootNode == M_NULLPTR)
        return MEMORY_FAILURE;

    create_Node_For_Utility_Version(rootNode, utilityName, buildVersion, "DST", DST_JSON_VERSION);
    create_Node_For_Drive_Information(rootNode, device);

    create_JSON_Nodes_For_DST_Log_Entries(rootNode, device, entries);

    // Convert JSON object to formatted string
    const char* jstr =
        json_object_to_json_string_ext(rootNode, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);

    // copy the json output into string
    if (asprintf(jsonFormat, "%s", jstr) < 0)
    {
        return MEMORY_FAILURE;
    }

    // Free the JSON object
    json_object_put(rootNode);

    return SUCCESS;
}