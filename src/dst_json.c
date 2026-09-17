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
// \file dst_json.c
// \brief This file defines types and functions related to the JSON-based output for DST logs.

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

#define DST_JSON_SNPRINTF(...)                                                                                         \
    M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(__VA_ARGS__),                                                           \
                           "DST JSON destination buffer is sized for the fixed display format")

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

static void get_Test_Name(eDriveType driveType, uint8_t testId, char** testName)
{
    M_IGNORE_SAFE_ERRNO_CALL(safe_memset(*testName, MAX_TEST_NAME_LENGTH, 0, MAX_TEST_NAME_LENGTH),
                             "clearing the full allocated test-name buffer cannot truncate");
    if (driveType == ATA_DRIVE)
    {
        switch (testId)
        {
        case 0:
            DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Offline Data Collect");
            break;
        case 1: // short
            DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Short (offline)");
            break;
        case 2: // extended
            DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Extended (offline)");
            break;
        case 3: // conveyance
            DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Conveyance (offline)");
            break;
        case 4: // selective
            DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Selective (offline)");
            break;
        case 0x81: // short
            DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Short (captive)");
            break;
        case 0x82: // extended
            DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Extended (captive)");
            break;
        case 0x83: // conveyance
            DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Conveyance (captive)");
            break;
        case 0x84: // selective
            DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Selective (captive)");
            break;
        default:
            if ((testId >= 0x40 && testId <= 0x7E) || (testId >= 0x90))
            {
                DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Vendor Specific - %" PRIX8 "h", testId);
            }
            else
            {
                DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Unknown - %" PRIX8 "h", testId);
            }
            break;
        }
    }
    else if (driveType == SCSI_DRIVE)
    {
        switch (testId)
        {
        case 0:
            DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Unknown (Not in spec)");
            break;
        case 1: // short
            DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Short (background)");
            break;
        case 2: // extended
            DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Extended (background)");
            break;
        case 5: // short
            DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Short (foreground)");
            break;
        case 6: // extended
            DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Extended (foreground)");
            break;
        default:
            DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Unknown - %" PRIX8 "h", testId);
            break;
        }
    }
    else if (driveType == NVME_DRIVE)
    {
        switch (testId)
        {
        case 0:
            DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Reserved");
            break;
        case 1: // short
            DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Short");
            break;
        case 2: // extended
            DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Extended");
            break;
        case 0x0E: // vendor specific
            DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Vendor Specific");
            break;
        default:
            DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Unknown - %" PRIX8 "h", testId);
            break;
        }
    }
    else
    {
        DST_JSON_SNPRINTF(*testName, MAX_TEST_NAME_LENGTH, "Unknown - %" PRIX8 "h", testId);
    }
}

