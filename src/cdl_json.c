//
// cdl_config_file.c
//
// Do NOT modify or remove this copyright and confidentiality notice.
//
// Copyright (c) 2012-2024 Seagate Technology LLC and/or its Affiliates, All Rights Reserved.
//
// The code contained herein is CONFIDENTIAL to Seagate Technology LLC
// and may be covered under one or more Non-Disclosure Agreements.
// All or portions are also trade secret.
// Any use, modification, duplication, derivation, distribution or disclosure
// of this code, for any reason, not expressly authorized is prohibited.
// All other rights are expressly reserved by Seagate Technology LLC.
//
// *****************************************************************************

// \file cdl_json.c
// \brief This file defines types and functions related to the new JSON-based Seagate CDL config file process.

#include <json_object.h>
#include <json.h>

#include "memory_safety.h"
#include "io_utils.h"
#include "string_utils.h"
#include "math_utils.h"
#include "secure_file.h"
#include "cdl_json.h"
#include "logs.h"

#define MAX_TIME_UNIT_STRING_LENGHT         4

static void translate_TimeUnitType_To_String(eCDLTimeFieldUnitType unitType, char *translatedString)
{
    switch (unitType)
    {
    case CDL_TIME_FIELD_UNIT_TYPE_SECONDS:
        snprintf(translatedString, MAX_TIME_UNIT_STRING_LENGHT, "s");
        break;
    case CDL_TIME_FIELD_UNIT_TYPE_MILLISECONDS:
        snprintf(translatedString, MAX_TIME_UNIT_STRING_LENGHT, "ms");
        break;
    case CDL_TIME_FIELD_UNIT_TYPE_MICROSECONDS:
    default:
        snprintf(translatedString, MAX_TIME_UNIT_STRING_LENGHT, "us");
        break;
    }
}

static void translate_String_To_TimeUnitType(const char *unitString, eCDLTimeFieldUnitType *unitType)
{
    if (strcmp(unitString, "s") == 0)
    {
        *unitType = CDL_TIME_FIELD_UNIT_TYPE_SECONDS;
    }
    else if (strcmp(unitString, "ms") == 0)
    {
        *unitType = CDL_TIME_FIELD_UNIT_TYPE_MILLISECONDS;
    }
    else
    {
        *unitType = CDL_TIME_FIELD_UNIT_TYPE_MICROSECONDS;
    }
}

