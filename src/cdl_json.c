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
// \file cdl_json.c
// \brief This file defines types and functions related to the JSON-based output for Seagate CDL config.

#include <json.h>
#include <json_object.h>

#include "cdl_json.h"
#include "io_utils.h"
#include "logs.h"
#include "math_utils.h"
#include "memory_safety.h"
#include "secure_file.h"
#include "string_utils.h"

#define MAX_TIME_UNIT_STRING_LENGHT 10

static void translate_TimeUnitType_To_String(eCDLTimeFieldUnitType unitType, char* translatedString)
{
    switch (unitType)
    {
    case CDL_TIME_FIELD_UNIT_TYPE_SECONDS:
        snprintf_err_handle(translatedString, MAX_TIME_UNIT_STRING_LENGHT, "s");
        break;

    case CDL_TIME_FIELD_UNIT_TYPE_MILLISECONDS:
        snprintf_err_handle(translatedString, MAX_TIME_UNIT_STRING_LENGHT, "ms");
        break;

    case CDL_TIME_FIELD_UNIT_TYPE_500_NANOSECONDS:
        snprintf_err_handle(translatedString, MAX_TIME_UNIT_STRING_LENGHT, "500 ns");
        break;

    case CDL_TIME_FIELD_UNIT_TYPE_10_MILLISECONDS:
        snprintf_err_handle(translatedString, MAX_TIME_UNIT_STRING_LENGHT, "10 ms");
        break;

    case CDL_TIME_FIELD_UNIT_TYPE_500_MILLISECONDS:
        snprintf_err_handle(translatedString, MAX_TIME_UNIT_STRING_LENGHT, "500 ms");
        break;

    case CDL_TIME_FIELD_UNIT_TYPE_NO_VALUE:
        snprintf_err_handle(translatedString, MAX_TIME_UNIT_STRING_LENGHT, "NA");
        break;

    case CDL_TIME_FIELD_UNIT_TYPE_MICROSECONDS:
    case CDL_TIME_FIELD_UNIT_TYPE_RESERVED:
    default:
        snprintf_err_handle(translatedString, MAX_TIME_UNIT_STRING_LENGHT, "us");
        break;
    }
}

static void translate_String_To_TimeUnitType(const char* unitString, eCDLTimeFieldUnitType* unitType)
{
    if (strcmp(unitString, "s") == 0)
    {
        *unitType = CDL_TIME_FIELD_UNIT_TYPE_SECONDS;
    }
    else if (strcmp(unitString, "ms") == 0)
    {
        *unitType = CDL_TIME_FIELD_UNIT_TYPE_MILLISECONDS;
    }
    else if (strcmp(unitString, "500 ns") == 0)
    {
        *unitType = CDL_TIME_FIELD_UNIT_TYPE_500_NANOSECONDS;
    }
    else if (strcmp(unitString, "10 ms") == 0)
    {
        *unitType = CDL_TIME_FIELD_UNIT_TYPE_10_MILLISECONDS;
    }
    else if (strcmp(unitString, "500 ms") == 0)
    {
        *unitType = CDL_TIME_FIELD_UNIT_TYPE_500_MILLISECONDS;
    }
    else if (strcmp(unitString, "NA") == 0)
    {
        *unitType = CDL_TIME_FIELD_UNIT_TYPE_NO_VALUE;
    }
    else
    {
        *unitType = CDL_TIME_FIELD_UNIT_TYPE_MICROSECONDS;
    }
}

