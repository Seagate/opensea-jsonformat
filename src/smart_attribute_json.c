//
// smart_attribute_json.c
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

// \file smart_attribute_json.c
// \brief This file defines types and functions related to the JSON-based output for SMART Attributes.

#include <json.h>
#include <json_object.h>

#include "io_utils.h"
#include "smart.h"
#include "smart_attribute_json.h"
#include "string_utils.h"

#define COMBINE_SMART_ATTRIBUTE_JSON_VERSIONS_(x, y, z) #x "." #y "." #z
#define COMBINE_SMART_ATTRIBUTE_JSON_VERSIONS(x, y, z)  COMBINE_SMART_ATTRIBUTE_JSON_VERSIONS_(x, y, z)

#define SMART_ATTRIBUTE_JSON_MAJOR_VERSION              1
#define SMART_ATTRIBUTE_JSON_MINOR_VERSION              0
#define SMART_ATTRIBUTE_JSON_PATCH_VERSION              0

#define SMART_ATTRIBUTE_JSON_VERSION                                                                                   \
    COMBINE_SMART_ATTRIBUTE_JSON_VERSIONS(SMART_ATTRIBUTE_JSON_MAJOR_VERSION, SMART_ATTRIBUTE_JSON_MINOR_VERSION,      \
                                          SMART_ATTRIBUTE_JSON_PATCH_VERSION)

