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
// \file smart_attribute_json.c
// \brief This file defines types and functions related to the JSON-based output for SMART Attributes.

#include "smart_attribute_json.h"
#include "io_utils.h"
#include "smart.h"
#include "string_utils.h"

#define COMBINE_SMART_ATTRIBUTE_JSON_VERSIONS_(x, y, z) #x "." #y "." #z
#define COMBINE_SMART_ATTRIBUTE_JSON_VERSIONS(x, y, z)  COMBINE_SMART_ATTRIBUTE_JSON_VERSIONS_(x, y, z)

#define SMART_ATTRIBUTE_JSON_MAJOR_VERSION              1
#define SMART_ATTRIBUTE_JSON_MINOR_VERSION              0
#define SMART_ATTRIBUTE_JSON_PATCH_VERSION              0

#define SMART_ATTRIBUTE_JSON_VERSION                                                                                   \
    COMBINE_SMART_ATTRIBUTE_JSON_VERSIONS(SMART_ATTRIBUTE_JSON_MAJOR_VERSION, SMART_ATTRIBUTE_JSON_MINOR_VERSION,      \
                                          SMART_ATTRIBUTE_JSON_PATCH_VERSION)

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

#define ADD_SMART_JSON_OBJECT(parent, key, child)                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        if (add_JSON_Object((parent), (key), (child)) != 0)                                                            \
        {                                                                                                              \
            json_object_put(attributeNode);                                                                            \
            return MEMORY_FAILURE;                                                                                     \
        }                                                                                                              \
    } while (0)

#define ADD_SMART_JSON_ARRAY_ELEMENT(parent, child)                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        if (add_JSON_Array_Element((parent), (child)) != 0)                                                            \
        {                                                                                                              \
            json_object_put(attributeNode);                                                                            \
            return MEMORY_FAILURE;                                                                                     \
        }                                                                                                              \
    } while (0)