static eReturnValues create_ATA_JSON_File_For_CDL_Settings(tDevice*      device,
                                                           tCDLSettings* cdlSettings,
                                                           const char*   logPath)
{
    eReturnValues ret = SUCCESS;

    // Create a new JSON object
    json_object* rootObj = json_object_new_object();
    if (rootObj != M_NULLPTR)
    {
        // Add version information
        json_object_object_add(rootObj, "CDL Feature Version", json_object_new_string(CDL_FEATURE_VERSION));

        // Add Performance Versus Command Completion to the JSON object
        if (is_Performance_Versus_Command_Completion_Supported(cdlSettings))
        {
            DECLARE_ZERO_INIT_ARRAY(char, value, 5);
            snprintf_err_handle(value, 5, "0x%02" PRIX8 "", cdlSettings->ataCDLSettings.performanceVsCommandCompletion);
            json_object_object_add(rootObj, "Performance Versus Command Completion", json_object_new_string(value));
        }

        // Add JSON objects for read descriptor
        for (uint8_t descriptorIndex = 0; descriptorIndex < MAX_CDL_READ_DESCRIPTOR; descriptorIndex++)
        {
            json_object* jdescriptor = json_object_new_object();
            DECLARE_ZERO_INIT_ARRAY(char, timeUnitValue, MAX_TIME_UNIT_STRING_LENGHT);
            translate_TimeUnitType_To_String(
                cdlSettings->ataCDLSettings.cdlReadDescriptor[descriptorIndex].timeFieldUnitType, timeUnitValue);
            json_object_object_add(jdescriptor, "Time Field Unit", json_object_new_string(timeUnitValue));
            json_object_object_add(
                jdescriptor, "Inactive Time",
                json_object_new_uint64(cdlSettings->ataCDLSettings.cdlReadDescriptor[descriptorIndex].inactiveTime));
            DECLARE_ZERO_INIT_ARRAY(char, value, 5);
            snprintf_err_handle(value, 5, "0x%02" PRIX8 "",
                                cdlSettings->ataCDLSettings.cdlReadDescriptor[descriptorIndex].inactiveTimePolicy);
            json_object_object_add(jdescriptor, "Inactive Time Policy", json_object_new_string(value));
            json_object_object_add(
                jdescriptor, "Active Time",
                json_object_new_uint64(cdlSettings->ataCDLSettings.cdlReadDescriptor[descriptorIndex].activeTime));
            snprintf_err_handle(value, 5, "0x%02" PRIX8 "",
                                cdlSettings->ataCDLSettings.cdlReadDescriptor[descriptorIndex].activeTimePolicy);
            json_object_object_add(jdescriptor, "Active Time Policy", json_object_new_string(value));
            if (is_Total_Time_Policy_Type_Supported(cdlSettings))
            {
                json_object_object_add(
                    jdescriptor, "Total Time",
                    json_object_new_uint64(cdlSettings->ataCDLSettings.cdlReadDescriptor[descriptorIndex].totalTime));
                snprintf_err_handle(value, 5, "0x%02" PRIX8 "",
                                    cdlSettings->ataCDLSettings.cdlReadDescriptor[descriptorIndex].totalTimePolicy);
                json_object_object_add(jdescriptor, "Total Time Policy", json_object_new_string(value));
            }

            // Add the read descriptor object to the main JSON object
            char descriptorKey[20];
            snprintf_err_handle(descriptorKey, 20, "Descriptor R%" PRIu8 "", C_CAST(uint8_t, (descriptorIndex + 1)));
            json_object_object_add(rootObj, descriptorKey, jdescriptor);
        }

        // Add JSON objects for write descriptor
        for (uint8_t descriptorIndex = 0; descriptorIndex < MAX_CDL_WRITE_DESCRIPTOR; descriptorIndex++)
        {
            json_object* jdescriptor = json_object_new_object();
            DECLARE_ZERO_INIT_ARRAY(char, timeUnitValue, MAX_TIME_UNIT_STRING_LENGHT);
            translate_TimeUnitType_To_String(
                cdlSettings->ataCDLSettings.cdlWriteDescriptor[descriptorIndex].timeFieldUnitType, timeUnitValue);
            json_object_object_add(jdescriptor, "Time Field Unit", json_object_new_string(timeUnitValue));
            json_object_object_add(
                jdescriptor, "Inactive Time",
                json_object_new_uint64(cdlSettings->ataCDLSettings.cdlWriteDescriptor[descriptorIndex].inactiveTime));
            DECLARE_ZERO_INIT_ARRAY(char, value, 5);
            snprintf_err_handle(value, 5, "0x%02" PRIX8 "",
                                cdlSettings->ataCDLSettings.cdlWriteDescriptor[descriptorIndex].inactiveTimePolicy);
            json_object_object_add(jdescriptor, "Inactive Time Policy", json_object_new_string(value));
            json_object_object_add(
                jdescriptor, "Active Time",
                json_object_new_uint64(cdlSettings->ataCDLSettings.cdlWriteDescriptor[descriptorIndex].activeTime));
            snprintf_err_handle(value, 5, "0x%02" PRIX8 "",
                                cdlSettings->ataCDLSettings.cdlWriteDescriptor[descriptorIndex].activeTimePolicy);
            json_object_object_add(jdescriptor, "Active Time Policy", json_object_new_string(value));
            if (is_Total_Time_Policy_Type_Supported(cdlSettings))
            {
                json_object_object_add(
                    jdescriptor, "Total Time",
                    json_object_new_uint64(cdlSettings->ataCDLSettings.cdlWriteDescriptor[descriptorIndex].totalTime));
                snprintf_err_handle(value, 5, "0x%02" PRIX8 "",
                                    cdlSettings->ataCDLSettings.cdlWriteDescriptor[descriptorIndex].totalTimePolicy);
                json_object_object_add(jdescriptor, "Total Time Policy", json_object_new_string(value));
            }

            // Add the write descriptor object to the main JSON object
            char descriptorKey[20];
            snprintf_err_handle(descriptorKey, 20, "Descriptor W%" PRIu8 "", C_CAST(uint8_t, (descriptorIndex + 1)));
            json_object_object_add(rootObj, descriptorKey, jdescriptor);
        }

        // Convert JSON object to formatted string
        const char* jstr = json_object_to_json_string_ext(rootObj, JSON_C_TO_STRING_PRETTY);

        // Write formatted JSON string to a file
        secureFileInfo* cdlJsonLog = M_NULLPTR;
        ret = create_And_Open_Secure_Log_File_Dev_EZ(device, &cdlJsonLog, NAMING_SERIAL_NUMBER_DATE_TIME, logPath,
                                                     "CDL", "json");
        if (SUCCESS == ret)
        {
            if (SEC_FILE_SUCCESS != secure_fprintf_File(cdlJsonLog, "%s", jstr))
            {
                if (VERBOSITY_QUIET < device->deviceVerbosity)
                {
                    printf("Error writing in file!\n");
                }
                ret = ERROR_WRITING_FILE;
            }

            if (SEC_FILE_SUCCESS != secure_Flush_File(cdlJsonLog))
            {
                if (VERBOSITY_QUIET < device->deviceVerbosity)
                {
                    printf("Error flushing data!\n");
                }
                ret = ERROR_WRITING_FILE;
            }

            if (SEC_FILE_SUCCESS != secure_Close_File(cdlJsonLog))
            {
                printf("Error closing file!\n");
            }

            if (ret == SUCCESS)
            {
                printf("\tCommand Duration Limit : Supported, %s\n", cdlSettings->isEnabled ? "Enabled" : "Disabled");
                uint32_t currentCDLFeatureVersion = M_BytesTo4ByteValue(
                    CDL_FEATURE_MAJOR_VERSION, CDL_FEATURE_MINOR_VERSION, CDL_FEATURE_PATCH_VERSION, 0);
                if (currentCDLFeatureVersion == 0x01000000)
                    printf("\tCommand Duration Guideline : %s\n",
                           cdlSettings->ataCDLSettings.isCommandDurationGuidelineSupported ? "Supported"
                                                                                           : "Not Supported");
                printf("\tCommand Duration Limit Minimum Limit (us) : %" PRIu32 "\n",
                       cdlSettings->ataCDLSettings.minimumTimeLimit);
                printf("\tCommand Duration Limit Maximum Limit (us) : %" PRIu32 "\n",
                       cdlSettings->ataCDLSettings.maximumTimeLimit);
                DECLARE_ZERO_INIT_ARRAY(char, inactivePolicyString, SUPPORTED_POLICY_STRING_LENGTH);
                get_Supported_Policy_String(ATA_DRIVE, CDL_POLICY_TYPE_INACTIVE_TIME,
                                            cdlSettings->ataCDLSettings.inactiveTimePolicySupportedDescriptor,
                                            inactivePolicyString);
                printf("\tSupported Inactive Time Policy : %s\n", inactivePolicyString);
                DECLARE_ZERO_INIT_ARRAY(char, activePolicyString, SUPPORTED_POLICY_STRING_LENGTH);
                get_Supported_Policy_String(ATA_DRIVE, CDL_POLICY_TYPE_ACTIVE_TIME,
                                            cdlSettings->ataCDLSettings.activeTimePolicySupportedDescriptor,
                                            activePolicyString);
                printf("\tSupported Active Time Policy : %s\n", activePolicyString);
                if (is_Total_Time_Policy_Type_Supported(cdlSettings))
                {
                    DECLARE_ZERO_INIT_ARRAY(char, totalPolicyString, SUPPORTED_POLICY_STRING_LENGTH);
                    get_Supported_Policy_String(ATA_DRIVE, CDL_POLICY_TYPE_TOTAL_TIME,
                                                cdlSettings->ataCDLSettings.totalTimePolicySupportedDescriptor,
                                                totalPolicyString);
                    printf("\tSupported Total Time Policy : %s\n", totalPolicyString);
                }

                if (VERBOSITY_QUIET < device->deviceVerbosity)
                {
                    printf("Output - %s.\n", cdlJsonLog->fullpath);
                }
            }
        }
        else if (ret == INSECURE_PATH)
        {
            if (VERBOSITY_QUIET < device->deviceVerbosity)
            {
                printf("File path is not secure!\n");
            }
        }
        else
        {
            if (VERBOSITY_QUIET < device->deviceVerbosity)
            {
                printf("Error in opening the file!\n");
            }
        }

        free_Secure_File_Info(&cdlJsonLog);

        // Free the JSON object
        json_object_put(rootObj);
    }

    return ret;
}