eReturnValues create_JSON_File_For_CDL_Settings(tDevice *device, tCDLSettings *cdlSettings, const char* logPath)
{
    eReturnValues ret = SUCCESS;

    //Create a new JSON object
    json_object *rootObj = json_object_new_object();
    if (rootObj != M_NULLPTR)
    {
        //Add version information
        json_object_object_add(rootObj, "CDL Feature Version", json_object_new_string(CDL_FEATURE_VERSION));

        //Add Performance Versus Command Completion to the JSON object
        if (is_Performance_Versus_Command_Completion_Supported(cdlSettings))
        {
            DECLARE_ZERO_INIT_ARRAY(char, value, 5);
            snprintf(value, 5, "0x%02" PRIX8 "", cdlSettings->cdlSettings.ataCDLSettings.performanceVsCommandCompletion);
            json_object_object_add(rootObj, "Performance Versus Command Completion", json_object_new_string(value));
        }

        //Add JSON objects for read descriptor
        for (uint8_t descriptorIndex = 0; descriptorIndex < MAX_CDL_READ_DESCRIPTOR; descriptorIndex++)
        {
            json_object *jdescriptor = json_object_new_object();
            json_object_object_add(jdescriptor, "Inactive Time", json_object_new_uint64(cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].inactiveTime));
            DECLARE_ZERO_INIT_ARRAY(char, timeUnitValue, MAX_TIME_UNIT_STRING_LENGHT);
            translate_TimeUnitType_To_String(cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].inactiveTimeUnitType, timeUnitValue);
            json_object_object_add(jdescriptor, "Inactive Time Unit", json_object_new_string(timeUnitValue));
            DECLARE_ZERO_INIT_ARRAY(char, value, 5);
            snprintf(value, 5, "0x%02" PRIX8 "", cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].inactiveTimePolicy);
            json_object_object_add(jdescriptor, "Inactive Time Policy", json_object_new_string(value));
            json_object_object_add(jdescriptor, "Active Time", json_object_new_uint64(cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].activeTime));
            translate_TimeUnitType_To_String(cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].activeTimeUnitType, timeUnitValue);
            json_object_object_add(jdescriptor, "Active Time Unit", json_object_new_string(timeUnitValue));
            snprintf(value, 5, "0x%02" PRIX8 "", cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].activeTimePolicy);
            json_object_object_add(jdescriptor, "Active Time Policy", json_object_new_string(value));
            if (is_Total_Time_Policy_Type_Supported(cdlSettings))
            {
                json_object_object_add(jdescriptor, "Total Time", json_object_new_uint64(cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].totalTime));
                translate_TimeUnitType_To_String(cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].totalTimeUnitType, timeUnitValue);
                json_object_object_add(jdescriptor, "Total Time Unit", json_object_new_string(timeUnitValue));
                snprintf(value, 5, "0x%02" PRIX8 "", cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].totalTimePolicy);
                json_object_object_add(jdescriptor, "Total Time Policy", json_object_new_string(value));
            }

            //Add the read descriptor object to the main JSON object
            char descriptorKey[15];
            snprintf(descriptorKey, 15, "Descriptor R%" PRIu8 "", (descriptorIndex + 1));
            json_object_object_add(rootObj, descriptorKey, jdescriptor);
        }

        //Add JSON objects for write descriptor
        for (uint8_t descriptorIndex = 0; descriptorIndex < MAX_CDL_WRITE_DESCRIPTOR; descriptorIndex++)
        {
            json_object *jdescriptor = json_object_new_object();
            json_object_object_add(jdescriptor, "Inactive Time", json_object_new_uint64(cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].inactiveTime));
            DECLARE_ZERO_INIT_ARRAY(char, timeUnitValue, MAX_TIME_UNIT_STRING_LENGHT);
            translate_TimeUnitType_To_String(cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].inactiveTimeUnitType, timeUnitValue);
            json_object_object_add(jdescriptor, "Inactive Time Unit", json_object_new_string(timeUnitValue));
            DECLARE_ZERO_INIT_ARRAY(char, value, 5);
            snprintf(value, 5, "0x%02" PRIX8 "", cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].inactiveTimePolicy);
            json_object_object_add(jdescriptor, "Inactive Time Policy", json_object_new_string(value));
            json_object_object_add(jdescriptor, "Active Time", json_object_new_uint64(cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].activeTime));
            translate_TimeUnitType_To_String(cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].activeTimeUnitType, timeUnitValue);
            json_object_object_add(jdescriptor, "Active Time Unit", json_object_new_string(timeUnitValue));
            snprintf(value, 5, "0x%02" PRIX8 "", cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].activeTimePolicy);
            json_object_object_add(jdescriptor, "Active Time Policy", json_object_new_string(value));
            if (is_Total_Time_Policy_Type_Supported(cdlSettings))
            {
                json_object_object_add(jdescriptor, "Total Time", json_object_new_uint64(cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].totalTime));
                translate_TimeUnitType_To_String(cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].totalTimeUnitType, timeUnitValue);
                json_object_object_add(jdescriptor, "Total Time Unit", json_object_new_string(timeUnitValue));
                snprintf(value, 5, "0x%02" PRIX8 "", cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].totalTimePolicy);
                json_object_object_add(jdescriptor, "Total Time Policy", json_object_new_string(value));
            }

            //Add the write descriptor object to the main JSON object
            char descriptorKey[15];
            snprintf(descriptorKey, 15, "Descriptor W%" PRIu8 "", (descriptorIndex + 1));
            json_object_object_add(rootObj, descriptorKey, jdescriptor);
        }

        //Convert JSON object to formatted string
        const char *jstr = json_object_to_json_string_ext(rootObj, JSON_C_TO_STRING_PRETTY);

        //Write formatted JSON string to a file
        secureFileInfo* cdlJsonLog = M_NULLPTR;
        ret = create_And_Open_Secure_Log_File_Dev_EZ(device, &cdlJsonLog, NAMING_SERIAL_NUMBER_DATE_TIME, logPath, "CDL", "json");
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
                uint32_t currentCDLFeatureVersion = M_BytesTo4ByteValue(CDL_FEATURE_MAJOR_VERSION, CDL_FEATURE_MINOR_VERSION, CDL_FEATURE_PATCH_VERSION, 0);
                if (currentCDLFeatureVersion == 0x01000000)
                    printf("\tCommand Duration Guideline : %s\n", cdlSettings->cdlSettings.ataCDLSettings.isCommandDurationGuidelineSupported ? "Supported" : "Not Supported");
                printf("\tCommand Duration Limit Minimum Limit (us) : %" PRIu32 "\n", cdlSettings->cdlSettings.ataCDLSettings.minimumTimeLimit);
                printf("\tCommand Duration Limit Maximum Limit (us) : %" PRIu32 "\n", cdlSettings->cdlSettings.ataCDLSettings.maximumTimeLimit);
                DECLARE_ZERO_INIT_ARRAY(char, inactivePolicyString, SUPPORTED_POLICY_STRING_LENGTH);
                get_Supported_Policy_String(CDL_POLICY_TYPE_INACTIVE_TIME, cdlSettings->cdlSettings.ataCDLSettings.inactiveTimePolicySupportedDescriptor, inactivePolicyString);
                printf("\tSupported Inactive Time Policy : %s\n", inactivePolicyString);
                DECLARE_ZERO_INIT_ARRAY(char, activePolicyString, SUPPORTED_POLICY_STRING_LENGTH);
                get_Supported_Policy_String(CDL_POLICY_TYPE_ACTIVE_TIME, cdlSettings->cdlSettings.ataCDLSettings.activeTimePolicySupportedDescriptor, activePolicyString);
                printf("\tSupported Active Time Policy : %s\n", activePolicyString);
                if (is_Total_Time_Policy_Type_Supported(cdlSettings))
                {
                    DECLARE_ZERO_INIT_ARRAY(char, totalPolicyString, SUPPORTED_POLICY_STRING_LENGTH);
                    get_Supported_Policy_String(CDL_POLICY_TYPE_TOTAL_TIME, cdlSettings->cdlSettings.ataCDLSettings.totalTimePolicySupportedDescriptor, totalPolicyString);
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

        //Free the JSON object
        json_object_put(rootObj);
    }

    return ret;
}