M_NODISCARD static eReturnValues create_Node_For_SMART_Attribute(json_object*              rootObject,
                                                                 ataSMARTAnalyzedAttribute smartAnalyzedAttribute)
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
    if (attributeNode == M_NULLPTR)
    {
        return MEMORY_FAILURE;
    }

    // add attribute name
    if (safe_strlen(smartAnalyzedAttribute.attributeName))
    {
        ADD_SMART_JSON_OBJECT(attributeNode, "Attribute Name",
                              json_object_new_string(smartAnalyzedAttribute.attributeName));
    }
    else
    {
        ADD_SMART_JSON_OBJECT(attributeNode, "Attribute Name", json_object_new_string("Unknown Attribute"));
    }

    // add atribute type
    json_object* statusNode = json_object_new_object();
    if (statusNode == M_NULLPTR)
    {
        json_object_put(attributeNode);
        return MEMORY_FAILURE;
    }
    // add the hex value
    DECLARE_ZERO_INIT_ARRAY(char, statusValue, MAX_UINT16_TO_HEX_STRING_LENGHT);
    M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(statusValue, MAX_UINT16_TO_HEX_STRING_LENGHT, "0x%04" PRIX16 "",
                                               smartAnalyzedAttribute.status),
                           "SMART JSON destination buffer is sized for the fixed display format");
    ADD_SMART_JSON_OBJECT(statusNode, "Flags", json_object_new_string(statusValue));
    // now add each attribute type set for this
    json_object* attributeTypeNode = json_object_new_object();
    if (attributeTypeNode == M_NULLPTR)
    {
        json_object_put(statusNode);
        json_object_put(attributeNode);
        return MEMORY_FAILURE;
    }
    ADD_SMART_JSON_OBJECT(attributeTypeNode, "Pre-fail",
                          smartAnalyzedAttribute.attributeType.preFailAttribute ? json_object_new_string("Yes")
                                                                                : json_object_new_string("No"));
    ADD_SMART_JSON_OBJECT(attributeTypeNode, "Online Data Collection",
                          smartAnalyzedAttribute.attributeType.onlineDataCollection ? json_object_new_string("Yes")
                                                                                    : json_object_new_string("No"));
    ADD_SMART_JSON_OBJECT(attributeTypeNode, "Performance degrades as current value decreases",
                          smartAnalyzedAttribute.attributeType.performanceIndicator ? json_object_new_string("Yes")
                                                                                    : json_object_new_string("No"));
    ADD_SMART_JSON_OBJECT(attributeTypeNode, "Error Rate",
                          smartAnalyzedAttribute.attributeType.errorRateIndicator ? json_object_new_string("Yes")
                                                                                  : json_object_new_string("No"));
    ADD_SMART_JSON_OBJECT(attributeTypeNode, "Event Count",
                          smartAnalyzedAttribute.attributeType.eventCounter ? json_object_new_string("Yes")
                                                                            : json_object_new_string("No"));
    ADD_SMART_JSON_OBJECT(attributeTypeNode, "Self-Preserving",
                          smartAnalyzedAttribute.attributeType.selfPreserving ? json_object_new_string("Yes")
                                                                              : json_object_new_string("No"));
    ADD_SMART_JSON_OBJECT(statusNode, "Flags Types", attributeTypeNode);
    ADD_SMART_JSON_OBJECT(attributeNode, "Flags Information", statusNode);

    // add current value, worst value
    ADD_SMART_JSON_OBJECT(attributeNode, "Current Value",
                          json_object_new_uint64(C_CAST(uint64_t, smartAnalyzedAttribute.nominal)));
    ADD_SMART_JSON_OBJECT(attributeNode, "Worst Ever Value",
                          json_object_new_uint64(C_CAST(uint64_t, smartAnalyzedAttribute.worstEver)));

    // add threshold information
    json_object* thresholdNode = json_object_new_object();
    if (thresholdNode == M_NULLPTR)
    {
        json_object_put(attributeNode);
        return MEMORY_FAILURE;
    }
    ADD_SMART_JSON_OBJECT(attributeNode, "Threshold Information", thresholdNode);
    switch (smartAnalyzedAttribute.thresholdInfo.thresholdType)
    {
    case THRESHOLD_ALWAYS_PASSING:
        ADD_SMART_JSON_OBJECT(thresholdNode, "Threshold Value", json_object_new_string("Set to always passing"));
        break;
    case THRESHOLD_ALWAYS_FAILING:
        ADD_SMART_JSON_OBJECT(thresholdNode, "Threshold Value", json_object_new_string("Set to always failing"));
        break;
    case THRESHOLD_INVALID:
        ADD_SMART_JSON_OBJECT(thresholdNode, "Threshold Value", json_object_new_string("Set to invalid value"));
        break;
    case THRESHOLD_SET:
    {
        DECLARE_ZERO_INIT_ARRAY(char, thresholdValue, MAX_UINT8_TO_HEX_STRING_LENGHT);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(thresholdValue, MAX_UINT8_TO_HEX_STRING_LENGHT, "0x%02" PRIX8 "",
                                                   smartAnalyzedAttribute.thresholdInfo.thresholdValue),
                               "SMART JSON destination buffer is sized for the fixed display format");
        ADD_SMART_JSON_OBJECT(thresholdNode, "Threshold Value", json_object_new_string(thresholdValue));
    }
    break;
    case THRESHOLD_UNKNOWN:
    default:
        // should never get here
        break;
    }
    ADD_SMART_JSON_OBJECT(thresholdNode, "Current Failure Status",
                          json_object_new_string(smartAnalyzedAttribute.thresholdInfo.currentFailStatusString));
    ADD_SMART_JSON_OBJECT(thresholdNode, "Past Failure Status",
                          json_object_new_string(smartAnalyzedAttribute.thresholdInfo.pastFailStatusString));

    // add raw data fields
    json_object* rawDataNode = json_object_new_object();
    if (rawDataNode == M_NULLPTR)
    {
        json_object_put(attributeNode);
        return MEMORY_FAILURE;
    }
    ADD_SMART_JSON_OBJECT(attributeNode, "Raw Data Information", rawDataNode);
    // add the raw data bytes 0:7 in hex
    DECLARE_ZERO_INIT_ARRAY(char, rawDataValue, MAX_RAW_DATA_VALUE_IN_HEX_LENGTH);
    M_IGNORE_SAFE_INT_CALL(
        snprintf_err_handle(rawDataValue, MAX_RAW_DATA_VALUE_IN_HEX_LENGTH,
                            "0x%02" PRIX8 "%02" PRIX8 "%02" PRIX8 "%02" PRIX8 "%02" PRIX8 "%02" PRIX8 "%02" PRIX8 "",
                            smartAnalyzedAttribute.rawData.rawData[6], smartAnalyzedAttribute.rawData.rawData[5],
                            smartAnalyzedAttribute.rawData.rawData[4], smartAnalyzedAttribute.rawData.rawData[3],
                            smartAnalyzedAttribute.rawData.rawData[2], smartAnalyzedAttribute.rawData.rawData[1],
                            smartAnalyzedAttribute.rawData.rawData[0]),
        "SMART JSON destination buffer is sized for the fixed display format");
    ADD_SMART_JSON_OBJECT(rawDataNode, "Raw Data", json_object_new_string(rawDataValue));
    // add raw fields
    if (smartAnalyzedAttribute.rawData.userFieldCount > 0)
    {
        json_object* fieldArray = json_object_new_array();
        if (fieldArray == M_NULLPTR)
        {
            json_object_put(attributeNode);
            return MEMORY_FAILURE;
        }
        for (uint8_t fieldCount = 0; fieldCount < smartAnalyzedAttribute.rawData.userFieldCount; fieldCount++)
        {
            json_object* fieldNode = json_object_new_object();
            if (fieldNode == M_NULLPTR)
            {
                json_object_put(fieldArray);
                json_object_put(attributeNode);
                return MEMORY_FAILURE;
            }
            if (add_JSON_Object(
                    fieldNode, "name",
                    json_object_new_string(smartAnalyzedAttribute.rawData.rawField[fieldCount].fieldName)) != 0)
            {
                json_object_put(fieldNode);
                json_object_put(fieldArray);
                json_object_put(attributeNode);
                return MEMORY_FAILURE;
            }
            char* unitString = M_REINTERPRET_CAST(char*, safe_calloc(MAX_RAW_FEILD_UNIT_STRING_LENGTH, sizeof(char)));
            if (unitString != M_NULLPTR)
            {
                get_Raw_Field_Unit_String(smartAnalyzedAttribute.rawData.rawField[fieldCount].fieldUnit, &unitString,
                                          false);
            }
            DECLARE_ZERO_INIT_ARRAY(char, fieldValue,
                                    (MAX_INT64_TO_DEC_STRING_LENGHT +
                                     MAX_RAW_FEILD_UNIT_STRING_LENGTH)); // to be able to hold the unit string
            if (unitString != M_NULLPTR && safe_strlen(unitString) > 0)
            {
                M_IGNORE_SAFE_INT_CALL(
                    snprintf_err_handle(fieldValue, (MAX_INT64_TO_DEC_STRING_LENGHT + MAX_RAW_FEILD_UNIT_STRING_LENGTH),
                                        "%" PRId64 " %s",
                                        smartAnalyzedAttribute.rawData.rawField[fieldCount].fieldValue, unitString),
                    "SMART JSON destination buffer is sized for the fixed display format");
            }
            else
            {
                M_IGNORE_SAFE_INT_CALL(
                    snprintf_err_handle(fieldValue, MAX_INT64_TO_DEC_STRING_LENGHT, "%" PRId64 "",
                                        smartAnalyzedAttribute.rawData.rawField[fieldCount].fieldValue),
                    "SMART JSON destination buffer is sized for the fixed display format");
            }
            if (add_JSON_Object(fieldNode, "value", json_object_new_string(fieldValue)) != 0)
            {
                safe_free(&unitString);
                json_object_put(fieldNode);
                json_object_put(fieldArray);
                json_object_put(attributeNode);
                return MEMORY_FAILURE;
            }
            safe_free(&unitString);

            // create new node, name if Field ? and then add name-value pair in this node
            json_object* field = json_object_new_object();
            if (field == M_NULLPTR)
            {
                json_object_put(fieldNode);
                json_object_put(fieldArray);
                json_object_put(attributeNode);
                return MEMORY_FAILURE;
            }
            DECLARE_ZERO_INIT_ARRAY(char, fieldNodeName, MAX_FIELD_NODE_NAME_LENGTH);
            M_IGNORE_SAFE_INT_CALL(
                snprintf_err_handle(fieldNodeName, MAX_FIELD_NODE_NAME_LENGTH, "Field %" PRIu8, (fieldCount + 1)),
                "SMART JSON destination buffer is sized for the fixed display format");
            if (add_JSON_Object(field, fieldNodeName, fieldNode) != 0)
            {
                json_object_put(field);
                json_object_put(fieldArray);
                json_object_put(attributeNode);
                return MEMORY_FAILURE;
            }

            // Add it into array
            if (add_JSON_Array_Element(fieldArray, field) != 0)
            {
                json_object_put(fieldArray);
                json_object_put(attributeNode);
                return MEMORY_FAILURE;
            }
        }
        if (add_JSON_Object(rawDataNode, "Fields", fieldArray) != 0)
        {
            json_object_put(attributeNode);
            return MEMORY_FAILURE;
        }
    }
    if (smartAnalyzedAttribute.rawData.doubleTypeAnalyzedFieldValid ||
        smartAnalyzedAttribute.rawData.int64TypeAnalyzedFieldValid ||
        smartAnalyzedAttribute.rawData.stringTypeAnalyzedFieldValid)
    {
        json_object* analyzedArray = json_object_new_array();
        if (analyzedArray == M_NULLPTR)
        {
            json_object_put(attributeNode);
            return MEMORY_FAILURE;
        }

        // create node for analyzed node
        uint8_t analyzedFieldCount = 0;

        // add analyzed value in double
        if (smartAnalyzedAttribute.rawData.doubleTypeAnalyzedFieldValid)
        {
            json_object* doubleTypeNode = json_object_new_object();
            if (doubleTypeNode == M_NULLPTR)
            {
                json_object_put(analyzedArray);
                json_object_put(attributeNode);
                return MEMORY_FAILURE;
            }
            if (add_JSON_Object(doubleTypeNode, "name",
                                json_object_new_string(smartAnalyzedAttribute.rawData.doubleTypeAnalyzedFieldName)) !=
                0)
            {
                json_object_put(doubleTypeNode);
                json_object_put(analyzedArray);
                json_object_put(attributeNode);
                return MEMORY_FAILURE;
            }

            char* unitString = M_REINTERPRET_CAST(char*, safe_calloc(MAX_RAW_FEILD_UNIT_STRING_LENGTH, sizeof(char)));
            if (unitString != M_NULLPTR)
            {
                get_Raw_Field_Unit_String(smartAnalyzedAttribute.rawData.doubleTypeAnalyzedFieldUnit, &unitString,
                                          false);
            }
            if (unitString != M_NULLPTR && safe_strlen(unitString) > 0)
            {
                DECLARE_ZERO_INIT_ARRAY(char, fieldValue,
                                        (MAX_DOUBLE_TO_DEC_STRING_LENGHT +
                                         MAX_RAW_FEILD_UNIT_STRING_LENGTH)); // to be able to hold the unit string
                int formatResult = snprintf_err_handle(
                    fieldValue, (MAX_DOUBLE_TO_DEC_STRING_LENGHT + MAX_RAW_FEILD_UNIT_STRING_LENGTH), "%f %s",
                    smartAnalyzedAttribute.rawData.doubleTypeAnalyzedFieldValue, unitString);
                if (add_JSON_Object(
                        doubleTypeNode, "value",
                        json_object_new_string(formatResult >= 0 && C_CAST(size_t, formatResult) < sizeof(fieldValue)
                                                   ? fieldValue
                                                   : "Invalid")) != 0)
                {
                    safe_free(&unitString);
                    json_object_put(doubleTypeNode);
                    json_object_put(analyzedArray);
                    json_object_put(attributeNode);
                    return MEMORY_FAILURE;
                }
            }
            else
            {
                DECLARE_ZERO_INIT_ARRAY(char, fieldValue, MAX_DOUBLE_TO_DEC_STRING_LENGHT);
                int formatResult = snprintf_err_handle(fieldValue, MAX_DOUBLE_TO_DEC_STRING_LENGHT, "%f",
                                                       smartAnalyzedAttribute.rawData.doubleTypeAnalyzedFieldValue);
                if (add_JSON_Object(
                        doubleTypeNode, "value",
                        json_object_new_string(formatResult >= 0 && C_CAST(size_t, formatResult) < sizeof(fieldValue)
                                                   ? fieldValue
                                                   : "Invalid")) != 0)
                {
                    safe_free(&unitString);
                    json_object_put(doubleTypeNode);
                    json_object_put(analyzedArray);
                    json_object_put(attributeNode);
                    return MEMORY_FAILURE;
                }
            }
            safe_free(&unitString);

            // create new node, name if Analyzed Field ? and then add name-value pair in this node
            json_object* node = json_object_new_object();
            if (node == M_NULLPTR)
            {
                json_object_put(doubleTypeNode);
                json_object_put(analyzedArray);
                json_object_put(attributeNode);
                return MEMORY_FAILURE;
            }
            DECLARE_ZERO_INIT_ARRAY(char, fieldNodeName, MAX_ANALYZED_FIELD_NODE_NAME_LENGTH);
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(fieldNodeName, MAX_ANALYZED_FIELD_NODE_NAME_LENGTH,
                                                       "Analyzed Field %" PRIu8, (analyzedFieldCount + 1)),
                                   "SMART JSON destination buffer is sized for the fixed display format");
            if (add_JSON_Object(node, fieldNodeName, doubleTypeNode) != 0)
            {
                json_object_put(node);
                json_object_put(analyzedArray);
                json_object_put(attributeNode);
                return MEMORY_FAILURE;
            }
            analyzedFieldCount++;

            // add this in array
            if (add_JSON_Array_Element(analyzedArray, node) != 0)
            {
                json_object_put(analyzedArray);
                json_object_put(attributeNode);
                return MEMORY_FAILURE;
            }
        }
        // add analyzed value in int64_t
        if (smartAnalyzedAttribute.rawData.int64TypeAnalyzedFieldValid)
        {
            json_object* int64TypeNode = json_object_new_object();
            if (int64TypeNode == M_NULLPTR)
            {
                json_object_put(analyzedArray);
                json_object_put(attributeNode);
                return MEMORY_FAILURE;
            }
            if (add_JSON_Object(int64TypeNode, "name",
                                json_object_new_string(smartAnalyzedAttribute.rawData.int64TypeAnalyzedFieldName)) != 0)
            {
                json_object_put(int64TypeNode);
                json_object_put(analyzedArray);
                json_object_put(attributeNode);
                return MEMORY_FAILURE;
            }

            char* unitString = M_REINTERPRET_CAST(char*, safe_calloc(MAX_RAW_FEILD_UNIT_STRING_LENGTH, sizeof(char)));
            if (unitString != M_NULLPTR)
            {
                get_Raw_Field_Unit_String(smartAnalyzedAttribute.rawData.int64TypeAnalyzedFieldUnit, &unitString,
                                          false);
            }
            if (unitString != M_NULLPTR && safe_strlen(unitString) > 0)
            {
                DECLARE_ZERO_INIT_ARRAY(char, fieldValue,
                                        (MAX_INT64_TO_DEC_STRING_LENGHT +
                                         MAX_RAW_FEILD_UNIT_STRING_LENGTH)); // to be able to hold the unit string
                M_IGNORE_SAFE_INT_CALL(
                    snprintf_err_handle(fieldValue, (MAX_INT64_TO_DEC_STRING_LENGHT + MAX_RAW_FEILD_UNIT_STRING_LENGTH),
                                        "%" PRId64 " %s", smartAnalyzedAttribute.rawData.int64TypeAnalyzedFieldValue,
                                        unitString),
                    "maximum int64_t value and unit string fit in the fixed SMART JSON field buffer");
                if (add_JSON_Object(int64TypeNode, "value", json_object_new_string(fieldValue)) != 0)
                {
                    safe_free(&unitString);
                    json_object_put(int64TypeNode);
                    json_object_put(analyzedArray);
                    json_object_put(attributeNode);
                    return MEMORY_FAILURE;
                }
            }
            else
            {
                DECLARE_ZERO_INIT_ARRAY(char, fieldValue, MAX_INT64_TO_DEC_STRING_LENGHT);
                M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(fieldValue, MAX_INT64_TO_DEC_STRING_LENGHT, "%" PRId64 "",
                                                           smartAnalyzedAttribute.rawData.int64TypeAnalyzedFieldValue),
                                       "maximum int64_t decimal text fits in the fixed SMART JSON field buffer");
                if (add_JSON_Object(int64TypeNode, "value", json_object_new_string(fieldValue)) != 0)
                {
                    safe_free(&unitString);
                    json_object_put(int64TypeNode);
                    json_object_put(analyzedArray);
                    json_object_put(attributeNode);
                    return MEMORY_FAILURE;
                }
            }
            safe_free(&unitString);

            // create new node, name if Analyzed Field ? and then add name-value pair in this node
            json_object* node = json_object_new_object();
            if (node == M_NULLPTR)
            {
                json_object_put(int64TypeNode);
                json_object_put(analyzedArray);
                json_object_put(attributeNode);
                return MEMORY_FAILURE;
            }
            DECLARE_ZERO_INIT_ARRAY(char, fieldNodeName, MAX_ANALYZED_FIELD_NODE_NAME_LENGTH);
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(fieldNodeName, MAX_ANALYZED_FIELD_NODE_NAME_LENGTH,
                                                       "Analyzed Field %" PRIu8, (analyzedFieldCount + 1)),
                                   "SMART JSON destination buffer is sized for the fixed display format");
            if (add_JSON_Object(node, fieldNodeName, int64TypeNode) != 0)
            {
                json_object_put(node);
                json_object_put(analyzedArray);
                json_object_put(attributeNode);
                return MEMORY_FAILURE;
            }
            analyzedFieldCount++;

            // add this in array
            if (add_JSON_Array_Element(analyzedArray, node) != 0)
            {
                json_object_put(analyzedArray);
                json_object_put(attributeNode);
                return MEMORY_FAILURE;
            }
        }
        // add analyzed value in boolean
        if (smartAnalyzedAttribute.rawData.stringTypeAnalyzedFieldValid)
        {
            json_object* stringTypeNode = json_object_new_object();
            if (stringTypeNode == M_NULLPTR)
            {
                json_object_put(analyzedArray);
                json_object_put(attributeNode);
                return MEMORY_FAILURE;
            }
            if (add_JSON_Object(stringTypeNode, "name",
                                json_object_new_string(smartAnalyzedAttribute.rawData.stringTypeAnalyzedFieldName)) !=
                    0 ||
                add_JSON_Object(stringTypeNode, "value",
                                json_object_new_string(smartAnalyzedAttribute.rawData.stringTypeAnalyzedFieldValue)) !=
                    0)
            {
                json_object_put(stringTypeNode);
                json_object_put(analyzedArray);
                json_object_put(attributeNode);
                return MEMORY_FAILURE;
            }

            // create new node, name if Analyzed Field ? and then add name-value pair in this node
            json_object* node = json_object_new_object();
            if (node == M_NULLPTR)
            {
                json_object_put(stringTypeNode);
                json_object_put(analyzedArray);
                json_object_put(attributeNode);
                return MEMORY_FAILURE;
            }
            DECLARE_ZERO_INIT_ARRAY(char, fieldNodeName, MAX_ANALYZED_FIELD_NODE_NAME_LENGTH);
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(fieldNodeName, MAX_ANALYZED_FIELD_NODE_NAME_LENGTH,
                                                       "Analyzed Field %" PRIu8, (analyzedFieldCount + 1)),
                                   "SMART JSON destination buffer is sized for the fixed display format");
            if (add_JSON_Object(node, fieldNodeName, stringTypeNode) != 0)
            {
                json_object_put(node);
                json_object_put(analyzedArray);
                json_object_put(attributeNode);
                return MEMORY_FAILURE;
            }
            analyzedFieldCount++;

            // add this in array
            if (add_JSON_Array_Element(analyzedArray, node) != 0)
            {
                json_object_put(analyzedArray);
                json_object_put(attributeNode);
                return MEMORY_FAILURE;
            }
        }

        if (add_JSON_Object(rawDataNode, "Analyzed Fields", analyzedArray) != 0)
        {
            json_object_put(attributeNode);
            return MEMORY_FAILURE;
        }
    }

    DECLARE_ZERO_INIT_ARRAY(char, attributeNodeName, MAX_ATTRIBUTE_NODE_NAME_LENGTH);
    M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(attributeNodeName, MAX_ATTRIBUTE_NODE_NAME_LENGTH, "Attribute %" PRIu8,
                                               smartAnalyzedAttribute.attributeNumber),
                           "SMART JSON destination buffer is sized for the fixed display format");
    if (add_JSON_Object(rootObject, attributeNodeName, attributeNode) != 0)
    {
        return MEMORY_FAILURE;
    }
    return SUCCESS;
}