static eReturnValues create_SCSI_JSON_File_For_CDL_Settings(tDevice*      device,
                                                            tCDLSettings* cdlSettings,
                                                            const char*   logPath)
{
    eReturnValues ret = SUCCESS;

    // Create a new JSON object
    json_object* rootObj = json_object_new_object();
    if (rootObj != M_NULLPTR)
    {
        // Add version information
        json_object_object_add(rootObj, "CDL Feature Version", json_object_new_string(CDL_FEATURE_VERSION));

        // Add Performance Versus Command Duration Guideline to the JSON object
        DECLARE_ZERO_INIT_ARRAY(char, value, 5);
        snprintf_err_handle(value, 5, "0x%02" PRIX8 "",
                            cdlSettings->scsiCDLSettings.performanceVsCommandDurationGuidelines);
        json_object_object_add(rootObj, "Performance Versus Command Duration Guidelines",
                               json_object_new_string(value));

        // Add JSON objects for T2A descriptor
        for (uint8_t descriptorIndex = 0; descriptorIndex < MAX_CDL_T2A_DESCRIPTOR; descriptorIndex++)
        {
            json_object* jdescriptor = json_object_new_object();
            DECLARE_ZERO_INIT_ARRAY(char, timeUnitValue, MAX_TIME_UNIT_STRING_LENGHT);
            translate_TimeUnitType_To_String(
                cdlSettings->scsiCDLSettings.cdlT2ADescriptor[descriptorIndex].timeFieldUnitType, timeUnitValue);
            json_object_object_add(jdescriptor, "Time Field Unit", json_object_new_string(timeUnitValue));
            json_object_object_add(
                jdescriptor, "Inactive Time",
                json_object_new_uint64(cdlSettings->scsiCDLSettings.cdlT2ADescriptor[descriptorIndex].inactiveTime));
            snprintf_err_handle(value, 5, "0x%02" PRIX8 "",
                                cdlSettings->scsiCDLSettings.cdlT2ADescriptor[descriptorIndex].inactiveTimePolicy);
            json_object_object_add(jdescriptor, "Inactive Time Policy", json_object_new_string(value));
            json_object_object_add(
                jdescriptor, "Active Time",
                json_object_new_uint64(cdlSettings->scsiCDLSettings.cdlT2ADescriptor[descriptorIndex].activeTime));
            snprintf_err_handle(value, 5, "0x%02" PRIX8 "",
                                cdlSettings->scsiCDLSettings.cdlT2ADescriptor[descriptorIndex].activeTimePolicy);
            json_object_object_add(jdescriptor, "Active Time Policy", json_object_new_string(value));
            json_object_object_add(
                jdescriptor, "Command Duration Guideline",
                json_object_new_uint64(
                    cdlSettings->scsiCDLSettings.cdlT2ADescriptor[descriptorIndex].commandDurationGuideline));
            snprintf_err_handle(
                value, 5, "0x%02" PRIX8 "",
                cdlSettings->scsiCDLSettings.cdlT2ADescriptor[descriptorIndex].CommandDurationGuidelinePolicy);
            json_object_object_add(jdescriptor, "Command Duration Guideline Policy", json_object_new_string(value));

            // Add the T2A descriptor object to the main JSON object
            char descriptorKey[20];
            snprintf_err_handle(descriptorKey, 20, "T2A Descriptor %" PRIu8 "", C_CAST(uint8_t, (descriptorIndex + 1)));
            json_object_object_add(rootObj, descriptorKey, jdescriptor);
        }

        // Add JSON objects for T2B descriptor
        for (uint8_t descriptorIndex = 0; descriptorIndex < MAX_CDL_T2B_DESCRIPTOR; descriptorIndex++)
        {
            json_object* jdescriptor = json_object_new_object();
            DECLARE_ZERO_INIT_ARRAY(char, timeUnitValue, MAX_TIME_UNIT_STRING_LENGHT);
            translate_TimeUnitType_To_String(
                cdlSettings->scsiCDLSettings.cdlT2BDescriptor[descriptorIndex].timeFieldUnitType, timeUnitValue);
            json_object_object_add(jdescriptor, "Time Field Unit", json_object_new_string(timeUnitValue));
            json_object_object_add(
                jdescriptor, "Inactive Time",
                json_object_new_uint64(cdlSettings->scsiCDLSettings.cdlT2BDescriptor[descriptorIndex].inactiveTime));
            snprintf_err_handle(value, 5, "0x%02" PRIX8 "",
                                cdlSettings->scsiCDLSettings.cdlT2BDescriptor[descriptorIndex].inactiveTimePolicy);
            json_object_object_add(jdescriptor, "Inactive Time Policy", json_object_new_string(value));
            json_object_object_add(
                jdescriptor, "Active Time",
                json_object_new_uint64(cdlSettings->scsiCDLSettings.cdlT2BDescriptor[descriptorIndex].activeTime));
            snprintf_err_handle(value, 5, "0x%02" PRIX8 "",
                                cdlSettings->scsiCDLSettings.cdlT2BDescriptor[descriptorIndex].activeTimePolicy);
            json_object_object_add(jdescriptor, "Active Time Policy", json_object_new_string(value));
            json_object_object_add(
                jdescriptor, "Command Duration Guideline",
                json_object_new_uint64(
                    cdlSettings->scsiCDLSettings.cdlT2BDescriptor[descriptorIndex].commandDurationGuideline));
            snprintf_err_handle(
                value, 5, "0x%02" PRIX8 "",
                cdlSettings->scsiCDLSettings.cdlT2BDescriptor[descriptorIndex].CommandDurationGuidelinePolicy);
            json_object_object_add(jdescriptor, "Command Duration Guideline Policy", json_object_new_string(value));

            // Add the T2B descriptor object to the main JSON object
            char descriptorKey[20];
            snprintf_err_handle(descriptorKey, 20, "T2B Descriptor %" PRIu8 "", C_CAST(uint8_t, (descriptorIndex + 1)));
            json_object_object_add(rootObj, descriptorKey, jdescriptor);
        }

        // Convert JSON object to formatted string
        const char* jstr = json_object_to_json_string_ext(rootObj, JSON_C_TO_STRING_PRETTY);

        // Write formatted JSON string to a file
        secureFileInfo* cdlJsonLog = M_NULLPTR;
        ret = create_And_Open_Secure_Log_File_Dev_EZ(device, &cdlJsonLog, NAMING_SERIAL_NUMBER_DATE_TIME, logPath,
                                                     "CDL", "json");
        if (SUCCESS == ret)
        {
            if (SEC_FILE_SUCCESS != secure_fprintf_File(cdlJsonLog, "%s", jstr))
            {
                if (VERBOSITY_QUIET < device->deviceVerbosity)
                {
                    printf("Error writing in file!\n");
                }
                ret = ERROR_WRITING_FILE;
            }

            if (SEC_FILE_SUCCESS != secure_Flush_File(cdlJsonLog))
            {
                if (VERBOSITY_QUIET < device->deviceVerbosity)
                {
                    printf("Error flushing data!\n");
                }
                ret = ERROR_WRITING_FILE;
            }

            if (SEC_FILE_SUCCESS != secure_Close_File(cdlJsonLog))
            {
                printf("Error closing file!\n");
            }

            if (ret == SUCCESS)
            {
                printf("\tCommand Duration Limit : Supported\n");
                printf("\tCommand Duration Guideline : Supported\n");
                printf("\tCommand Duration Limit Minimum Limit (ns) : %llu\n", 500ULL); // TODO - read values from drive
                printf("\tCommand Duration Limit Maximum Limit (ns) : %llu\n",
                       (500000000ULL * 500000000ULL)); // TODO - read values from drive
                DECLARE_ZERO_INIT_ARRAY(char, inactivePolicyString, SUPPORTED_POLICY_STRING_LENGTH);
                get_Supported_Policy_String(SCSI_DRIVE, CDL_POLICY_TYPE_INACTIVE_TIME, 0, inactivePolicyString);
                printf("\tSupported Inactive Time Policy : %s\n", inactivePolicyString);
                DECLARE_ZERO_INIT_ARRAY(char, activePolicyString, SUPPORTED_POLICY_STRING_LENGTH);
                get_Supported_Policy_String(SCSI_DRIVE, CDL_POLICY_TYPE_ACTIVE_TIME, 0, activePolicyString);
                printf("\tSupported Active Time Policy : %s\n", activePolicyString);
                DECLARE_ZERO_INIT_ARRAY(char, commandDurationGuidelinePolicyString, SUPPORTED_POLICY_STRING_LENGTH);
                get_Supported_Policy_String(SCSI_DRIVE, CDL_POLICY_TYPE_COMMAND_DURATION_GUIDELINE, 0,
                                            commandDurationGuidelinePolicyString);
                printf("\tSupported Command Duration Guideline Policy : %s\n", commandDurationGuidelinePolicyString);

                if (VERBOSITY_QUIET < device->deviceVerbosity)
                {
                    printf("Output - %s.\n", cdlJsonLog->fullpath);
                }
            }
        }
        else if (ret == INSECURE_PATH)
        {
            if (VERBOSITY_QUIET < device->deviceVerbosity)
            {
                printf("File path is not secure!\n");
            }
        }
        else
        {
            if (VERBOSITY_QUIET < device->deviceVerbosity)
            {
                printf("Error in opening the file!\n");
            }
        }

        free_Secure_File_Info(&cdlJsonLog);

        // Free the JSON object
        json_object_put(rootObj);
    }

    return ret;
}

