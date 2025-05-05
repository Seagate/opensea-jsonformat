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
#define MAX_ATTRIBUTE_NODE_NAME_LENGTH      21
#define MAX_FIELD_NODE_NAME_LENGTH          15
#define MAX_ANALYZED_FIELD_NODE_NAME_LENGTH 21
#define MAX_RAW_DATA_VALUE_IN_HEX_LENGTH    17
#define MAX_INT64_TO_DEC_STRING_LENGHT      21
#define MAX_DOUBLE_TO_DEC_STRING_LENGHT     21
#define MAX_UINT16_TO_HEX_STRING_LENGHT     7
#define MAX_UINT8_TO_HEX_STRING_LENGHT      5

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
    if (smartAnalyzedAttribute.rawData.userFieldCount > 0)
    {
        json_object* fieldArray = json_object_new_array();
        for (uint8_t fieldCount = 0; fieldCount < smartAnalyzedAttribute.rawData.userFieldCount; fieldCount++)
        {
            json_object* fieldNode = json_object_new_object();
            json_object_object_add(
                fieldNode, "name",
                json_object_new_string(smartAnalyzedAttribute.rawData.rawField[fieldCount].fieldName));
            char* unitString = M_REINTERPRET_CAST(char*, safe_calloc(MAX_RAW_FEILD_UNIT_STRING_LENGTH, sizeof(char)));
            get_Raw_Field_Unit_String(smartAnalyzedAttribute.rawData.rawField[fieldCount].fieldUnit, &unitString,
                                      false);
            DECLARE_ZERO_INIT_ARRAY(char, fieldValue,
                                    (MAX_INT64_TO_DEC_STRING_LENGHT +
                                     MAX_RAW_FEILD_UNIT_STRING_LENGTH)); // to be able to hold the unit string
            if (safe_strlen(unitString) > 0)
            {
                snprintf_err_handle(fieldValue, (MAX_INT64_TO_DEC_STRING_LENGHT + MAX_RAW_FEILD_UNIT_STRING_LENGTH),
                                    "%" PRId64 " %s", smartAnalyzedAttribute.rawData.rawField[fieldCount].fieldValue,
                                    unitString);
            }
            else
            {
                snprintf_err_handle(fieldValue, MAX_INT64_TO_DEC_STRING_LENGHT, "%" PRId64 "",
                                    smartAnalyzedAttribute.rawData.rawField[fieldCount].fieldValue);
            }
            json_object_object_add(fieldNode, "value", json_object_new_string(fieldValue));
            safe_free(&unitString);

            // create new node, name if Field#? and then add name-value pair in this node
            json_object* field = json_object_new_object();
            DECLARE_ZERO_INIT_ARRAY(char, fieldNodeName, MAX_FIELD_NODE_NAME_LENGTH);
            snprintf_err_handle(fieldNodeName, MAX_FIELD_NODE_NAME_LENGTH, "Field # %" PRIu8, fieldCount);
            json_object_object_add(field, fieldNodeName, fieldNode);

            // Add it into array
            json_object_array_add(fieldArray, field);
        }
        json_object_object_add(rawDataNode, "Fields", fieldArray);
    }
    if (smartAnalyzedAttribute.rawData.doubleTypeAnalyzedFieldValid ||
        smartAnalyzedAttribute.rawData.int64TypeAnalyzedFieldValid ||
        smartAnalyzedAttribute.rawData.stringTypeAnalyzedFieldValid)
    {
        json_object* analyzedArray = json_object_new_array();

        // create node for analyzed node
        uint8_t analyzedFieldCount = 0;

        // add analyzed value in double
        if (smartAnalyzedAttribute.rawData.doubleTypeAnalyzedFieldValid)
        {
            json_object* doubleTypeNode = json_object_new_object();
            json_object_object_add(doubleTypeNode, "name",
                                   json_object_new_string(smartAnalyzedAttribute.rawData.doubleTypeAnalyzedFieldName));

            char* unitString = M_REINTERPRET_CAST(char*, safe_calloc(MAX_RAW_FEILD_UNIT_STRING_LENGTH, sizeof(char)));
            get_Raw_Field_Unit_String(smartAnalyzedAttribute.rawData.doubleTypeAnalyzedFieldUnit, &unitString, false);
            if (safe_strlen(unitString) > 0)
            {
                DECLARE_ZERO_INIT_ARRAY(char, fieldValue,
                                        (MAX_DOUBLE_TO_DEC_STRING_LENGHT +
                                         MAX_RAW_FEILD_UNIT_STRING_LENGTH)); // to be able to hold the unit string
                snprintf_err_handle(fieldValue, (MAX_DOUBLE_TO_DEC_STRING_LENGHT + MAX_RAW_FEILD_UNIT_STRING_LENGTH),
                                    "%f %s", smartAnalyzedAttribute.rawData.doubleTypeAnalyzedFieldValue, unitString);
                json_object_object_add(doubleTypeNode, "value", json_object_new_string(fieldValue));
            }
            else
            {
                DECLARE_ZERO_INIT_ARRAY(char, fieldValue, MAX_DOUBLE_TO_DEC_STRING_LENGHT);
                snprintf_err_handle(fieldValue, MAX_DOUBLE_TO_DEC_STRING_LENGHT, "%f",
                                    smartAnalyzedAttribute.rawData.doubleTypeAnalyzedFieldValue);
                json_object_object_add(doubleTypeNode, "value", json_object_new_string(fieldValue));
            }
            safe_free(&unitString);

            // create new node, name if Analyzed Field#? and then add name-value pair in this node
            json_object* node = json_object_new_object();
            DECLARE_ZERO_INIT_ARRAY(char, fieldNodeName, MAX_ANALYZED_FIELD_NODE_NAME_LENGTH);
            snprintf_err_handle(fieldNodeName, MAX_ANALYZED_FIELD_NODE_NAME_LENGTH, "Analyzed Field # %" PRIu8,
                                (analyzedFieldCount + 1));
            json_object_object_add(node, fieldNodeName, doubleTypeNode);
            analyzedFieldCount++;

            // add this in array
            json_object_array_add(analyzedArray, node);
        }
        // add analyzed value in int64_t
        if (smartAnalyzedAttribute.rawData.int64TypeAnalyzedFieldValid)
        {
            json_object* int64TypeNode = json_object_new_object();
            json_object_object_add(int64TypeNode, "name",
                                   json_object_new_string(smartAnalyzedAttribute.rawData.int64TypeAnalyzedFieldName));

            char* unitString = M_REINTERPRET_CAST(char*, safe_calloc(MAX_RAW_FEILD_UNIT_STRING_LENGTH, sizeof(char)));
            get_Raw_Field_Unit_String(smartAnalyzedAttribute.rawData.int64TypeAnalyzedFieldUnit, &unitString, false);
            if (safe_strlen(unitString) > 0)
            {
                DECLARE_ZERO_INIT_ARRAY(char, fieldValue,
                                        (MAX_INT64_TO_DEC_STRING_LENGHT +
                                         MAX_RAW_FEILD_UNIT_STRING_LENGTH)); // to be able to hold the unit string
                snprintf_err_handle(fieldValue, (MAX_INT64_TO_DEC_STRING_LENGHT + MAX_RAW_FEILD_UNIT_STRING_LENGTH),
                                    "%" PRId64 " %s", smartAnalyzedAttribute.rawData.int64TypeAnalyzedFieldValue,
                                    unitString);
                json_object_object_add(int64TypeNode, "value", json_object_new_string(fieldValue));
            }
            else
            {
                DECLARE_ZERO_INIT_ARRAY(char, fieldValue, MAX_INT64_TO_DEC_STRING_LENGHT);
                snprintf_err_handle(fieldValue, MAX_INT64_TO_DEC_STRING_LENGHT, "%" PRId64 "",
                                    smartAnalyzedAttribute.rawData.int64TypeAnalyzedFieldValue);
                json_object_object_add(int64TypeNode, "value", json_object_new_string(fieldValue));
            }
            safe_free(&unitString);

            // create new node, name if Analyzed Field#? and then add name-value pair in this node
            json_object* node = json_object_new_object();
            DECLARE_ZERO_INIT_ARRAY(char, fieldNodeName, MAX_ANALYZED_FIELD_NODE_NAME_LENGTH);
            snprintf_err_handle(fieldNodeName, MAX_ANALYZED_FIELD_NODE_NAME_LENGTH, "Analyzed Field # %" PRIu8,
                                (analyzedFieldCount + 1));
            json_object_object_add(node, fieldNodeName, int64TypeNode);
            analyzedFieldCount++;

            // add this in array
            json_object_array_add(analyzedArray, node);
        }
        // add analyzed value in boolean
        if (smartAnalyzedAttribute.rawData.stringTypeAnalyzedFieldValid)
        {
            json_object* stringTypeNode = json_object_new_object();
            json_object_object_add(stringTypeNode, "name",
                                   json_object_new_string(smartAnalyzedAttribute.rawData.stringTypeAnalyzedFieldName));
            json_object_object_add(stringTypeNode, "value",
                                   json_object_new_string(smartAnalyzedAttribute.rawData.stringTypeAnalyzedFieldValue));

            // create new node, name if Analyzed Field#? and then add name-value pair in this node
            json_object* node = json_object_new_object();
            DECLARE_ZERO_INIT_ARRAY(char, fieldNodeName, MAX_ANALYZED_FIELD_NODE_NAME_LENGTH);
            snprintf_err_handle(fieldNodeName, MAX_ANALYZED_FIELD_NODE_NAME_LENGTH, "Analyzed Field # %" PRIu8,
                                (analyzedFieldCount + 1));
            json_object_object_add(node, fieldNodeName, stringTypeNode);
            analyzedFieldCount++;

            // add this in array
            json_object_array_add(analyzedArray, node);
        }

        json_object_object_add(rawDataNode, "Analyzed Fields", analyzedArray);
    }
    json_object_object_add(attributeNode, "Raw Data Information", rawDataNode);

    DECLARE_ZERO_INIT_ARRAY(char, attributeNodeName, MAX_ATTRIBUTE_NODE_NAME_LENGTH);
    snprintf_err_handle(attributeNodeName, MAX_ATTRIBUTE_NODE_NAME_LENGTH, "Attribute # %" PRIu8,
                        smartAnalyzedAttribute.attributeNumber);
    json_object_object_add(rootObject, attributeNodeName, attributeNode);
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