eReturnValues parse_JSON_File_For_CDL_Settings(tDevice *device, tCDLSettings *cdlSettings, const char* fileName, bool skipValidation)
{
    eReturnValues ret = SUCCESS;

    secureFileInfo* cdlJsonfile = secure_Open_File(fileName, "r", M_NULLPTR, M_NULLPTR, M_NULLPTR);
    if (cdlJsonfile && cdlJsonfile->error == SEC_FILE_SUCCESS)
    {
        char* jsonMem = C_CAST(char*, safe_calloc(cdlJsonfile->fileSize, sizeof(uint8_t)));
        if (jsonMem)
        {
            if (SEC_FILE_SUCCESS == secure_Read_File(cdlJsonfile, jsonMem, cdlJsonfile->fileSize, sizeof(char), cdlJsonfile->fileSize, M_NULLPTR))
            {
                json_object* rootObj = json_tokener_parse(jsonMem);
                if (rootObj != M_NULLPTR)
                {
                    //Parse CDL Feature version
                    struct json_object* childObj = M_NULLPTR;
                    if (json_object_object_get_ex(rootObj, "CDL Feature Version", &childObj) != 0)
                    {
                        const char *charValue = json_object_get_string(childObj);
                        if(strncmp(charValue, CDL_FEATURE_VERSION, M_Min(safe_strlen(charValue), safe_strlen(CDL_FEATURE_VERSION))) != 0)
                        {
                            printf("\"CDL Feature Version\" for provided JSON file doesn't match with current version. Please generate the JSON file with current version of tool.\n");
                            ret = VALIDATION_FAILURE;
                        }
                    }

                    //Parse performance vs command completion
                    if (json_object_object_get_ex(rootObj, "Performance Versus Command Completion", &childObj) != 0)
                    {
                        const char *charValue = json_object_get_string(childObj);
                        if (!get_And_Validate_Integer_Input_Uint8(charValue, M_NULLPTR, ALLOW_UNIT_NONE, &cdlSettings->cdlSettings.ataCDLSettings.performanceVsCommandCompletion))
                        {
                            printf("Invalid entry for \"Performance Versus Command Completion\"\n");
                            ret = VALIDATION_FAILURE;
                        }
                    }

                    //Parse read descriptor
                    for (uint8_t descriptorIndex = 0; descriptorIndex < MAX_CDL_READ_DESCRIPTOR; descriptorIndex++)
                    {
                        //get the descriptor json object
                        char descriptorKey[15];
                        snprintf(descriptorKey, 15, "Descriptor R%" PRIu8 "", (descriptorIndex + 1));
                        if (json_object_object_get_ex(rootObj, descriptorKey, &childObj) != 0)
                        {
                            struct json_object* descriptorObj = M_NULLPTR;

                            //Parse Inactive Time Unit Type
                            if (json_object_object_get_ex(childObj, "Inactive Time Unit", &descriptorObj) != 0)
                            {
                                const char *charValue = json_object_get_string(descriptorObj);
                                translate_String_To_TimeUnitType(charValue, &cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].inactiveTimeUnitType);
                            }

                            //Parse Inactive Time
                            if (json_object_object_get_ex(childObj, "Inactive Time", &descriptorObj) != 0)
                            {
                                uint32_t value = C_CAST(uint32_t, json_object_get_uint64(descriptorObj));
                                if (cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].inactiveTimeUnitType == CDL_TIME_FIELD_UNIT_TYPE_SECONDS)
                                    value *= 1000000;
                                else if (cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].inactiveTimeUnitType == CDL_TIME_FIELD_UNIT_TYPE_MILLISECONDS)
                                    value *= 1000;
                                cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].inactiveTime = value;
                            }

                            //Parse Inactive Time Policy
                            if (json_object_object_get_ex(childObj, "Inactive Time Policy", &descriptorObj) != 0)
                            {
                                const char *charValue = json_object_get_string(descriptorObj);
                                if (!get_And_Validate_Integer_Input_Uint8(charValue, M_NULLPTR, ALLOW_UNIT_NONE, &cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].inactiveTimePolicy))
                                {
                                    printf("Invalid Entry for \"Inactive Time Policy\" for Descriptor R%" PRIu8 ".\n", (descriptorIndex + 1));
                                    ret = VALIDATION_FAILURE;
                                }
                            }

                            //Parse Active Time Unit Type
                            if (json_object_object_get_ex(childObj, "Active Time Unit", &descriptorObj) != 0)
                            {
                                const char *charValue = json_object_get_string(descriptorObj);
                                translate_String_To_TimeUnitType(charValue, &cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].activeTimeUnitType);
                            }

                            //Parse Active Time
                            if (json_object_object_get_ex(childObj, "Active Time", &descriptorObj) != 0)
                            {
                                uint32_t value = C_CAST(uint32_t, json_object_get_uint64(descriptorObj));
                                if (cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].activeTimeUnitType == CDL_TIME_FIELD_UNIT_TYPE_SECONDS)
                                    value *= 1000000;
                                else if (cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].activeTimeUnitType == CDL_TIME_FIELD_UNIT_TYPE_MILLISECONDS)
                                    value *= 1000;
                                cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].activeTime = value;
                            }

                            //Parse Active Time Policy
                            if (json_object_object_get_ex(childObj, "Active Time Policy", &descriptorObj) != 0)
                            {
                                const char *charValue = json_object_get_string(descriptorObj);
                                if (!get_And_Validate_Integer_Input_Uint8(charValue, M_NULLPTR, ALLOW_UNIT_NONE, &cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].activeTimePolicy))
                                {
                                    printf("Invalid Entry for \"Active Time Policy\" for Descriptor R%" PRIu8 ".\n", (descriptorIndex + 1));
                                    ret = VALIDATION_FAILURE;
                                }
                            }

                            //Parse Total Time Unit Type
                            if (json_object_object_get_ex(childObj, "Total Time Unit", &descriptorObj) != 0)
                            {
                                const char *charValue = json_object_get_string(descriptorObj);
                                translate_String_To_TimeUnitType(charValue, &cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].totalTimeUnitType);
                            }

                            //Parse Total Time
                            if (json_object_object_get_ex(childObj, "Total Time", &descriptorObj) != 0)
                            {
                                uint32_t value = C_CAST(uint32_t, json_object_get_uint64(descriptorObj));
                                if (cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].totalTimeUnitType == CDL_TIME_FIELD_UNIT_TYPE_SECONDS)
                                    value *= 1000000;
                                else if (cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].totalTimeUnitType == CDL_TIME_FIELD_UNIT_TYPE_MILLISECONDS)
                                    value *= 1000;
                                cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].totalTime = value;
                            }

                            //Parse Total Time Policy
                            if (json_object_object_get_ex(childObj, "Total Time Policy", &descriptorObj) != 0)
                            {
                                const char *charValue = json_object_get_string(descriptorObj);
                                if (!get_And_Validate_Integer_Input_Uint8(charValue, M_NULLPTR, ALLOW_UNIT_NONE, &cdlSettings->cdlSettings.ataCDLSettings.cdlReadDescriptor[descriptorIndex].totalTimePolicy))
                                {
                                    printf("Invalid Entry for \"Total Time Policy\" for Descriptor R%" PRIu8 ".\n", (descriptorIndex + 1));
                                    ret = VALIDATION_FAILURE;
                                }
                            }
                        }
                    }

                    //Parse write descriptor
                    for (uint8_t descriptorIndex = 0; descriptorIndex < MAX_CDL_WRITE_DESCRIPTOR; descriptorIndex++)
                    {
                        //get the descriptor json object
                        char descriptorKey[15];
                        snprintf(descriptorKey, 15, "Descriptor W%" PRIu8 "", (descriptorIndex + 1));
                        if (json_object_object_get_ex(rootObj, descriptorKey, &childObj) != 0)
                        {
                            struct json_object* descriptorObj = M_NULLPTR;

                            //Parse Inactive Time Unit Type
                            if (json_object_object_get_ex(childObj, "Inactive Time Unit", &descriptorObj) != 0)
                            {
                                const char *charValue = json_object_get_string(descriptorObj);
                                translate_String_To_TimeUnitType(charValue, &cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].inactiveTimeUnitType);
                            }

                            //Parse Inactive Time                                        
                            if (json_object_object_get_ex(childObj, "Inactive Time", &descriptorObj) != 0)
                            {
                                uint32_t value = C_CAST(uint32_t, json_object_get_uint64(descriptorObj));
                                if (cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].inactiveTimeUnitType == CDL_TIME_FIELD_UNIT_TYPE_SECONDS)
                                    value *= 1000000;
                                else if (cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].inactiveTimeUnitType == CDL_TIME_FIELD_UNIT_TYPE_MILLISECONDS)
                                    value *= 1000;
                                cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].inactiveTime = value;
                            }

                            //Parse Inactive Time Policy
                            if (json_object_object_get_ex(childObj, "Inactive Time Policy", &descriptorObj) != 0)
                            {
                                const char *charValue = json_object_get_string(descriptorObj);
                                if (!get_And_Validate_Integer_Input_Uint8(charValue, M_NULLPTR, ALLOW_UNIT_NONE, &cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].inactiveTimePolicy))
                                {
                                    printf("Invalid Entry for \"Inactive Time Policy\" for Descriptor W%" PRIu8 ".\n", (descriptorIndex + 1));
                                    ret = VALIDATION_FAILURE;
                                }
                            }

                            //Parse Active Time Unit Type
                            if (json_object_object_get_ex(childObj, "Active Time Unit", &descriptorObj) != 0)
                            {
                                const char *charValue = json_object_get_string(descriptorObj);
                                translate_String_To_TimeUnitType(charValue, &cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].activeTimeUnitType);
                            }

                            //Parse Active Time
                            if (json_object_object_get_ex(childObj, "Active Time", &descriptorObj) != 0)
                            {
                                uint32_t value = C_CAST(uint32_t, json_object_get_uint64(descriptorObj));
                                if (cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].activeTimeUnitType == CDL_TIME_FIELD_UNIT_TYPE_SECONDS)
                                    value *= 1000000;
                                else if (cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].activeTimeUnitType == CDL_TIME_FIELD_UNIT_TYPE_MILLISECONDS)
                                    value *= 1000;
                                cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].activeTime = value;
                            }

                            //Parse Active Time Policy
                            if (json_object_object_get_ex(childObj, "Active Time Policy", &descriptorObj) != 0)
                            {
                                const char *charValue = json_object_get_string(descriptorObj);
                                if (!get_And_Validate_Integer_Input_Uint8(charValue, M_NULLPTR, ALLOW_UNIT_NONE, &cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].activeTimePolicy))
                                {
                                    printf("Invalid Entry for \"Active Time Policy\" for Descriptor W%" PRIu8 ".\n", (descriptorIndex + 1));
                                    ret = VALIDATION_FAILURE;
                                }
                            }

                            //Parse Total Time Unit Type
                            if (json_object_object_get_ex(childObj, "Total Time Unit", &descriptorObj) != 0)
                            {
                                const char *charValue = json_object_get_string(descriptorObj);
                                translate_String_To_TimeUnitType(charValue, &cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].totalTimeUnitType);
                            }

                            //Parse Total Time
                            if (json_object_object_get_ex(childObj, "Total Time", &descriptorObj) != 0)
                            {
                                uint32_t value = C_CAST(uint32_t, json_object_get_uint64(descriptorObj));
                                if (cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].totalTimeUnitType == CDL_TIME_FIELD_UNIT_TYPE_SECONDS)
                                    value *= 1000000;
                                else if (cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].totalTimeUnitType == CDL_TIME_FIELD_UNIT_TYPE_MILLISECONDS)
                                    value *= 1000;
                                cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].totalTime = value;
                            }

                            //Parse Total Time Policy
                            if (json_object_object_get_ex(childObj, "Total Time Policy", &descriptorObj) != 0)
                            {
                                const char *charValue = json_object_get_string(descriptorObj);
                                if (!get_And_Validate_Integer_Input_Uint8(charValue, M_NULLPTR, ALLOW_UNIT_NONE, &cdlSettings->cdlSettings.ataCDLSettings.cdlWriteDescriptor[descriptorIndex].totalTimePolicy))
                                {
                                    printf("Invalid Entry for \"Total Time Policy\" for Descriptor W%" PRIu8 ".\n", (descriptorIndex + 1));
                                    ret = VALIDATION_FAILURE;
                                }
                            }
                        }
                    }

                    //Free the json object
                    json_object_put(rootObj);

                    //validate the parsed CDL settings for the accepted values according to spec
                    if (ret == SUCCESS && !skipValidation)
                    {
                        ret = is_Valid_Config_CDL_Settings(device, cdlSettings);
                        if (ret != SUCCESS)
                        {
                            //Invalid json file                                    
                            ret = VALIDATION_FAILURE;
                        }
                    }
                    else if (skipValidation)
                    {
                        printf("Skipping validation for JSON file. The operation could fail, if values provided are not according to SPEC.\n");
                    }
                }
                else
                {
                    //Invalid json file or parsing failure
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

        //parse json file
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