eReturnValues create_JSON_File_For_CDL_Settings(tDevice* device, tCDLSettings* cdlSettings, const char* logPath)
{
    eReturnValues ret = NOT_SUPPORTED;

    if (device->drive_info.drive_type == ATA_DRIVE)
    {
        ret = create_ATA_JSON_File_For_CDL_Settings(device, cdlSettings, logPath);
    }
    else if (device->drive_info.drive_type == SCSI_DRIVE)
    {
        ret = create_SCSI_JSON_File_For_CDL_Settings(device, cdlSettings, logPath);
    }

    return ret;
}

static eReturnValues parse_ATA_JSON_File_For_CDL_Settings(tDevice*      device,
                                                          tCDLSettings* cdlSettings,
                                                          const char*   fileName,
                                                          bool          skipValidation)
{
    eReturnValues ret = SUCCESS;

    secureFileInfo* cdlJsonfile = secure_Open_File(fileName, "r", M_NULLPTR, M_NULLPTR, M_NULLPTR);
    if (cdlJsonfile && cdlJsonfile->error == SEC_FILE_SUCCESS)
    {
        char* jsonMem = C_CAST(char*, safe_calloc(cdlJsonfile->fileSize, sizeof(uint8_t)));
        if (jsonMem)
        {
            if (SEC_FILE_SUCCESS == secure_Read_File(cdlJsonfile, jsonMem, cdlJsonfile->fileSize, sizeof(char),
                                                     cdlJsonfile->fileSize, M_NULLPTR))
            {
                json_object* rootObj = json_tokener_parse(jsonMem);
                if (rootObj != M_NULLPTR)
                {
                    // Parse CDL Feature version
                    struct json_object* childObj = M_NULLPTR;
                    if (json_object_object_get_ex(rootObj, "CDL Feature Version", &childObj) != 0)
                    {
                        const char* charValue = json_object_get_string(childObj);
                        if (strncmp(charValue, CDL_FEATURE_VERSION,
                                    M_Min(safe_strlen(charValue), safe_strlen(CDL_FEATURE_VERSION))) != 0)
                        {
                            printf("\"CDL Feature Version\" for provided JSON file doesn't match with current version. "
                                   "Please generate the JSON file with current version of tool.\n");
                            ret = VALIDATION_FAILURE;
                        }
                    }

                    // Parse performance vs command completion
                    if (json_object_object_get_ex(rootObj, "Performance Versus Command Completion", &childObj) != 0)
                    {
                        const char* charValue = json_object_get_string(childObj);
                        if (!get_And_Validate_Integer_Input_Uint8(
                                charValue, M_NULLPTR, ALLOW_UNIT_NONE,
                                &cdlSettings->ataCDLSettings.performanceVsCommandCompletion))
                        {
                            printf("Invalid entry for \"Performance Versus Command Completion\"\n");
                            ret = VALIDATION_FAILURE;
                        }
                    }

                    // Parse read descriptor
                    for (uint8_t descriptorIndex = 0; descriptorIndex < MAX_CDL_READ_DESCRIPTOR; descriptorIndex++)
                    {
                        // get the descriptor json object
                        char descriptorKey[20];
                        snprintf_err_handle(descriptorKey, 20, "Descriptor R%" PRIu8 "",
                                            C_CAST(uint8_t, (descriptorIndex + 1)));
                        if (json_object_object_get_ex(rootObj, descriptorKey, &childObj) != 0)
                        {
                            struct json_object* descriptorObj = M_NULLPTR;

                            // Parse Time Field Unit Type
                            if (json_object_object_get_ex(childObj, "Time Field Unit", &descriptorObj) != 0)
                            {
                                const char* charValue = json_object_get_string(descriptorObj);
                                translate_String_To_TimeUnitType(
                                    charValue,
                                    &cdlSettings->ataCDLSettings.cdlReadDescriptor[descriptorIndex].timeFieldUnitType);
                            }

                            // Parse Inactive Time
                            if (json_object_object_get_ex(childObj, "Inactive Time", &descriptorObj) != 0)
                            {
                                uint32_t value = C_CAST(uint32_t, json_object_get_uint64(descriptorObj));
                                cdlSettings->ataCDLSettings.cdlReadDescriptor[descriptorIndex]
                                    .inactiveTime = convert_CDL_TimeField_To_Microseconds(
                                    cdlSettings->ataCDLSettings.cdlReadDescriptor[descriptorIndex].timeFieldUnitType,
                                    value);
                            }

                            // Parse Inactive Time Policy
                            if (json_object_object_get_ex(childObj, "Inactive Time Policy", &descriptorObj) != 0)
                            {
                                const char* charValue = json_object_get_string(descriptorObj);
                                if (!get_And_Validate_Integer_Input_Uint8(
                                        charValue, M_NULLPTR, ALLOW_UNIT_NONE,
                                        &cdlSettings->ataCDLSettings.cdlReadDescriptor[descriptorIndex]
                                             .inactiveTimePolicy))
                                {
                                    printf("Invalid Entry for \"Inactive Time Policy\" for Descriptor R%" PRIu8 ".\n",
                                           (descriptorIndex + 1));
                                    ret = VALIDATION_FAILURE;
                                }
                            }

                            // Parse Active Time
                            if (json_object_object_get_ex(childObj, "Active Time", &descriptorObj) != 0)
                            {
                                uint32_t value = C_CAST(uint32_t, json_object_get_uint64(descriptorObj));
                                cdlSettings->ataCDLSettings.cdlReadDescriptor[descriptorIndex]
                                    .activeTime = convert_CDL_TimeField_To_Microseconds(
                                    cdlSettings->ataCDLSettings.cdlReadDescriptor[descriptorIndex].timeFieldUnitType,
                                    value);
                            }

                            // Parse Active Time Policy
                            if (json_object_object_get_ex(childObj, "Active Time Policy", &descriptorObj) != 0)
                            {
                                const char* charValue = json_object_get_string(descriptorObj);
                                if (!get_And_Validate_Integer_Input_Uint8(
                                        charValue, M_NULLPTR, ALLOW_UNIT_NONE,
                                        &cdlSettings->ataCDLSettings.cdlReadDescriptor[descriptorIndex]
                                             .activeTimePolicy))
                                {
                                    printf("Invalid Entry for \"Active Time Policy\" for Descriptor R%" PRIu8 ".\n",
                                           (descriptorIndex + 1));
                                    ret = VALIDATION_FAILURE;
                                }
                            }

                            // Parse Total Time
                            if (json_object_object_get_ex(childObj, "Total Time", &descriptorObj) != 0)
                            {
                                uint32_t value = C_CAST(uint32_t, json_object_get_uint64(descriptorObj));
                                cdlSettings->ataCDLSettings.cdlReadDescriptor[descriptorIndex]
                                    .totalTime = convert_CDL_TimeField_To_Microseconds(
                                    cdlSettings->ataCDLSettings.cdlReadDescriptor[descriptorIndex].timeFieldUnitType,
                                    value);
                            }

                            // Parse Total Time Policy
                            if (json_object_object_get_ex(childObj, "Total Time Policy", &descriptorObj) != 0)
                            {
                                const char* charValue = json_object_get_string(descriptorObj);
                                if (!get_And_Validate_Integer_Input_Uint8(
                                        charValue, M_NULLPTR, ALLOW_UNIT_NONE,
                                        &cdlSettings->ataCDLSettings.cdlReadDescriptor[descriptorIndex]
                                             .totalTimePolicy))
                                {
                                    printf("Invalid Entry for \"Total Time Policy\" for Descriptor R%" PRIu8 ".\n",
                                           (descriptorIndex + 1));
                                    ret = VALIDATION_FAILURE;
                                }
                            }
                        }
                    }

                    // Parse write descriptor
                    for (uint8_t descriptorIndex = 0; descriptorIndex < MAX_CDL_WRITE_DESCRIPTOR; descriptorIndex++)
                    {
                        // get the descriptor json object
                        char descriptorKey[20];
                        snprintf_err_handle(descriptorKey, 20, "Descriptor W%" PRIu8 "",
                                            C_CAST(uint8_t, (descriptorIndex + 1)));
                        if (json_object_object_get_ex(rootObj, descriptorKey, &childObj) != 0)
                        {
                            struct json_object* descriptorObj = M_NULLPTR;

                            // Parse Time Field Unit Type
                            if (json_object_object_get_ex(childObj, "Time Field Unit", &descriptorObj) != 0)
                            {
                                const char* charValue = json_object_get_string(descriptorObj);
                                translate_String_To_TimeUnitType(
                                    charValue,
                                    &cdlSettings->ataCDLSettings.cdlWriteDescriptor[descriptorIndex].timeFieldUnitType);
                            }

                            // Parse Inactive Time
                            if (json_object_object_get_ex(childObj, "Inactive Time", &descriptorObj) != 0)
                            {
                                uint32_t value = C_CAST(uint32_t, json_object_get_uint64(descriptorObj));
                                cdlSettings->ataCDLSettings.cdlWriteDescriptor[descriptorIndex]
                                    .inactiveTime = convert_CDL_TimeField_To_Microseconds(
                                    cdlSettings->ataCDLSettings.cdlWriteDescriptor[descriptorIndex].timeFieldUnitType,
                                    value);
                            }

                            // Parse Inactive Time Policy
                            if (json_object_object_get_ex(childObj, "Inactive Time Policy", &descriptorObj) != 0)
                            {
                                const char* charValue = json_object_get_string(descriptorObj);
                                if (!get_And_Validate_Integer_Input_Uint8(
                                        charValue, M_NULLPTR, ALLOW_UNIT_NONE,
                                        &cdlSettings->ataCDLSettings.cdlWriteDescriptor[descriptorIndex]
                                             .inactiveTimePolicy))
                                {
                                    printf("Invalid Entry for \"Inactive Time Policy\" for Descriptor W%" PRIu8 ".\n",
                                           (descriptorIndex + 1));
                                    ret = VALIDATION_FAILURE;
                                }
                            }

                            // Parse Active Time
                            if (json_object_object_get_ex(childObj, "Active Time", &descriptorObj) != 0)
                            {
                                uint32_t value = C_CAST(uint32_t, json_object_get_uint64(descriptorObj));
                                cdlSettings->ataCDLSettings.cdlWriteDescriptor[descriptorIndex]
                                    .activeTime = convert_CDL_TimeField_To_Microseconds(
                                    cdlSettings->ataCDLSettings.cdlWriteDescriptor[descriptorIndex].timeFieldUnitType,
                                    value);
                            }

                            // Parse Active Time Policy
                            if (json_object_object_get_ex(childObj, "Active Time Policy", &descriptorObj) != 0)
                            {
                                const char* charValue = json_object_get_string(descriptorObj);
                                if (!get_And_Validate_Integer_Input_Uint8(
                                        charValue, M_NULLPTR, ALLOW_UNIT_NONE,
                                        &cdlSettings->ataCDLSettings.cdlWriteDescriptor[descriptorIndex]
                                             .activeTimePolicy))
                                {
                                    printf("Invalid Entry for \"Active Time Policy\" for Descriptor W%" PRIu8 ".\n",
                                           (descriptorIndex + 1));
                                    ret = VALIDATION_FAILURE;
                                }
                            }

                            // Parse Total Time
                            if (json_object_object_get_ex(childObj, "Total Time", &descriptorObj) != 0)
                            {
                                uint32_t value = C_CAST(uint32_t, json_object_get_uint64(descriptorObj));
                                cdlSettings->ataCDLSettings.cdlWriteDescriptor[descriptorIndex]
                                    .totalTime = convert_CDL_TimeField_To_Microseconds(
                                    cdlSettings->ataCDLSettings.cdlWriteDescriptor[descriptorIndex].timeFieldUnitType,
                                    value);
                            }

                            // Parse Total Time Policy
                            if (json_object_object_get_ex(childObj, "Total Time Policy", &descriptorObj) != 0)
                            {
                                const char* charValue = json_object_get_string(descriptorObj);
                                if (!get_And_Validate_Integer_Input_Uint8(
                                        charValue, M_NULLPTR, ALLOW_UNIT_NONE,
                                        &cdlSettings->ataCDLSettings.cdlWriteDescriptor[descriptorIndex]
                                             .totalTimePolicy))
                                {
                                    printf("Invalid Entry for \"Total Time Policy\" for Descriptor W%" PRIu8 ".\n",
                                           (descriptorIndex + 1));
                                    ret = VALIDATION_FAILURE;
                                }
                            }
                        }
                    }

                    // Free the json object
                    json_object_put(rootObj);

                    // validate the parsed CDL settings for the accepted values according to spec
                    if (ret == SUCCESS && !skipValidation)
                    {
                        ret = is_Valid_Config_CDL_Settings(device, cdlSettings);
                        if (ret != SUCCESS)
                        {
                            // Invalid json file
                            ret = VALIDATION_FAILURE;
                        }
                    }
                    else if (skipValidation)
                    {
                        printf("Skipping validation for JSON file. The operation could fail, if values provided are "
                               "not according to SPEC.\n");
                    }
                }
                else
                {
                    // Invalid json file or parsing failure
                    printf("Failure in parsing JSON file!\n");
                    ret = VALIDATION_FAILURE;
                }
            }
            else
            {
                ret = FILE_OPEN_ERROR;
            }

            safe_free(&jsonMem);
        }
        else
        {
            ret = MEMORY_FAILURE;
        }

        // parse json file
        if (SEC_FILE_SUCCESS != secure_Close_File(cdlJsonfile))
        {
            printf("Error attempting to close file!\n");
        }
    }
    else
    {
        if (VERBOSITY_QUIET < device->deviceVerbosity)
        {
            printf("Couldn't open file %s\n", fileName);
        }

        ret = FILE_OPEN_ERROR;
    }

    free_Secure_File_Info(&cdlJsonfile);

    return ret;
}