static void get_Execution_Status_Name(eDriveType driveType, dstDescriptor dstDescriptor, char** executionStatusString)
{
    M_IGNORE_SAFE_ERRNO_CALL(safe_memset(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, 0,
                                         MAX_DST_EXECUTION_STATUS_STRING_LENGTH),
                             "clearing the full allocated execution-status buffer cannot truncate");

    if (driveType == NVME_DRIVE)
    {
        switch (M_Nibble1(dstDescriptor.selfTestExecutionStatus))
        {
        case 0:
            DST_JSON_SNPRINTF(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "No Error");
            break;
        case 1:
            DST_JSON_SNPRINTF(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Aborted by command");
            break;
        case 2:
            DST_JSON_SNPRINTF(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH,
                              "Aborted by controller reset");
            break;
        case 3:
            DST_JSON_SNPRINTF(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH,
                              "Aborted by namespace removal");
            break;
        case 4:
            DST_JSON_SNPRINTF(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Aborted by NVM format");
            break;
        case 5:
            DST_JSON_SNPRINTF(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Unknown/Fatal Error");
            break;
        case 6:
            DST_JSON_SNPRINTF(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH,
                              "Unknown Segment Failure");
            break;
        case 7:
            DST_JSON_SNPRINTF(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH,
                              "Failed on segment %" PRIu8 "", dstDescriptor.segmentNumber);
            break;
        case 8:
            DST_JSON_SNPRINTF(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH,
                              "Aborted for Unknown Reason");
            break;
        default:
            DST_JSON_SNPRINTF(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Reserved");
            break;
        }
    }
    else
    {
        switch (M_Nibble1(dstDescriptor.selfTestExecutionStatus))
        {
        case 0:
            DST_JSON_SNPRINTF(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Success");
            break;
        case 1:
            DST_JSON_SNPRINTF(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Aborted by host");
            break;
        case 2:
            DST_JSON_SNPRINTF(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Interrupted by reset");
            break;
        case 3:
            DST_JSON_SNPRINTF(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Fatal Error - Unknown");
            break;
        case 4:
            DST_JSON_SNPRINTF(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Unknown Failure Type");
            break;
        case 5:
            DST_JSON_SNPRINTF(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Electrical Failure");
            break;
        case 6:
            DST_JSON_SNPRINTF(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Servo/Seek Failure");
            break;
        case 7:
            DST_JSON_SNPRINTF(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Read Failure");
            break;
        case 8:
            DST_JSON_SNPRINTF(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Handling Damage");
            break;
        case 0xF:
            DST_JSON_SNPRINTF(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "In progress");
            break;
        default:
            DST_JSON_SNPRINTF(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, "Reserved");
            break;
        }

        if (driveType == ATA_DRIVE)
        {
            uint8_t percentRemaining = M_Nibble0(dstDescriptor.selfTestExecutionStatus) * 10;
            if (percentRemaining > 0)
            {
                DECLARE_ZERO_INIT_ARRAY(char, percentRemainingString, 8);
                DST_JSON_SNPRINTF(percentRemainingString, 8, " (%" PRIu8 "%%)", percentRemaining);
                M_IGNORE_SAFE_ERRNO_CALL(
                    safe_strcat(*executionStatusString, MAX_DST_EXECUTION_STATUS_STRING_LENGTH, percentRemainingString),
                    "execution status and percentage strings fit in the allocated destination buffer");
            }
        }
    }
}

M_NODISCARD static eReturnValues create_JSON_Nodes_For_DST_Log_Entries(json_object*     rootObject,
                                                                       const tDevice*   device,
                                                                       ptrDstLogEntries entries)
{
    json_object* dstLog = json_object_new_object();
    if (dstLog == M_NULLPTR)
    {
        return MEMORY_FAILURE;
    }

    DECLARE_ZERO_INIT_ARRAY(char, totalDSTEntriesValue, MAX_UINT8_TO_DEC_STRING_LENGHT);
    DST_JSON_SNPRINTF(totalDSTEntriesValue, MAX_UINT8_TO_DEC_STRING_LENGHT, "%" PRIu8 "", entries->numberOfEntries);
    if (add_JSON_Object(dstLog, "Total DST Entries", json_object_new_string(totalDSTEntriesValue)) != 0)
    {
        json_object_put(dstLog);
        return MEMORY_FAILURE;
    }

    if (entries->numberOfEntries > UINT8_C(0))
    {
        // create array for dst log entries
        json_object* dstEntriesList = json_object_new_array();
        if (dstEntriesList == M_NULLPTR)
        {
            json_object_put(dstLog);
            return MEMORY_FAILURE;
        }
        if (add_JSON_Object(dstLog, "Log Entries", dstEntriesList) != 0)
        {
            json_object_put(dstLog);
            return MEMORY_FAILURE;
        }

        // add individual dst log entry in array
        for (uint32_t iter = UINT32_C(0); iter < entries->numberOfEntries; ++iter)
        {
            // create defect node
            json_object* logEntry = json_object_new_object();
            if (logEntry == M_NULLPTR)
            {
                json_object_put(dstLog);
                return MEMORY_FAILURE;
            }

            char* testName = M_REINTERPRET_CAST(char*, safe_calloc(MAX_TEST_NAME_LENGTH, sizeof(char)));
            if (testName == M_NULLPTR)
            {
                json_object_put(logEntry);
                json_object_put(dstLog);
                return MEMORY_FAILURE;
            }
            get_Test_Name(get_Device_DriveType(device), entries->dstEntry[iter].selfTestRun, &testName);
            if (add_JSON_Object(logEntry, "Test", json_object_new_string(testName)) != 0)
            {
                safe_free(&testName);
                json_object_put(logEntry);
                json_object_put(dstLog);
                return MEMORY_FAILURE;
            }
            safe_free(&testName);

            DECLARE_ZERO_INIT_ARRAY(char, timestampValue, MAX_UINT64_TO_DEC_STRING_LENGHT);
            DST_JSON_SNPRINTF(timestampValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "%" PRIu64 "",
                              entries->dstEntry[iter].lifetimeTimestamp);
            if (add_JSON_Object(logEntry, "Timestamp", json_object_new_string(timestampValue)) != 0)
            {
                json_object_put(logEntry);
                json_object_put(dstLog);
                return MEMORY_FAILURE;
            }

            char* executionStatusValue =
                M_REINTERPRET_CAST(char*, safe_calloc(MAX_DST_EXECUTION_STATUS_STRING_LENGTH, sizeof(char)));
            if (executionStatusValue == M_NULLPTR)
            {
                json_object_put(logEntry);
                json_object_put(dstLog);
                return MEMORY_FAILURE;
            }
            get_Execution_Status_Name(get_Device_DriveType(device), entries->dstEntry[iter], &executionStatusValue);
            if (add_JSON_Object(logEntry, "Execution Status", json_object_new_string(executionStatusValue)) != 0)
            {
                safe_free(&executionStatusValue);
                json_object_put(logEntry);
                json_object_put(dstLog);
                return MEMORY_FAILURE;
            }
            safe_free(&executionStatusValue);

            DECLARE_ZERO_INIT_ARRAY(char, errorLBAValue, MAX_UINT64_TO_DEC_STRING_LENGHT);
            if (entries->dstEntry[iter].lbaOfFailure == UINT64_MAX)
            {
                DST_JSON_SNPRINTF(errorLBAValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "None");
            }
            else
            {
                DST_JSON_SNPRINTF(errorLBAValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "%" PRIu64 "",
                                  entries->dstEntry[iter].lbaOfFailure);
            }
            if (add_JSON_Object(logEntry, "Error LBA", json_object_new_string(errorLBAValue)) != 0)
            {
                json_object_put(logEntry);
                json_object_put(dstLog);
                return MEMORY_FAILURE;
            }

            DECLARE_ZERO_INIT_ARRAY(char, checkPointValue, MAX_UINT8_TO_HEX_STRING_LENGHT);
            DST_JSON_SNPRINTF(checkPointValue, MAX_UINT8_TO_HEX_STRING_LENGHT, "%" PRIX8 "",
                              entries->dstEntry[iter].checkPointByte);
            if (get_Device_DriveType(device) == NVME_DRIVE)
            {
                if (add_JSON_Object(logEntry, "Segment Number", json_object_new_string(checkPointValue)) != 0)
                {
                    json_object_put(logEntry);
                    json_object_put(dstLog);
                    return MEMORY_FAILURE;
                }
            }
            else
            {
                if (add_JSON_Object(logEntry, "Checkpoint", json_object_new_string(checkPointValue)) != 0)
                {
                    json_object_put(logEntry);
                    json_object_put(dstLog);
                    return MEMORY_FAILURE;
                }
            }

            DECLARE_ZERO_INIT_ARRAY(char, senseInfoValue, MAX_SENSE_INFO_STRING_LENGTH);
            if (get_Device_DriveType(device) == NVME_DRIVE)
            {
#define NVM_STATUS_CODE_STR_LEN 10
                DECLARE_ZERO_INIT_ARRAY(char, sctVal, NVM_STATUS_CODE_STR_LEN);
                DECLARE_ZERO_INIT_ARRAY(char, scVal, NVM_STATUS_CODE_STR_LEN);
                if (entries->dstEntry[iter].nvmeStatus.statusCodeTypeValid)
                {
                    DST_JSON_SNPRINTF(sctVal, NVM_STATUS_CODE_STR_LEN, "%02" PRIX8 "",
                                      entries->dstEntry[iter].nvmeStatus.statusCodeType);
                }
                else
                {
                    DST_JSON_SNPRINTF(sctVal, NVM_STATUS_CODE_STR_LEN, "NA");
                }
                if (entries->dstEntry[iter].nvmeStatus.statusCodeValid)
                {
                    DST_JSON_SNPRINTF(scVal, NVM_STATUS_CODE_STR_LEN, "%02" PRIX8 "",
                                      entries->dstEntry[iter].nvmeStatus.statusCode);
                }
                else
                {
                    DST_JSON_SNPRINTF(scVal, NVM_STATUS_CODE_STR_LEN, "NA");
                }
                DST_JSON_SNPRINTF(senseInfoValue, MAX_SENSE_INFO_STRING_LENGTH, "%s/%s", sctVal, scVal);
            }
            else
            {
                DST_JSON_SNPRINTF(senseInfoValue, MAX_SENSE_INFO_STRING_LENGTH, "%02" PRIX8 "/%02" PRIX8 "/%02" PRIX8,
                                  entries->dstEntry[iter].scsiSenseCode.senseKey,
                                  entries->dstEntry[iter].scsiSenseCode.additionalSenseCode,
                                  entries->dstEntry[iter].scsiSenseCode.additionalSenseCodeQualifier);
            }
            if (add_JSON_Object(logEntry, "Sense Info", json_object_new_string(senseInfoValue)) != 0)
            {
                json_object_put(logEntry);
                json_object_put(dstLog);
                return MEMORY_FAILURE;
            }

            // add this entry into list
            if (add_JSON_Array_Element(dstEntriesList, logEntry) != 0)
            {
                json_object_put(dstLog);
                return MEMORY_FAILURE;
            }
        }
    }

    if (add_JSON_Object(rootObject, "DST Log", dstLog) != 0)
    {
        return MEMORY_FAILURE;
    }

    return SUCCESS;
}

M_PARAM_RO(1)
M_PARAM_RO(2)
M_PARAM_RO(3)
M_PARAM_RO(4)
M_PARAM_WO(5)
M_NODISCARD OPENSEA_JSONFORMAT_API eReturnValues create_JSON_Output_For_DST(const tDevice* M_NONNULL   device,
                                                                            ptrDstLogEntries M_NONNULL entries,
                                                                            const char* M_NONNULL      utilityName,
                                                                            const char* M_NONNULL      buildVersion,
                                                                            char**                     jsonFormat)
{
    if (device == M_NULLPTR || entries == M_NULLPTR || jsonFormat == M_NULLPTR)
        return BAD_PARAMETER;

    *jsonFormat = M_NULLPTR;

    json_object* rootNode = json_object_new_object();

    if (rootNode == M_NULLPTR)
        return MEMORY_FAILURE;

    if (create_Node_For_Utility_Version(rootNode, utilityName, buildVersion, "DST", DST_JSON_VERSION) != SUCCESS ||
        create_Node_For_Drive_Information(rootNode, device) != SUCCESS)
    {
        json_object_put(rootNode);
        return MEMORY_FAILURE;
    }

    eReturnValues ret = create_JSON_Nodes_For_DST_Log_Entries(rootNode, device, entries);
    if (ret != SUCCESS)
    {
        json_object_put(rootNode);
        return ret;
    }

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