static void create_Node_For_SMART_Attribute(json_object* rootObject, ataSMARTAnalyzedAttribute smartAnalyzedAttribute)
{
#define MAX_ATTRIBUTE_NODE_NAME_LENGTH   21
#define MAX_RAW_DATA_VALUE_IN_HEX_LENGTH 17
#define MAX_INT64_TO_DEC_STRING_LENGHT   21
#define MAX_DOUBLE_TO_DEC_STRING_LENGHT  21
#define MAX_UINT16_TO_HEX_STRING_LENGHT  7
#define MAX_UINT8_TO_HEX_STRING_LENGHT   5

    json_object* attributeNode = json_object_new_object();

    // add attribute name
    if (safe_strlen(smartAnalyzedAttribute.attributeName))
    {
        json_object_object_add(attributeNode, "Attribute Name",
                               json_object_new_string(smartAnalyzedAttribute.attributeName));
    }
    else
    {
        json_object_object_add(attributeNode, "Attribute Name", json_object_new_string("Unknown Attribute"));
    }

    // add atribute type
    json_object* statusNode = json_object_new_object();
    // add the hex value
    DECLARE_ZERO_INIT_ARRAY(char, statusValue, MAX_UINT16_TO_HEX_STRING_LENGHT);
    snprintf_err_handle(statusValue, MAX_UINT16_TO_HEX_STRING_LENGHT, "0x%04" PRIX16 "", smartAnalyzedAttribute.status);
    json_object_object_add(statusNode, "Status", json_object_new_string(statusValue));
    // now add each attribute type set for this
    json_object* jarray = json_object_new_array();
    if (smartAnalyzedAttribute.attributeType.preFailAttribute)
    {
        json_object_array_add(
            jarray, json_object_new_string("Pre-fail/warranty. Indicates a cause of known impending failure."));
    }
    if (smartAnalyzedAttribute.attributeType.onlineDataCollection)
    {
        json_object_array_add(jarray, json_object_new_string("Online Data Collection. Updates as the drive runs."));
    }
    if (smartAnalyzedAttribute.attributeType.performanceIndicator)
    {
        json_object_array_add(
            jarray, json_object_new_string("Performance. Degredation of this attribute will affect performance."));
    }
    if (smartAnalyzedAttribute.attributeType.errorRateIndicator)
    {
        json_object_array_add(jarray, json_object_new_string("Error Rate. Attribute tracks and error rate."));
    }
    if (smartAnalyzedAttribute.attributeType.eventCounter)
    {
        json_object_array_add(jarray, json_object_new_string("Event Count. Attribute is a counter."));
    }
    if (smartAnalyzedAttribute.attributeType.selfPreserving)
    {
        json_object_array_add(jarray, json_object_new_string("Self-Preserving. Saves between power cycles."));
    }
    json_object_object_add(statusNode, "Attribute Type(s)", jarray);
    json_object_object_add(attributeNode, "Status Information", statusNode);

    // add current(nominal) value, worst value
    json_object_object_add(attributeNode, "Current (Nominal) Value",
                           json_object_new_uint64(C_CAST(uint64_t, smartAnalyzedAttribute.nominal)));
    json_object_object_add(attributeNode, "Worst Ever Value",
                           json_object_new_uint64(C_CAST(uint64_t, smartAnalyzedAttribute.worstEver)));

    // add threshold information
    json_object* thresholdNode = json_object_new_object();
    switch (smartAnalyzedAttribute.thresholdInfo.thresholdType)
    {
    case THRESHOLD_ALWAYS_PASSING:
        json_object_object_add(thresholdNode, "Threshold Value", json_object_new_string("Set to always passing"));
        break;
    case THRESHOLD_ALWAYS_FAILING:
        json_object_object_add(thresholdNode, "Threshold Value", json_object_new_string("Set to always failing"));
        break;
    case THRESHOLD_INVALID:
        json_object_object_add(thresholdNode, "Threshold Value", json_object_new_string("Set to invalid value"));
        break;
    case THRESHOLD_SET:
    {
        DECLARE_ZERO_INIT_ARRAY(char, thresholdValue, MAX_UINT8_TO_HEX_STRING_LENGHT);
        snprintf_err_handle(thresholdValue, MAX_UINT8_TO_HEX_STRING_LENGHT, "0x%02" PRIX8 "",
                            smartAnalyzedAttribute.thresholdInfo.thresholdValue);
        json_object_object_add(thresholdNode, "Threshold Value", json_object_new_string(thresholdValue));
        if (smartAnalyzedAttribute.thresholdInfo.failStatus != FAIL_STATUS_NOT_SET)
        {
            json_object_object_add(thresholdNode, "Fail Status",
                                   json_object_new_string(smartAnalyzedAttribute.thresholdInfo.failStatusString));
        }
    }
    break;
    case THRESHOLD_UNKNOWN:
    default:
        // should never get here
        break;
    }
    json_object_object_add(attributeNode, "Threshold Information", thresholdNode);

    // add raw data fields
    json_object* rawDataNode = json_object_new_object();
    // add the raw data bytes 0:7 in hex
    DECLARE_ZERO_INIT_ARRAY(char, rawDataValue, MAX_RAW_DATA_VALUE_IN_HEX_LENGTH);
    snprintf_err_handle(rawDataValue, MAX_RAW_DATA_VALUE_IN_HEX_LENGTH,
                        "0x%02" PRIX8 "%02" PRIX8 "%02" PRIX8 "%02" PRIX8 "%02" PRIX8 "%02" PRIX8 "%02" PRIX8 "",
                        smartAnalyzedAttribute.rawData.rawData[6], smartAnalyzedAttribute.rawData.rawData[5],
                        smartAnalyzedAttribute.rawData.rawData[4], smartAnalyzedAttribute.rawData.rawData[3],
                        smartAnalyzedAttribute.rawData.rawData[2], smartAnalyzedAttribute.rawData.rawData[1],
                        smartAnalyzedAttribute.rawData.rawData[0]);
    json_object_object_add(rawDataNode, "Raw Data", json_object_new_string(rawDataValue));
    // add raw fields
    for (uint8_t field = 0; field < smartAnalyzedAttribute.rawData.userFieldCount; field++)
    {
        char* unitString = M_REINTERPRET_CAST(char*, safe_calloc(MAX_RAW_FEILD_UNIT_STRING_LENGTH, sizeof(char)));
        get_Raw_Field_Unit_String(smartAnalyzedAttribute.rawData.rawField[field].fieldUnit, &unitString);
        if (safe_strlen(unitString) > 0)
        {
            DECLARE_ZERO_INIT_ARRAY(char, fieldValue,
                                    (MAX_INT64_TO_DEC_STRING_LENGHT +
                                     MAX_RAW_FEILD_UNIT_STRING_LENGTH)); // to be able to hold the unit string
            snprintf_err_handle(fieldValue, (MAX_INT64_TO_DEC_STRING_LENGHT + MAX_RAW_FEILD_UNIT_STRING_LENGTH),
                                "%" PRId64 " %s", smartAnalyzedAttribute.rawData.rawField[field].fieldValue,
                                unitString);
            json_object_object_add(rawDataNode, smartAnalyzedAttribute.rawData.rawField[field].fieldName,
                                   json_object_new_string(fieldValue));
        }
        else
        {
            DECLARE_ZERO_INIT_ARRAY(char, fieldValue, MAX_INT64_TO_DEC_STRING_LENGHT);
            snprintf_err_handle(fieldValue, MAX_INT64_TO_DEC_STRING_LENGHT, "%" PRId64 "",
                                smartAnalyzedAttribute.rawData.rawField[field].fieldValue);
            json_object_object_add(rawDataNode, smartAnalyzedAttribute.rawData.rawField[field].fieldName,
                                   json_object_new_string(fieldValue));
        }
        safe_free(&unitString);
    }
    // add analyzed value in double
    if (smartAnalyzedAttribute.rawData.doubleAnalyzedValueValid)
    {
        char* unitString = M_REINTERPRET_CAST(char*, safe_calloc(MAX_RAW_FEILD_UNIT_STRING_LENGTH, sizeof(char)));
        get_Raw_Field_Unit_String(smartAnalyzedAttribute.rawData.doubleAnalyzedValueUnit, &unitString);
        if (safe_strlen(unitString) > 0)
        {
            DECLARE_ZERO_INIT_ARRAY(char, fieldValue,
                                    (MAX_DOUBLE_TO_DEC_STRING_LENGHT +
                                     MAX_RAW_FEILD_UNIT_STRING_LENGTH)); // to be able to hold the unit string
            snprintf_err_handle(fieldValue, (MAX_DOUBLE_TO_DEC_STRING_LENGHT + MAX_RAW_FEILD_UNIT_STRING_LENGTH),
                                "%f %s", smartAnalyzedAttribute.rawData.doubleAnalyzedValue, unitString);
            json_object_object_add(rawDataNode, smartAnalyzedAttribute.rawData.doubleAnalyzedString,
                                   json_object_new_string(fieldValue));
        }
        else
        {
            DECLARE_ZERO_INIT_ARRAY(char, fieldValue, MAX_DOUBLE_TO_DEC_STRING_LENGHT);
            snprintf_err_handle(fieldValue, MAX_DOUBLE_TO_DEC_STRING_LENGHT, "%f",
                                smartAnalyzedAttribute.rawData.doubleAnalyzedValue);
            json_object_object_add(rawDataNode, smartAnalyzedAttribute.rawData.doubleAnalyzedString,
                                   json_object_new_string(fieldValue));
        }
        safe_free(&unitString);
    }
    // add analyzed value in int64_t
    if (smartAnalyzedAttribute.rawData.int64AnalyzedValueValid)
    {
        char* unitString = M_REINTERPRET_CAST(char*, safe_calloc(MAX_RAW_FEILD_UNIT_STRING_LENGTH, sizeof(char)));
        get_Raw_Field_Unit_String(smartAnalyzedAttribute.rawData.int64AnalyzedValueUnit, &unitString);
        if (safe_strlen(unitString) > 0)
        {
            DECLARE_ZERO_INIT_ARRAY(char, fieldValue,
                                    (MAX_INT64_TO_DEC_STRING_LENGHT +
                                     MAX_RAW_FEILD_UNIT_STRING_LENGTH)); // to be able to hold the unit string
            snprintf_err_handle(fieldValue, (MAX_INT64_TO_DEC_STRING_LENGHT + MAX_RAW_FEILD_UNIT_STRING_LENGTH),
                                "%" PRId64 " %s", smartAnalyzedAttribute.rawData.int64AnalyzedValue, unitString);
            json_object_object_add(rawDataNode, smartAnalyzedAttribute.rawData.int64AnalyzedString,
                                   json_object_new_string(fieldValue));
        }
        else
        {
            DECLARE_ZERO_INIT_ARRAY(char, fieldValue, MAX_INT64_TO_DEC_STRING_LENGHT);
            snprintf_err_handle(fieldValue, MAX_INT64_TO_DEC_STRING_LENGHT, "%" PRId64 "",
                                smartAnalyzedAttribute.rawData.int64AnalyzedValue);
            json_object_object_add(rawDataNode, smartAnalyzedAttribute.rawData.int64AnalyzedString,
                                   json_object_new_string(fieldValue));
        }
        safe_free(&unitString);
    }
    // add analyzed value in boolean
    if (smartAnalyzedAttribute.rawData.stringAnalyzedValueValid)
    {
        json_object_object_add(rawDataNode, smartAnalyzedAttribute.rawData.stringAnalyzedString,
                               json_object_new_string(smartAnalyzedAttribute.rawData.stringAnalyzedValue));
    }
    json_object_object_add(attributeNode, "Raw Data Information", rawDataNode);

    DECLARE_ZERO_INIT_ARRAY(char, statisticName, MAX_ATTRIBUTE_NODE_NAME_LENGTH);
    snprintf_err_handle(statisticName, MAX_ATTRIBUTE_NODE_NAME_LENGTH, "Attribute # %" PRIu8,
                        smartAnalyzedAttribute.attributeNumber);
    json_object_object_add(rootObject, statisticName, attributeNode);
}