M_NODISCARD static eReturnValues create_JSON_Output_For_ATA(const tDevice*        device,
                                                            ataSMARTAnalyzedData* smartAnalyzedData,
                                                            const char*           utilityName,
                                                            const char*           buildVersion,
                                                            char**                jsonFormat)
{
    if (smartAnalyzedData == M_NULLPTR)
    {
        return BAD_PARAMETER;
    }

    json_object* rootNode = json_object_new_object();

    if (rootNode == M_NULLPTR)
    {
        return MEMORY_FAILURE;
    }

    if (create_Node_For_Utility_Version(rootNode, utilityName, buildVersion, "SMART Attribute",
                                        SMART_ATTRIBUTE_JSON_VERSION) != SUCCESS ||
        create_Node_For_Drive_Information(rootNode, device) != SUCCESS)
    {
        json_object_put(rootNode);
        return MEMORY_FAILURE;
    }

    for (uint8_t iter = UINT8_C(0); iter < UINT8_MAX; ++iter)
    {
        if (smartAnalyzedData->attributes[iter].isValid)
        {
            eReturnValues ret = create_Node_For_SMART_Attribute(rootNode, smartAnalyzedData->attributes[iter]);
            if (ret != SUCCESS)
            {
                json_object_put(rootNode);
                return ret;
            }
        }
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

OPENSEA_JSONFORMAT_API eReturnValues create_JSON_Output_For_SMART_Attributes(const tDevice* M_NONNULL device,
                                                                             const char* M_NONNULL    utilityName,
                                                                             const char* M_NONNULL    buildVersion,
                                                                             char**                   jsonFormat)
{
    eReturnValues ret = NOT_SUPPORTED;

    if (device == M_NULLPTR || jsonFormat == M_NULLPTR)
    {
        return BAD_PARAMETER;
    }

    *jsonFormat = M_NULLPTR;

    if (get_Device_DriveType(device) == ATA_DRIVE)
    {
        ataSMARTAnalyzedData* smartAnalyzedData =
            M_REINTERPRET_CAST(ataSMARTAnalyzedData*, safe_calloc(1, sizeof(ataSMARTAnalyzedData)));
        if (smartAnalyzedData == M_NULLPTR)
        {
            ret = MEMORY_FAILURE;
        }
        else
        {
            ret = get_ATA_Analyzed_SMART_Attributes(device, smartAnalyzedData);
            if (ret == SUCCESS)
            {
                ret = create_JSON_Output_For_ATA(device, smartAnalyzedData, utilityName, buildVersion, jsonFormat);
            }
        }
        safe_free_ata_smart_analyzed_data(&smartAnalyzedData);
    }

    return ret;
}