static eReturnValues parse_SCSI_JSON_File_For_CDL_Settings(tDevice*      device,
                                                           tCDLSettings* cdlSettings,
                                                           const char*   fileName,
                                                           bool          skipValidation)
{
    eReturnValues ret = SUCCESS;

    secureFileInfo* cdlJsonfile = secure_Open_File(fileName, "r", M_NULLPTR, M_NULLPTR, M_NULLPTR);
    if (cdlJsonfile && cdlJsonfile->error == SEC_FILE_SUCCESS)
    {
        char* jsonMem = C_CAST(char*, safe_calloc(cdlJsonfile->fileSize, sizeof(uint8_t)));
        if (jsonMem)
        {
            if (SEC_FILE_SUCCESS == secure_Read_File(cdlJsonfile, jsonMem, cdlJsonfile->fileSize, sizeof(char),
                                                     cdlJsonfile->fileSize, M_NULLPTR))
            {
                json_object* rootObj = json_tokener_parse(jsonMem);
                if (rootObj != M_NULLPTR)
                {
                    // Parse CDL Feature version
                    struct json_object* childObj = M_NULLPTR;
                    if (json_object_object_get_ex(rootObj, "CDL Feature Version", &childObj) != 0)
                    {
                        const char* charValue = json_object_get_string(childObj);
                        if (strncmp(charValue, CDL_FEATURE_VERSION,
                                    M_Min(safe_strlen(charValue), safe_strlen(CDL_FEATURE_VERSION))) != 0)
                        {
                            printf("\"CDL Feature Version\" for provided JSON file doesn't match with current version. "
                                   "Please generate the JSON file with current version of tool.\n");
                            ret = VALIDATION_FAILURE;
                        }
                    }

                    // Parse performance vs command duration guidelines
                    if (json_object_object_get_ex(rootObj, "Performance Versus Command Duration Guidelines",
                                                  &childObj) != 0)
                    {
                        const char* charValue = json_object_get_string(childObj);
                        if (!get_And_Validate_Integer_Input_Uint8(
                                charValue, M_NULLPTR, ALLOW_UNIT_NONE,
                                &cdlSettings->scsiCDLSettings.performanceVsCommandDurationGuidelines))
                        {
                            printf("Invalid entry for \"Performance Versus Command Duration Guidelines\"\n");
                            ret = VALIDATION_FAILURE;
                        }
                    }

                    // Parse T2A descriptor
                    for (uint8_t descriptorIndex = 0; descriptorIndex < MAX_CDL_T2A_DESCRIPTOR; descriptorIndex++)
                    {
                        // get the descriptor json object
                        char descriptorKey[20];
                        snprintf_err_handle(descriptorKey, 20, "T2A Descriptor %" PRIu8 "",
                                            C_CAST(uint8_t, (descriptorIndex + 1)));
                        if (json_object_object_get_ex(rootObj, descriptorKey, &childObj) != 0)
                        {
                            struct json_object* descriptorObj = M_NULLPTR;

                            // Parse Time Field Unit Type
                            if (json_object_object_get_ex(childObj, "Time Field Unit", &descriptorObj) != 0)
                            {
                                const char* charValue = json_object_get_string(descriptorObj);
                                translate_String_To_TimeUnitType(
                                    charValue,
                                    &cdlSettings->scsiCDLSettings.cdlT2ADescriptor[descriptorIndex].timeFieldUnitType);
                            }

                            // Parse Inactive Time
                            if (json_object_object_get_ex(childObj, "Inactive Time", &descriptorObj) != 0)
                            {
                                cdlSettings->scsiCDLSettings.cdlT2ADescriptor[descriptorIndex].inactiveTime =
                                    C_CAST(uint32_t, json_object_get_uint64(descriptorObj));
                            }

                            // Parse Inactive Time Policy
                            if (json_object_object_get_ex(childObj, "Inactive Time Policy", &descriptorObj) != 0)
                            {
                                const char* charValue = json_object_get_string(descriptorObj);
                                if (!get_And_Validate_Integer_Input_Uint8(
                                        charValue, M_NULLPTR, ALLOW_UNIT_NONE,
                                        &cdlSettings->scsiCDLSettings.cdlT2ADescriptor[descriptorIndex]
                                             .inactiveTimePolicy))
                                {
                                    printf("Invalid Entry for \"Inactive Time Policy\" for T2A Descriptor %" PRIu8
                                           ".\n",
                                           (descriptorIndex + 1));
                                    ret = VALIDATION_FAILURE;
                                }
                            }

                            // Parse Active Time
                            if (json_object_object_get_ex(childObj, "Active Time", &descriptorObj) != 0)
                            {
                                cdlSettings->scsiCDLSettings.cdlT2ADescriptor[descriptorIndex].activeTime =
                                    C_CAST(uint32_t, json_object_get_uint64(descriptorObj));
                            }

                            // Parse Active Time Policy
                            if (json_object_object_get_ex(childObj, "Active Time Policy", &descriptorObj) != 0)
                            {
                                const char* charValue = json_object_get_string(descriptorObj);
                                if (!get_And_Validate_Integer_Input_Uint8(
                                        charValue, M_NULLPTR, ALLOW_UNIT_NONE,
                                        &cdlSettings->scsiCDLSettings.cdlT2ADescriptor[descriptorIndex]
                                             .activeTimePolicy))
                                {
                                    printf("Invalid Entry for \"Active Time Policy\" for T2A Descriptor %" PRIu8 ".\n",
                                           (descriptorIndex + 1));
                                    ret = VALIDATION_FAILURE;
                                }
                            }

                            // Parse Command Duration Guideline
                            if (json_object_object_get_ex(childObj, "Command Duration Guideline", &descriptorObj) != 0)
                            {
                                cdlSettings->scsiCDLSettings.cdlT2ADescriptor[descriptorIndex]
                                    .commandDurationGuideline = C_CAST(uint32_t, json_object_get_uint64(descriptorObj));
                            }

                            // Parse Command Duration Guideline Policy
                            if (json_object_object_get_ex(childObj, "Command Duration Guideline Policy",
                                                          &descriptorObj) != 0)
                            {
                                const char* charValue = json_object_get_string(descriptorObj);
                                if (!get_And_Validate_Integer_Input_Uint8(
                                        charValue, M_NULLPTR, ALLOW_UNIT_NONE,
                                        &cdlSettings->scsiCDLSettings.cdlT2ADescriptor[descriptorIndex]
                                             .CommandDurationGuidelinePolicy))
                                {
                                    printf("Invalid Entry for \"Command Duration Guideline\" for T2A Descriptor %" PRIu8
                                           ".\n",
                                           (descriptorIndex + 1));
                                    ret = VALIDATION_FAILURE;
                                }
                            }
                        }
                    }

                    // Parse T2B descriptor
                    for (uint8_t descriptorIndex = 0; descriptorIndex < MAX_CDL_T2B_DESCRIPTOR; descriptorIndex++)
                    {
                        // get the descriptor json object
                        char descriptorKey[20];
                        snprintf_err_handle(descriptorKey, 20, "T2B Descriptor %" PRIu8 "",
                                            C_CAST(uint8_t, (descriptorIndex + 1)));
                        if (json_object_object_get_ex(rootObj, descriptorKey, &childObj) != 0)
                        {
                            struct json_object* descriptorObj = M_NULLPTR;

                            // Parse Time Field Unit Type
                            if (json_object_object_get_ex(childObj, "Time Field Unit", &descriptorObj) != 0)
                            {
                                const char* charValue = json_object_get_string(descriptorObj);
                                translate_String_To_TimeUnitType(
                                    charValue,
                                    &cdlSettings->scsiCDLSettings.cdlT2BDescriptor[descriptorIndex].timeFieldUnitType);
                            }

                            // Parse Inactive Time
                            if (json_object_object_get_ex(childObj, "Inactive Time", &descriptorObj) != 0)
                            {
                                cdlSettings->scsiCDLSettings.cdlT2BDescriptor[descriptorIndex].inactiveTime =
                                    C_CAST(uint32_t, json_object_get_uint64(descriptorObj));
                            }

                            // Parse Inactive Time Policy
                            if (json_object_object_get_ex(childObj, "Inactive Time Policy", &descriptorObj) != 0)
                            {
                                const char* charValue = json_object_get_string(descriptorObj);
                                if (!get_And_Validate_Integer_Input_Uint8(
                                        charValue, M_NULLPTR, ALLOW_UNIT_NONE,
                                        &cdlSettings->scsiCDLSettings.cdlT2BDescriptor[descriptorIndex]
                                             .inactiveTimePolicy))
                                {
                                    printf("Invalid Entry for \"Inactive Time Policy\" for T2B Descriptor %" PRIu8
                                           ".\n",
                                           (descriptorIndex + 1));
                                    ret = VALIDATION_FAILURE;
                                }
                            }

                            // Parse Active Time
                            if (json_object_object_get_ex(childObj, "Active Time", &descriptorObj) != 0)
                            {
                                cdlSettings->scsiCDLSettings.cdlT2BDescriptor[descriptorIndex].activeTime =
                                    C_CAST(uint32_t, json_object_get_uint64(descriptorObj));
                            }

                            // Parse Active Time Policy
                            if (json_object_object_get_ex(childObj, "Active Time Policy", &descriptorObj) != 0)
                            {
                                const char* charValue = json_object_get_string(descriptorObj);
                                if (!get_And_Validate_Integer_Input_Uint8(
                                        charValue, M_NULLPTR, ALLOW_UNIT_NONE,
                                        &cdlSettings->scsiCDLSettings.cdlT2BDescriptor[descriptorIndex]
                                             .activeTimePolicy))
                                {
                                    printf("Invalid Entry for \"Active Time Policy\" for T2B Descriptor %" PRIu8 ".\n",
                                           (descriptorIndex + 1));
                                    ret = VALIDATION_FAILURE;
                                }
                            }

                            // Parse Command Duration Guideline
                            if (json_object_object_get_ex(childObj, "Command Duration Guideline", &descriptorObj) != 0)
                            {
                                cdlSettings->scsiCDLSettings.cdlT2BDescriptor[descriptorIndex]
                                    .commandDurationGuideline = C_CAST(uint32_t, json_object_get_uint64(descriptorObj));
                            }

                            // Parse Command Duration Guideline Policy
                            if (json_object_object_get_ex(childObj, "Command Duration Guideline Policy",
                                                          &descriptorObj) != 0)
                            {
                                const char* charValue = json_object_get_string(descriptorObj);
                                if (!get_And_Validate_Integer_Input_Uint8(
                                        charValue, M_NULLPTR, ALLOW_UNIT_NONE,
                                        &cdlSettings->scsiCDLSettings.cdlT2BDescriptor[descriptorIndex]
                                             .CommandDurationGuidelinePolicy))
                                {
                                    printf("Invalid Entry for \"Command Duration Guideline Policy\" for T2B Descriptor "
                                           "%" PRIu8 ".\n",
                                           (descriptorIndex + 1));
                                    ret = VALIDATION_FAILURE;
                                }
                            }
                        }
                    }

                    // Free the json object
                    json_object_put(rootObj);

                    // validate the parsed CDL settings for the accepted values according to spec
                    if (ret == SUCCESS && !skipValidation)
                    {
                        ret = is_Valid_Config_CDL_Settings(device, cdlSettings);
                        if (ret != SUCCESS)
                        {
                            // Invalid json file
                            ret = VALIDATION_FAILURE;
                        }
                    }
                    else if (skipValidation)
                    {
                        printf("Skipping validation for JSON file. The operation could fail, if values provided are "
                               "not according to SPEC.\n");
                    }
                }
                else
                {
                    // Invalid json file or parsing failure
                    printf("Failure in parsing JSON file!\n");
                    ret = VALIDATION_FAILURE;
                }
            }
            else
            {
                ret = FILE_OPEN_ERROR;
            }

            safe_free(&jsonMem);
        }
        else
        {
            ret = MEMORY_FAILURE;
        }

        // parse json file
        if (SEC_FILE_SUCCESS != secure_Close_File(cdlJsonfile))
        {
            printf("Error attempting to close file!\n");
        }
    }
    else
    {
        if (VERBOSITY_QUIET < device->deviceVerbosity)
        {
            printf("Couldn't open file %s\n", fileName);
        }

        ret = FILE_OPEN_ERROR;
    }

    free_Secure_File_Info(&cdlJsonfile);

    return ret;
}

eReturnValues parse_JSON_File_For_CDL_Settings(tDevice*      device,
                                               tCDLSettings* cdlSettings,
                                               const char*   fileName,
                                               bool          skipValidation)
{
    eReturnValues ret = SUCCESS;

    if (device->drive_info.drive_type == ATA_DRIVE)
    {
        ret = parse_ATA_JSON_File_For_CDL_Settings(device, cdlSettings, fileName, skipValidation);
    }
    else if (device->drive_info.drive_type == SCSI_DRIVE)
    {
        ret = parse_SCSI_JSON_File_For_CDL_Settings(device, cdlSettings, fileName, skipValidation);
    }

    return ret;
}