static eReturnValues create_JSON_Output_For_ATA(tDevice*              device,
                                                ataSMARTAnalyzedData* smartAnalyzedData,
                                                char**                jsonFormat)
{
    if (smartAnalyzedData == M_NULLPTR)
    {
        return BAD_PARAMETER;
    }

    json_object* rootNode = json_object_new_object();

    if (rootNode == M_NULLPTR)
        return MEMORY_FAILURE;

    // Add version information
    json_object_object_add(rootNode, "SMART Attribute Json Version",
                           json_object_new_string(SMART_ATTRIBUTE_JSON_VERSION));

    // Add general drive information
    json_object_object_add(rootNode, "Model Name", json_object_new_string(device->drive_info.product_identification));
    json_object_object_add(rootNode, "Serial Number", json_object_new_string(device->drive_info.serialNumber));
    json_object_object_add(rootNode, "Firmware Version", json_object_new_string(device->drive_info.product_revision));

    for (uint8_t iter = UINT8_C(0); iter < UINT8_MAX; ++iter)
    {
        if (smartAnalyzedData->attributes[iter].isValid)
        {
            create_Node_For_SMART_Attribute(rootNode, smartAnalyzedData->attributes[iter]);
        }
    }

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

eReturnValues create_JSON_Output_For_SMART_Attributes(tDevice* device, char** jsonFormat)
{
    eReturnValues ret = NOT_SUPPORTED;

    if (device->drive_info.drive_type == ATA_DRIVE)
    {
        ataSMARTAnalyzedData* smartAnalyzedData =
            M_REINTERPRET_CAST(ataSMARTAnalyzedData*, safe_calloc(1, sizeof(ataSMARTAnalyzedData)));
        ret = get_ATA_Analyzed_SMART_Attributes(device, smartAnalyzedData);
        if (ret == SUCCESS)
        {
            ret = create_JSON_Output_For_ATA(device, smartAnalyzedData, jsonFormat);
        }
        safe_free(&smartAnalyzedData);
    }

    return ret;
}