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
// \file farm_json.c
// \brief This file defines types and functions related to the JSON-based output for FARM.

#include <json.h>
#include <json_object.h>

#include "farm_json.h"
#include "io_utils.h"
#include "math_utils.h"
#include "string_utils.h"

#define COMBINE_FARM_JSON_VERSIONS_(x, y, z) #x "." #y "." #z
#define COMBINE_FARM_JSON_VERSIONS(x, y, z)  COMBINE_FARM_JSON_VERSIONS_(x, y, z)

#define FARM_JSON_MAJOR_VERSION              1
#define FARM_JSON_MINOR_VERSION              0
#define FARM_JSON_PATCH_VERSION              0

#define FARM_JSON_VERSION                                                                                              \
    COMBINE_FARM_JSON_VERSIONS(FARM_JSON_MAJOR_VERSION, FARM_JSON_MINOR_VERSION, FARM_JSON_PATCH_VERSION)

#define MAX_UINT64_TO_HEX_STRING_LENGHT  19
#define MAX_DOA_STRING_LENGHT            14
#define MAX_UINT64_TO_DEC_STRING_LENGHT  21
#define MAX_RECORDING_TYPE_STRING_LENGHT 9
#define MAX_DOUBLE_TO_DEC_STRING_LENGHT  21
#define MAX_BOOL_TO_BOOL_STRING_LENGHT   21
#define MAX_INT64_TO_DEC_STRING_LENGHT   21
#define MAX_DEC_TO_DOUBLE_STRING_LENGHT  21
#define MAX_HEAD_NODE_NAME_LENGTH        9
#define MAX_UINT8_TO_DEC_STRING_LENGHT   4
#define MAX_UPDATED_NODE_NAME_LENGHT     41
#define MAX_ACTUATOR_NODE_NAME_LENGTH    11
#define MAX_FLED_EVENT_NODE_NAME_LENGTH  21

static void create_Node_For_String_Type_From_Multiple_QWords(json_object* node,
                                                             const char*  nodeName,
                                                             uint64_t*    valueQword,
                                                             uint8_t      numberOfQword)
{
    if (valueQword != M_NULLPTR)
    {
        size_t asciilen      = (uint8_to_sizet(numberOfQword) * SIZE_T_C(4)) + SIZE_T_C(1);
        char*  farmASCIIData = M_REINTERPRET_CAST(char*, safe_calloc(asciilen, sizeof(char)));
        if (farmASCIIData != M_NULLPTR)
        {
            size_t asciioffset = SIZE_T_C(0);
            for (uint8_t qwordIter = UINT8_C(0); qwordIter < numberOfQword; ++qwordIter, asciioffset += 4)
            {
                uint32_t rawdata = w_swap_32(b_swap_32(M_DoubleWord0(valueQword[qwordIter])));
                safe_memcpy(&farmASCIIData[asciioffset], asciilen, &rawdata, sizeof(uint32_t));
            }
            farmASCIIData[asciilen] = 0;
            remove_Leading_And_Trailing_Whitespace(farmASCIIData);
            json_object_object_add(node, nodeName, json_object_new_string(farmASCIIData));
            safe_free(&farmASCIIData);
        }
    }
}

static void create_Node_For_UINT64_Hex_From_2_QWords(json_object* node, const char* nodeName, uint64_t* valueQword)
{
    if (valueQword != M_NULLPTR)
    {
        uint64_t finalData =
            M_DWordsTo8ByteValue(w_swap_32(M_DoubleWord0(valueQword[0])), w_swap_32(M_DoubleWord0(valueQword[1])));
        DECLARE_ZERO_INIT_ARRAY(char, finalASCIIValue, MAX_UINT64_TO_HEX_STRING_LENGHT);
        snprintf_err_handle(finalASCIIValue, MAX_UINT64_TO_HEX_STRING_LENGHT, "0x%016" PRIX64 "", finalData);
        json_object_object_add(node, nodeName, json_object_new_string(finalASCIIValue));
    }
}

static void create_Node_For_DOA_From_QWord(json_object* node, const char* nodeName, uint64_t valueQword)
{
    uint32_t rawdata       = M_DoubleWord0(valueQword);
    size_t   asciilen      = (SIZE_T_C(5));
    char*    farmASCIIData = M_REINTERPRET_CAST(char*, safe_calloc(asciilen, sizeof(char)));
    if (farmASCIIData != M_NULLPTR)
    {
        safe_memcpy(&farmASCIIData[0], asciilen, &rawdata, sizeof(uint32_t));
        DECLARE_ZERO_INIT_ARRAY(char, doaYYStr, 5);
        doaYYStr[0] = '2';
        doaYYStr[1] = '0';
        safe_memcpy(&doaYYStr[2], 3, farmASCIIData, 2);
        DECLARE_ZERO_INIT_ARRAY(char, doaWWStr, 3);
        safe_memcpy(&doaWWStr[0], 3, &farmASCIIData[2], 2);

        DECLARE_ZERO_INIT_ARRAY(char, finalDOAValue, MAX_DOA_STRING_LENGHT);
        snprintf_err_handle(finalDOAValue, MAX_DOA_STRING_LENGHT, "Week %s, %s", doaWWStr, doaYYStr);
        json_object_object_add(node, nodeName, json_object_new_string(finalDOAValue));
    }
}

static void create_Node_For_UINT64_From_QWord(json_object* node, const char* nodeName, uint64_t valueQword)
{
    uint8_t status = get_Farm_Status_Byte(valueQword);
    if ((status & FARM_FIELD_SUPPORTED_BIT) > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, value, MAX_UINT64_TO_DEC_STRING_LENGHT);
        if ((status & FARM_FIELD_VALID_BIT) > 0)
            snprintf_err_handle(value, MAX_UINT64_TO_DEC_STRING_LENGHT, "%" PRIu64 "", get_Farm_Qword_Data(valueQword));
        else
            snprintf_err_handle(value, MAX_UINT64_TO_DEC_STRING_LENGHT, "Invalid");
        json_object_object_add(node, nodeName, json_object_new_string(value));
    }
}

static void create_Node_For_Recording_Type_From_QWord(json_object* node, const char* nodeName, uint64_t valueQword)
{
    uint8_t status = get_Farm_Status_Byte(valueQword);
    if ((status & FARM_FIELD_SUPPORTED_BIT) > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, value, MAX_RECORDING_TYPE_STRING_LENGHT);
        if ((status & FARM_FIELD_VALID_BIT) > 0)
        {
            uint64_t recordingType = get_Farm_Qword_Data(valueQword);
            if ((recordingType & FARM_DRIVE_RECORDING_SMR) && (recordingType & FARM_DRIVE_RECORDING_CMR))
                snprintf_err_handle(value, MAX_UINT64_TO_DEC_STRING_LENGHT, "SMR, CMR");
            else if (recordingType & FARM_DRIVE_RECORDING_SMR)
                snprintf_err_handle(value, MAX_UINT64_TO_DEC_STRING_LENGHT, "SMR");
            else if (recordingType & FARM_DRIVE_RECORDING_CMR)
                snprintf_err_handle(value, MAX_UINT64_TO_DEC_STRING_LENGHT, "CMR");
            else
                snprintf_err_handle(value, MAX_UINT64_TO_DEC_STRING_LENGHT, "Invalid");
        }
        else
            snprintf_err_handle(value, MAX_UINT64_TO_DEC_STRING_LENGHT, "Invalid");
        json_object_object_add(node, nodeName, json_object_new_string(value));
    }
}

static void create_Node_For_UINT64_Hex_From_QWord(json_object* node, const char* nodeName, uint64_t valueQword)
{
    uint8_t status = get_Farm_Status_Byte(valueQword);
    if ((status & FARM_FIELD_SUPPORTED_BIT) > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, value, MAX_UINT64_TO_HEX_STRING_LENGHT);
        if ((status & FARM_FIELD_VALID_BIT) > 0)
            snprintf_err_handle(value, MAX_UINT64_TO_HEX_STRING_LENGHT, "%" PRIX64 "", get_Farm_Qword_Data(valueQword));
        else
            snprintf_err_handle(value, MAX_UINT64_TO_HEX_STRING_LENGHT, "Invalid");
        json_object_object_add(node, nodeName, json_object_new_string(value));
    }
}

#define MICRO_SECONDS_PER_HOUR          3600000000.0
#define MICRO_SECONDS_PER_MINUTE        60000000.0
#define MICRO_SECONDS_PER_SECOND        1000000.0
#define MICRO_SECONDS_PER_MILLI_SECONDS 1000.0

static void create_Node_For_Time_From_QWord(json_object* node,
                                            const char*  nodeName,
                                            uint64_t     valueQword,
                                            double       conversionToMicroseconds)
{
    uint8_t status = get_Farm_Status_Byte(valueQword);
    if ((status & FARM_FIELD_SUPPORTED_BIT) > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, value, MAX_DOUBLE_TO_DEC_STRING_LENGHT);
        if ((status & FARM_FIELD_VALID_BIT) > 0)
        {
            double timeMicroseconds = M_STATIC_CAST(double, get_Farm_Qword_Data(valueQword)) * conversionToMicroseconds;
            snprintf_err_handle(value, MAX_DOUBLE_TO_DEC_STRING_LENGHT, "%0.02f",
                                (timeMicroseconds / MICRO_SECONDS_PER_HOUR));
        }
        else
            snprintf_err_handle(value, MAX_DOUBLE_TO_DEC_STRING_LENGHT, "Invalid");
        json_object_object_add(node, nodeName, json_object_new_string(value));
    }
}

static void create_Node_For_Bool_From_QWord(json_object* node,
                                            const char*  nodeName,
                                            uint64_t     valueQword,
                                            const char*  trueString,
                                            const char*  falseString)
{
    uint8_t status = get_Farm_Status_Byte(valueQword);
    if ((status & FARM_FIELD_SUPPORTED_BIT) > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, value, MAX_BOOL_TO_BOOL_STRING_LENGHT);
        if ((status & FARM_FIELD_VALID_BIT) > 0)
        {
            if (get_Farm_Qword_Data(valueQword) > 0)
            {
                if (trueString != M_NULLPTR)
                    snprintf_err_handle(value, MAX_BOOL_TO_BOOL_STRING_LENGHT, "%s", trueString);
                else
                    snprintf_err_handle(value, MAX_BOOL_TO_BOOL_STRING_LENGHT, "true");
            }
            else
            {
                if (falseString != M_NULLPTR)
                    snprintf_err_handle(value, MAX_BOOL_TO_BOOL_STRING_LENGHT, "%s", falseString);
                else
                    snprintf_err_handle(value, MAX_BOOL_TO_BOOL_STRING_LENGHT, "false");
            }
        }
        else
            snprintf_err_handle(value, MAX_BOOL_TO_BOOL_STRING_LENGHT, "Invalid");
        json_object_object_add(node, nodeName, json_object_new_string(value));
    }
}

static void create_Node_For_INT64_From_QWord(json_object* node, const char* nodeName, uint64_t valueQword)
{
    uint8_t status = get_Farm_Status_Byte(valueQword);
    if ((status & FARM_FIELD_SUPPORTED_BIT) > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, value, MAX_INT64_TO_DEC_STRING_LENGHT);
        if ((status & FARM_FIELD_VALID_BIT) > 0)
        {
            int64_t signedval = M_STATIC_CAST(int64_t, get_Farm_Qword_Data(valueQword));
            if (M_Byte6(M_STATIC_CAST(uint64_t, signedval)) & BIT7)
            {
                // sign bit is set. To make sure this converts as we expect it to we need to make sure the int64_t sign
                // bit of the host is set properly.
                signedval = M_STATIC_CAST(int64_t, M_STATIC_CAST(uint64_t, signedval) | UINT64_C(0xFFFF000000000000));
            }
            snprintf_err_handle(value, MAX_INT64_TO_DEC_STRING_LENGHT, "%" PRId64 "", signedval);
        }
        else
            snprintf_err_handle(value, MAX_INT64_TO_DEC_STRING_LENGHT, "Invalid");
        json_object_object_add(node, nodeName, json_object_new_string(value));
    }
}

static void create_Node_For_Float_With_Factor_From_QWord(json_object* node,
                                                         const char*  nodeName,
                                                         uint64_t     valueQword,
                                                         double       conversionFactor,
                                                         bool         isSignedValue)
{
    uint8_t status = get_Farm_Status_Byte(valueQword);
    if ((status & FARM_FIELD_SUPPORTED_BIT) > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, value, MAX_DEC_TO_DOUBLE_STRING_LENGHT);
        if ((status & FARM_FIELD_VALID_BIT) > 0)
        {
            int precision = 2;
            if (conversionFactor <= 0.001)
            {
                precision = 3;
            }
            if (conversionFactor <= 0.0001)
            {
                precision = 4;
            }
            if (conversionFactor <= 0.00001)
            {
                precision = 5;
            }

            if (isSignedValue)
            {
                int64_t signedval = M_STATIC_CAST(int64_t, get_Farm_Qword_Data(valueQword));
                if (M_Byte6(M_STATIC_CAST(uint64_t, signedval)) & BIT7)
                {
                    // sign bit is set. To make sure this converts as we expect it to we need to make sure the int64_t
                    // sign bit of the host is set properly.
                    signedval =
                        M_STATIC_CAST(int64_t, M_STATIC_CAST(uint64_t, signedval) | UINT64_C(0xFFFF000000000000));
                }
                snprintf_err_handle(value, MAX_DEC_TO_DOUBLE_STRING_LENGHT, "%0.*f", precision,
                                    M_STATIC_CAST(double, signedval) * conversionFactor);
            }
            else
                snprintf_err_handle(value, MAX_DEC_TO_DOUBLE_STRING_LENGHT, "%0.*f", precision,
                                    M_STATIC_CAST(double, get_Farm_Qword_Data(valueQword)) * conversionFactor);
        }
        else
            snprintf_err_handle(value, MAX_INT64_TO_DEC_STRING_LENGHT, "Invalid");
        json_object_object_add(node, nodeName, json_object_new_string(value));
    }
}

#define FARM_FLOAT_PERCENT_DELTA_FACTORY_BIT BIT0
#define FARM_FLOAT_NEGATIVE_BIT              BIT1

static M_INLINE uint8_t get_Farm_Float_Bits(uint64_t floatData)
{
    return M_Byte6(floatData);
}

static void create_Node_For_Float_From_QWord(json_object* node, const char* nodeName, uint64_t valueQword)
{
    uint8_t status = get_Farm_Status_Byte(valueQword);
    if ((status & FARM_FIELD_SUPPORTED_BIT) > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, value, MAX_DEC_TO_DOUBLE_STRING_LENGHT);
        if ((status & FARM_FIELD_VALID_BIT) > 0)
        {
            uint8_t bits = get_Farm_Float_Bits(valueQword);
            if (bits & FARM_FLOAT_PERCENT_DELTA_FACTORY_BIT || bits & FARM_FLOAT_NEGATIVE_BIT)
            {
                double  calculated  = 0.0;
                double  decimalPart = M_STATIC_CAST(double, get_DWord0(valueQword));
                int16_t wholePart   = M_STATIC_CAST(int16_t, M_Word2(valueQword));

                calculated = M_STATIC_CAST(double, wholePart) + (decimalPart * 0.0001);
                if (bits & FARM_FLOAT_NEGATIVE_BIT)
                {
                    calculated *= -1.0;
                }
                snprintf_err_handle(value, MAX_DEC_TO_DOUBLE_STRING_LENGHT, "%0.02f", calculated);
            }
            else
            {
                snprintf_err_handle(value, MAX_DEC_TO_DOUBLE_STRING_LENGHT, "%" PRIu16 "", M_Word0(valueQword));
            }
        }
        else
            snprintf_err_handle(value, MAX_DEC_TO_DOUBLE_STRING_LENGHT, "Invalid");
        json_object_object_add(node, nodeName, json_object_new_string(value));
    }
}

typedef enum eFARMByHeadOutputFormat
{
    FARM_BY_HEAD_TO_UINT64_FROM_QWORD = 0,
    FARM_BY_HEAD_TO_UINT64_FACTOR_FROM_QWORD,
    FARM_BY_HEAD_TO_INT64_FROM_QWORD,
    FARM_BY_HEAD_TO_INT64_FACTOR_FROM_QWORD,
    FARM_BY_HEAD_TO_UINT64_HEX_FROM_QWORD,
    FARM_BY_HEAD_TO_FLOAT_FROM_QWORD,
    FARM_BY_HEAD_TO_TIME_FROM_QWORD,
    FARM_BY_HEAD_TO_GPES_FROM_QWORD, // Special case for get physical element status. This reports health in byte 1 and
                                     // a timestamp in the data
} eFARMByHeadOutputFormat;

static void create_Node_For_Head_Data_From_QWords(json_object*            node,
                                                  const char*             nodeName,
                                                  uint64_t*               byHead,
                                                  uint64_t                numberOfHeads,
                                                  eFARMByHeadOutputFormat outputFormat,
                                                  double                  conversionFactor)
{
    if (byHead != M_NULLPTR)
    {
        bool         headDataAdded  = false;
        json_object* headValueArray = json_object_new_array();

        for (uint8_t headIter = UINT8_C(0); headIter < FARM_MAX_HEADS && headIter < numberOfHeads; ++headIter)
        {
            uint8_t status = get_Farm_Status_Byte(byHead[headIter]);
            if ((status & FARM_FIELD_SUPPORTED_BIT) > 0)
            {
                headDataAdded = true; // atleast one head data has support bit enabled
                // to add value for each head
                json_object* headDataNode = json_object_new_object();

                if ((status & FARM_FIELD_VALID_BIT) > 0)
                {
                    switch (outputFormat)
                    {
                    case FARM_BY_HEAD_TO_UINT64_FROM_QWORD:
                        create_Node_For_UINT64_From_QWord(headDataNode, "value", byHead[headIter]);
                        break;

                    case FARM_BY_HEAD_TO_UINT64_FACTOR_FROM_QWORD:
                        create_Node_For_Float_With_Factor_From_QWord(headDataNode, "value", byHead[headIter],
                                                                     conversionFactor, false);
                        break;

                    case FARM_BY_HEAD_TO_INT64_FROM_QWORD:
                        create_Node_For_INT64_From_QWord(headDataNode, "value", byHead[headIter]);
                        break;

                    case FARM_BY_HEAD_TO_INT64_FACTOR_FROM_QWORD:
                        create_Node_For_Float_With_Factor_From_QWord(headDataNode, "value", byHead[headIter],
                                                                     conversionFactor, true);
                        break;

                    case FARM_BY_HEAD_TO_UINT64_HEX_FROM_QWORD:
                        create_Node_For_UINT64_Hex_From_QWord(headDataNode, "value", byHead[headIter]);
                        break;

                    case FARM_BY_HEAD_TO_FLOAT_FROM_QWORD:
                        create_Node_For_Float_From_QWord(headDataNode, "value", byHead[headIter]);
                        break;

                    case FARM_BY_HEAD_TO_TIME_FROM_QWORD:
                        create_Node_For_Time_From_QWord(headDataNode, "value", byHead[headIter], conversionFactor);
                        break;

                    case FARM_BY_HEAD_TO_GPES_FROM_QWORD:
                    {
                        // add health
                        uint8_t health = M_Byte0(get_Farm_Qword_Data(byHead[headIter]));
                        DECLARE_ZERO_INIT_ARRAY(char, healthValue, MAX_UINT8_TO_DEC_STRING_LENGHT);
                        snprintf_err_handle(healthValue, MAX_UINT8_TO_DEC_STRING_LENGHT, "%" PRId8 "", health);
                        json_object_object_add(headDataNode, "health", json_object_new_string(healthValue));

                        // add timestamp
                        uint64_t timestamp = get_bit_range_uint64(get_Farm_Qword_Data(byHead[headIter]), 39, 8);
                        DECLARE_ZERO_INIT_ARRAY(char, timestampValue, MAX_UINT64_TO_DEC_STRING_LENGHT);
                        snprintf_err_handle(timestampValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "%" PRId64 "", timestamp);
                        json_object_object_add(headDataNode, "timestamp", json_object_new_string(timestampValue));
                    }
                    break;

                    default:
                        // invalid format
                        json_object_object_add(headDataNode, "value", json_object_new_string("Invalid"));
                        break;
                    }
                }
                else
                {
                    // invalid data
                    json_object_object_add(headDataNode, "value", json_object_new_string("Invalid"));
                }

                // create new node, name if Head #?
                json_object* head = json_object_new_object();
                DECLARE_ZERO_INIT_ARRAY(char, headNodeName, MAX_HEAD_NODE_NAME_LENGTH);
                snprintf_err_handle(headNodeName, MAX_HEAD_NODE_NAME_LENGTH, "Head %" PRIu8, (headIter + 1));
                json_object_object_add(head, headNodeName, headDataNode);

                // Add it into array
                json_object_array_add(headValueArray, head);
            }
        }

        if (headDataAdded)
            json_object_object_add(node, nodeName, headValueArray);
    }
}

static void create_Node_For_Head_Data_With_Delta_Or_Unit_From_QWords(json_object* node,
                                                                     const char*  nodeName,
                                                                     const char*  unitName,
                                                                     uint64_t*    byHead,
                                                                     uint64_t     numberOfHeads)
{
    DECLARE_ZERO_INIT_ARRAY(char, updatedNodeName, MAX_UPDATED_NODE_NAME_LENGHT);
    if (get_Farm_Float_Bits(byHead[0]) & FARM_FLOAT_PERCENT_DELTA_FACTORY_BIT ||
        get_Farm_Float_Bits(byHead[0]) & FARM_FLOAT_NEGATIVE_BIT)
    {
        snprintf_err_handle(updatedNodeName, MAX_UPDATED_NODE_NAME_LENGHT, "%s (%% delta)", nodeName);
        create_Node_For_Head_Data_From_QWords(node, updatedNodeName, byHead, numberOfHeads,
                                              FARM_BY_HEAD_TO_FLOAT_FROM_QWORD, 0.0);
    }
    else
    {
        snprintf_err_handle(updatedNodeName, MAX_UPDATED_NODE_NAME_LENGHT, "%s (%s)", nodeName, unitName);
        create_Node_For_Head_Data_From_QWords(node, updatedNodeName, byHead, numberOfHeads,
                                              FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.001);
    }
}

#define THREE_STATS_IN_ONE    3
#define MAX_STATS_NAME_LENGTH 3
static void create_Node_For_Head_Data_With_3_Stat_From_QWord(
    json_object*            node,
    const char*             nodeName,
    const char              statisticsName[THREE_STATS_IN_ONE][MAX_STATS_NAME_LENGTH],
    uint64_t                byHead[FARM_MAX_HEADS][THREE_STATS_IN_ONE],
    uint64_t                numberOfHeads,
    eFARMByHeadOutputFormat outputFormat,
    double                  conversionFactor)
{
    if (byHead != M_NULLPTR)
    {
        bool         headDataAdded  = false;
        json_object* headValueArray = json_object_new_array();

        for (uint8_t headIter = UINT8_C(0); headIter < FARM_MAX_HEADS && headIter < numberOfHeads; ++headIter)
        {
            uint8_t status = get_Farm_Status_Byte(byHead[headIter][0]);
            if ((status & FARM_FIELD_SUPPORTED_BIT) > 0)
            {
                headDataAdded = true; // atleast one head data has support bit enabled
                // to add value for each head
                json_object* headDataNode = json_object_new_object();

                if ((status & FARM_FIELD_VALID_BIT) > 0)
                {
                    switch (outputFormat)
                    {
                    case FARM_BY_HEAD_TO_UINT64_FACTOR_FROM_QWORD:
                        for (uint8_t statNum = 0; statNum < THREE_STATS_IN_ONE; ++statNum)
                        {
                            create_Node_For_Float_With_Factor_From_QWord(headDataNode, statisticsName[statNum],
                                                                         byHead[headIter][statNum], conversionFactor,
                                                                         false);
                        }
                        break;

                    case FARM_BY_HEAD_TO_INT64_FACTOR_FROM_QWORD:
                        for (uint8_t statNum = 0; statNum < THREE_STATS_IN_ONE; ++statNum)
                        {
                            create_Node_For_Float_With_Factor_From_QWord(headDataNode, statisticsName[statNum],
                                                                         byHead[headIter][statNum], conversionFactor,
                                                                         true);
                        }
                        break;

                    case FARM_BY_HEAD_TO_UINT64_FROM_QWORD:
                    case FARM_BY_HEAD_TO_INT64_FROM_QWORD:
                    case FARM_BY_HEAD_TO_UINT64_HEX_FROM_QWORD:
                    case FARM_BY_HEAD_TO_FLOAT_FROM_QWORD:
                    case FARM_BY_HEAD_TO_TIME_FROM_QWORD:
                    case FARM_BY_HEAD_TO_GPES_FROM_QWORD:
                    default:
                        // invalid format
                        json_object_object_add(headDataNode, "value", json_object_new_string("Invalid"));
                        break;
                    }
                }
                else
                {
                    // invalid data
                    json_object_object_add(headDataNode, "value", json_object_new_string("Invalid"));
                }

                // create new node, name if Head #?
                json_object* head = json_object_new_object();
                DECLARE_ZERO_INIT_ARRAY(char, headNodeName, MAX_HEAD_NODE_NAME_LENGTH);
                snprintf_err_handle(headNodeName, MAX_HEAD_NODE_NAME_LENGTH, "Head %" PRIu8, (headIter + 1));
                json_object_object_add(head, headNodeName, headDataNode);

                // Add it into array
                json_object_array_add(headValueArray, head);
            }
        }

        if (headDataAdded)
            json_object_object_add(node, nodeName, headValueArray);
    }
}

static void create_Node_For_FARM_Flash_LED_Events(json_object* node, farmErrorStatistics* error)
{
    if (error != M_NULLPTR && get_Farm_Qword_Data(error->pageNumber) == FARM_PAGE_ERROR_STATS)
    {
        json_object* actuatorArray = json_object_new_array();
        bool         addInArray    = false;

        // add actuator 0
        {
            json_object* outerNode = json_object_new_object();

            uint8_t status = get_Farm_Status_Byte(error->totalFlashLEDEvents);
            if (status & FARM_FIELD_SUPPORTED_BIT && status & FARM_FIELD_VALID_BIT)
            {
                addInArray                 = true;
                json_object* actuator0Node = json_object_new_object();

                create_Node_For_UINT64_From_QWord(actuator0Node, "Total Flash LED Events", error->totalFlashLEDEvents);

                status = get_Farm_Status_Byte(error->lastFLEDIndex);
                if (status & FARM_FIELD_SUPPORTED_BIT && status & FARM_FIELD_VALID_BIT)
                {
                    int64_t index      = M_STATIC_CAST(int64_t, get_Farm_Qword_Data(error->lastFLEDIndex));
                    uint8_t eventCount = UINT8_C(0);
                    // now add flash LED events in form of array
                    json_object* eventArray = json_object_new_array();
                    while (eventCount < FARM_FLED_EVENTS && index < FARM_FLED_EVENTS && index >= INT64_C(0))
                    {
                        json_object* fLEDNode = json_object_new_object();

                        // add FLED Field
                        uint8_t  fledStatus = get_Farm_Status_Byte(error->last8FLEDEvents[index]);
                        uint64_t fledValue  = get_Farm_Qword_Data(error->last8FLEDEvents[index]);
                        DECLARE_ZERO_INIT_ARRAY(char, fLEDInfoValue, MAX_UINT64_TO_DEC_STRING_LENGHT);
                        if ((fledStatus & FARM_FIELD_SUPPORTED_BIT) > UINT8_C(0) &&
                            (fledStatus & FARM_FIELD_VALID_BIT) > UINT8_C(0))
                        {
                            snprintf_err_handle(fLEDInfoValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "%" PRIu64 "",
                                                fledValue);
                        }
                        else
                        {
                            snprintf_err_handle(fLEDInfoValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "-");
                        }
                        json_object_object_add(fLEDNode, "FLED", json_object_new_string(fLEDInfoValue));

                        // add RW Retry field
                        fledStatus = get_Farm_Status_Byte(error->last8ReadWriteRetryEvents[index]);
                        fledValue  = get_Farm_Qword_Data(error->last8ReadWriteRetryEvents[index]);
                        DECLARE_ZERO_INIT_ARRAY(char, fLEDRWRetryValue, MAX_UINT64_TO_DEC_STRING_LENGHT);
                        if ((fledStatus & FARM_FIELD_SUPPORTED_BIT) > UINT8_C(0) &&
                            (fledStatus & FARM_FIELD_VALID_BIT) > UINT8_C(0))
                        {
                            snprintf_err_handle(fLEDRWRetryValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "%" PRIu64 "",
                                                fledValue);
                        }
                        else
                        {
                            snprintf_err_handle(fLEDRWRetryValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "-");
                        }
                        json_object_object_add(fLEDNode, "RW Retry", json_object_new_string(fLEDRWRetryValue));

                        // add Timestamp field
                        fledStatus = get_Farm_Status_Byte(error->timestampOfLast8FLEDs[index]);
                        fledValue  = get_Farm_Qword_Data(error->timestampOfLast8FLEDs[index]);
                        DECLARE_ZERO_INIT_ARRAY(char, fLEDTimestampValue, MAX_UINT64_TO_DEC_STRING_LENGHT);
                        if ((fledStatus & FARM_FIELD_SUPPORTED_BIT) > UINT8_C(0) &&
                            (fledStatus & FARM_FIELD_VALID_BIT) > UINT8_C(0))
                        {
                            snprintf_err_handle(fLEDTimestampValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "%" PRIu64 "",
                                                fledValue);
                        }
                        else
                        {
                            snprintf_err_handle(fLEDTimestampValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "-");
                        }
                        json_object_object_add(fLEDNode, "Timestamp", json_object_new_string(fLEDTimestampValue));

                        // add Power Cycle field
                        fledStatus = get_Farm_Status_Byte(error->powerCycleOfLast8FLEDs[index]);
                        fledValue  = get_Farm_Qword_Data(error->powerCycleOfLast8FLEDs[index]);
                        DECLARE_ZERO_INIT_ARRAY(char, fLEDPowerCycleValue, MAX_UINT64_TO_DEC_STRING_LENGHT);
                        if ((fledStatus & FARM_FIELD_SUPPORTED_BIT) > UINT8_C(0) &&
                            (fledStatus & FARM_FIELD_VALID_BIT) > UINT8_C(0))
                        {
                            snprintf_err_handle(fLEDPowerCycleValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "%" PRIu64 "",
                                                fledValue);
                        }
                        else
                        {
                            snprintf_err_handle(fLEDPowerCycleValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "-");
                        }
                        json_object_object_add(fLEDNode, "Power Cycle", json_object_new_string(fLEDPowerCycleValue));

                        // create new node, name if Field#? and then add name-value pair in this node
                        json_object* event = json_object_new_object();
                        DECLARE_ZERO_INIT_ARRAY(char, eventNodeName, MAX_FLED_EVENT_NODE_NAME_LENGTH);
                        snprintf_err_handle(eventNodeName, MAX_FLED_EVENT_NODE_NAME_LENGTH, "Flash LED Event # %" PRIu8,
                                            (eventCount + 1));
                        json_object_object_add(event, eventNodeName, fLEDNode);

                        // Add it into array
                        json_object_array_add(eventArray, event);

                        // decrement index. Reset to zero if we go past the last event since this is a wrapping array.
                        --index;
                        if (index < INT64_C(0))
                        {
                            index = FARM_FLED_EVENTS;
                        }
                        ++eventCount;
                    }

                    json_object_object_add(actuator0Node, "Flash LED Events", eventArray);
                }

                json_object_object_add(outerNode, "Actuator 0", actuator0Node);

                // Add it into array
                json_object_array_add(actuatorArray, outerNode);
            }
        }

        // add actuator 1
        {
            json_object* outerNode = json_object_new_object();

            uint8_t status = get_Farm_Status_Byte(error->totalFlashLEDEventsActuator1);
            if (status & FARM_FIELD_SUPPORTED_BIT && status & FARM_FIELD_VALID_BIT)
            {
                addInArray                 = true;
                json_object* actuator1Node = json_object_new_object();

                create_Node_For_UINT64_From_QWord(actuator1Node, "Total Flash LED Events",
                                                  error->totalFlashLEDEventsActuator1);

                status = get_Farm_Status_Byte(error->lastFLEDIndexActuator1);
                if (status & FARM_FIELD_SUPPORTED_BIT && status & FARM_FIELD_VALID_BIT)
                {
                    int64_t index      = M_STATIC_CAST(int64_t, get_Farm_Qword_Data(error->lastFLEDIndexActuator1));
                    uint8_t eventCount = UINT8_C(0);
                    // now add flash LED events in form of array
                    json_object* eventArray = json_object_new_array();
                    while (eventCount < FARM_FLED_EVENTS && index < FARM_FLED_EVENTS && index >= INT64_C(0))
                    {
                        json_object* fLEDNode = json_object_new_object();

                        // add FLED Field
                        uint8_t  fledStatus = get_Farm_Status_Byte(error->last8FLEDEventsActuator1[index]);
                        uint64_t fledValue  = get_Farm_Qword_Data(error->last8FLEDEventsActuator1[index]);
                        DECLARE_ZERO_INIT_ARRAY(char, fLEDInfoValue, MAX_UINT64_TO_DEC_STRING_LENGHT);
                        if ((fledStatus & FARM_FIELD_SUPPORTED_BIT) > UINT8_C(0) &&
                            (fledStatus & FARM_FIELD_VALID_BIT) > UINT8_C(0))
                        {
                            snprintf_err_handle(fLEDInfoValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "%" PRIu64 "",
                                                fledValue);
                        }
                        else
                        {
                            snprintf_err_handle(fLEDInfoValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "-");
                        }
                        json_object_object_add(fLEDNode, "FLED", json_object_new_string(fLEDInfoValue));

                        // add RW Retry field
                        fledStatus = get_Farm_Status_Byte(error->last8ReadWriteRetryEventsActuator1[index]);
                        fledValue  = get_Farm_Qword_Data(error->last8ReadWriteRetryEventsActuator1[index]);
                        DECLARE_ZERO_INIT_ARRAY(char, fLEDRWRetryValue, MAX_UINT64_TO_DEC_STRING_LENGHT);
                        if ((fledStatus & FARM_FIELD_SUPPORTED_BIT) > UINT8_C(0) &&
                            (fledStatus & FARM_FIELD_VALID_BIT) > UINT8_C(0))
                        {
                            snprintf_err_handle(fLEDRWRetryValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "%" PRIu64 "",
                                                fledValue);
                        }
                        else
                        {
                            snprintf_err_handle(fLEDRWRetryValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "-");
                        }
                        json_object_object_add(fLEDNode, "RW Retry", json_object_new_string(fLEDRWRetryValue));

                        // add Timestamp field
                        fledStatus = get_Farm_Status_Byte(error->timestampOfLast8FLEDsActuator1[index]);
                        fledValue  = get_Farm_Qword_Data(error->timestampOfLast8FLEDsActuator1[index]);
                        DECLARE_ZERO_INIT_ARRAY(char, fLEDTimestampValue, MAX_UINT64_TO_DEC_STRING_LENGHT);
                        if ((fledStatus & FARM_FIELD_SUPPORTED_BIT) > UINT8_C(0) &&
                            (fledStatus & FARM_FIELD_VALID_BIT) > UINT8_C(0))
                        {
                            snprintf_err_handle(fLEDTimestampValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "%" PRIu64 "",
                                                fledValue);
                        }
                        else
                        {
                            snprintf_err_handle(fLEDTimestampValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "-");
                        }
                        json_object_object_add(fLEDNode, "Timestamp", json_object_new_string(fLEDTimestampValue));

                        // add Power Cycle field
                        fledStatus = get_Farm_Status_Byte(error->powerCycleOfLast8FLEDsActuator1[index]);
                        fledValue  = get_Farm_Qword_Data(error->powerCycleOfLast8FLEDsActuator1[index]);
                        DECLARE_ZERO_INIT_ARRAY(char, fLEDPowerCycleValue, MAX_UINT64_TO_DEC_STRING_LENGHT);
                        if ((fledStatus & FARM_FIELD_SUPPORTED_BIT) > UINT8_C(0) &&
                            (fledStatus & FARM_FIELD_VALID_BIT) > UINT8_C(0))
                        {
                            snprintf_err_handle(fLEDPowerCycleValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "%" PRIu64 "",
                                                fledValue);
                        }
                        else
                        {
                            snprintf_err_handle(fLEDPowerCycleValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "-");
                        }
                        json_object_object_add(fLEDNode, "Power Cycle", json_object_new_string(fLEDPowerCycleValue));

                        // create new node, name if Field#? and then add name-value pair in this node
                        json_object* event = json_object_new_object();
                        DECLARE_ZERO_INIT_ARRAY(char, eventNodeName, MAX_FLED_EVENT_NODE_NAME_LENGTH);
                        snprintf_err_handle(eventNodeName, MAX_FLED_EVENT_NODE_NAME_LENGTH, "Flash LED Event # %" PRIu8,
                                            (eventCount + 1));
                        json_object_object_add(event, eventNodeName, fLEDNode);

                        // Add it into array
                        json_object_array_add(eventArray, event);

                        // decrement index. Reset to zero if we go past the last event since this is a wrapping array.
                        --index;
                        if (index < INT64_C(0))
                        {
                            index = FARM_FLED_EVENTS;
                        }
                        ++eventCount;
                    }

                    json_object_object_add(actuator1Node, "Flash LED Events", eventArray);
                }

                json_object_object_add(outerNode, "Actuator 1", actuator1Node);

                // Add it into array
                json_object_array_add(actuatorArray, outerNode);
            }
        }

        if (addInArray)
        {
            json_object_object_add(node, "Flash LED Events", actuatorArray);
        }
    }
}

static void create_Node_For_FARM_Header_Page(json_object* rootObject, farmHeader* header)
{
    json_object* headerNode = json_object_new_object();

    DECLARE_ZERO_INIT_ARRAY(char, value, MAX_UINT64_TO_DEC_STRING_LENGHT + MAX_UINT64_TO_DEC_STRING_LENGHT + 1);
    snprintf_err_handle(value, (MAX_UINT64_TO_DEC_STRING_LENGHT + MAX_UINT64_TO_DEC_STRING_LENGHT + 1),
                        "%" PRIu64 ".%" PRIu64 "", get_Farm_Qword_Data(header->majorVersion),
                        get_Farm_Qword_Data(header->minorVersion));
    json_object_object_add(headerNode, "FARM Version", json_object_new_string(value));

    json_object_object_add(rootObject, "FARM Log Header", headerNode);
}

static void create_Node_For_FARM_Drive_Information_Page(json_object*        rootObject,
                                                        farmDriveInfo*      driveInfo,
                                                        eFARMDriveInterface farmInterface)
{
    if (driveInfo != M_NULLPTR && get_Farm_Qword_Data(driveInfo->pageNumber) == FARM_PAGE_DRIVE_INFO)
    {
        json_object* driveInfoNode = json_object_new_object();

        create_Node_For_String_Type_From_Multiple_QWords(driveInfoNode, "Model Number", &driveInfo->modelNumber[0],
                                                         FARM_DRIVE_INFO_MN_ASCII_LEN);
        create_Node_For_String_Type_From_Multiple_QWords(driveInfoNode, "Serial Number", &driveInfo->sn[0],
                                                         FARM_DRIVE_INFO_SN_ASCII_LEN);
        create_Node_For_String_Type_From_Multiple_QWords(driveInfoNode, "Firmware Revision", &driveInfo->fwrev[0],
                                                         FARM_DRIVE_INFO_FWREV_ASCII_LEN);
        create_Node_For_UINT64_Hex_From_2_QWords(driveInfoNode, "World Wide Name", &driveInfo->wwn[0]);
        create_Node_For_DOA_From_QWord(driveInfoNode, "Date Of Assembly", driveInfo->dateOfAssembly);
        json_object_object_add(driveInfoNode, "Drive Interface",
                               farmInterface == FARM_DRIVE_INTERFACE_SATA ? json_object_new_string("SATA")
                                                                          : json_object_new_string("SAS"));
        create_Node_For_UINT64_From_QWord(driveInfoNode, "Device Capacity (LBAs)", driveInfo->driveCapacity);
        create_Node_For_UINT64_From_QWord(driveInfoNode, "Number of LBAs (HSMR SWR capacity)", driveInfo->numberOfLBAs);
        create_Node_For_UINT64_From_QWord(driveInfoNode, "Physical Sector Size (B)", driveInfo->physicalSectorSize);
        create_Node_For_UINT64_From_QWord(driveInfoNode, "Logical Sector Size (B)", driveInfo->logicalSectorSize);
        create_Node_For_UINT64_From_QWord(driveInfoNode, "Device Buffer Size (B)", driveInfo->deviceBufferSize);
        create_Node_For_UINT64_From_QWord(driveInfoNode, "Number Of Heads", driveInfo->numberOfHeads);
        create_Node_For_Recording_Type_From_QWord(driveInfoNode, "Drive Recording Type", driveInfo->driveRecordingType);
        create_Node_For_UINT64_Hex_From_QWord(driveInfoNode, "Form Factor", driveInfo->deviceFormFactor);
        create_Node_For_UINT64_From_QWord(driveInfoNode, "Rotation Rate", driveInfo->rotationRate);
        create_Node_For_UINT64_Hex_From_QWord(driveInfoNode, "ATA Security State", driveInfo->ataSecurityState);
        create_Node_For_UINT64_Hex_From_QWord(driveInfoNode, "ATA Features Supported", driveInfo->ataFeaturesSupported);
        create_Node_For_UINT64_Hex_From_QWord(driveInfoNode, "ATA Features Enabled", driveInfo->ataFeaturesEnabled);
        create_Node_For_Time_From_QWord(driveInfoNode, "Power On Hours", driveInfo->powerOnHours,
                                        MICRO_SECONDS_PER_HOUR);
        create_Node_For_Time_From_QWord(driveInfoNode, "Spindle Power On Hours", driveInfo->spindlePowerOnHours,
                                        MICRO_SECONDS_PER_HOUR);
        create_Node_For_Time_From_QWord(driveInfoNode, "Head Flight Hours", driveInfo->headFlightHours,
                                        MICRO_SECONDS_PER_HOUR);
        create_Node_For_Time_From_QWord(driveInfoNode, "Head Flight Hours, Actuator 1",
                                        driveInfo->headFlightHoursActuator1, MICRO_SECONDS_PER_HOUR);
        create_Node_For_UINT64_From_QWord(driveInfoNode, "Head Load Events", driveInfo->headLoadEvents);
        create_Node_For_UINT64_From_QWord(driveInfoNode, "Head Load Events, Actuator 1",
                                          driveInfo->headLoadEventsActuator1);
        create_Node_For_UINT64_From_QWord(driveInfoNode, "Power Cycle Count", driveInfo->powerCycleCount);
        create_Node_For_UINT64_From_QWord(driveInfoNode, "Hardware Reset Count", driveInfo->hardwareResetCount);
        create_Node_For_UINT64_From_QWord(driveInfoNode, "Spin up time (ms)", driveInfo->spinUpTimeMilliseconds);
        create_Node_For_UINT64_From_QWord(driveInfoNode, "Time to ready, last power cycle (ms)",
                                          driveInfo->timeToReadyOfLastPowerCycle);
        create_Node_For_UINT64_From_QWord(driveInfoNode, "Time in staggered spinup, last power on sequence (ms)",
                                          driveInfo->timeDriveHeldInStaggeredSpinDuringLastPowerOnSequence);
        if (farmInterface == FARM_DRIVE_INTERFACE_SAS)
        {
            create_Node_For_UINT64_From_QWord(driveInfoNode, "NVC Status at Power On", driveInfo->nvcStatusOnPoweron);
            create_Node_For_UINT64_From_QWord(driveInfoNode, "Time Availabe to Save User Data To NV Mem",
                                              driveInfo->timeAvailableToSaveUDToNVMem); // 100us
        }
        create_Node_For_Time_From_QWord(driveInfoNode, "Lowest POH timestamp (Hours)",
                                        driveInfo->lowestPOHForTimeRestrictedParameters,
                                        MICRO_SECONDS_PER_MILLI_SECONDS);
        create_Node_For_Time_From_QWord(driveInfoNode, "Highest POH timestamp (Hours)",
                                        driveInfo->highestPOHForTimeRestrictedParameters,
                                        MICRO_SECONDS_PER_MILLI_SECONDS);
        create_Node_For_Bool_From_QWord(driveInfoNode, "Depopulation Status", driveInfo->isDriveDepopulated,
                                        "Depopulated", "Not Depopulated");
        create_Node_For_UINT64_Hex_From_QWord(driveInfoNode, "Depopulation Head Mask", driveInfo->depopulationHeadMask);
        create_Node_For_UINT64_Hex_From_QWord(driveInfoNode, "Regeneration Head Mask", driveInfo->regenHeadMask);
        create_Node_For_Head_Data_From_QWords(driveInfoNode, "Physical Element Status (Health/Timestamp)",
                                              driveInfo->getPhysicalElementStatusByHead, driveInfo->numberOfHeads,
                                              FARM_BY_HEAD_TO_GPES_FROM_QWORD, 0.0);
        create_Node_For_UINT64_From_QWord(driveInfoNode, "Max # Available Disc Sectors for Reassignment",
                                          driveInfo->maxAvailableSectorsForReassignment);
        create_Node_For_Bool_From_QWord(driveInfoNode, "HAMR Data Protect Status", driveInfo->hamrDataProtectStatus,
                                        "Data Protect", "No Data Protect");
        create_Node_For_Time_From_QWord(driveInfoNode, "POH of Most Recent FARM TS Frame",
                                        driveInfo->pohOfMostRecentTimeseriesFrame, MICRO_SECONDS_PER_MILLI_SECONDS);
        create_Node_For_Time_From_QWord(driveInfoNode, "POH of 2nd Most Recent FARM TS Frame",
                                        driveInfo->pohOfSecondMostRecentTimeseriesFrame,
                                        MICRO_SECONDS_PER_MILLI_SECONDS);
        create_Node_For_UINT64_From_QWord(driveInfoNode, "Seq or Before Req for Active Zone Config",
                                          driveInfo->sequentialOrBeforeWriteRequiredForActiveZoneConfiguration);
        create_Node_For_UINT64_From_QWord(driveInfoNode, "Seq Write Req Active Zone Config",
                                          driveInfo->sequentialWriteRequiredForActiveZoneConfiguration);

        json_object_object_add(rootObject, "FARM Log Drive Information", driveInfoNode);
    }
}

static void create_Node_For_FARM_Workload_Statistics_Page(json_object*  rootObject,
                                                          farmWorkload* workload,
                                                          uint64_t      timeRestrictedRangeMS)
{
    if (workload != M_NULLPTR && get_Farm_Qword_Data(workload->pageNumber) == FARM_PAGE_WORKLOAD)
    {
        json_object* workloadNode = json_object_new_object();

        create_Node_For_UINT64_From_QWord(workloadNode, "Rated Workload (%)", workload->ratedWorkloadPercentage);
        create_Node_For_UINT64_From_QWord(workloadNode, "Total # of Read Commands", workload->totalReadCommands);
        create_Node_For_UINT64_From_QWord(workloadNode, "Total # of Write Commands", workload->totalWriteCommands);
        create_Node_For_UINT64_From_QWord(workloadNode, "Total # of Random Read Commands",
                                          workload->totalRandomReadCommands);
        create_Node_For_UINT64_From_QWord(workloadNode, "Total # of Random Write Commands",
                                          workload->totalRandomWriteCommands);
        create_Node_For_UINT64_From_QWord(workloadNode, "Total # of Other Commands", workload->totalOtherCommands);
        create_Node_For_UINT64_From_QWord(workloadNode, "LBAs Written", workload->logicalSectorsWritten);
        create_Node_For_UINT64_From_QWord(workloadNode, "LBAs Read", workload->logicalSectorsRead);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of Dither events in power cycle",
                                          workload->numberOfDitherEventsInCurrentPowerCycle);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of Dither events in power cycle, Actuator 1",
                                          workload->numberOfDitherEventsInCurrentPowerCycleActuator1);
        create_Node_For_UINT64_From_QWord(workloadNode, "# dither pause - random workloads in power cycle",
                                          workload->numberDitherHeldOffDueToRandomWorkloadsInCurrentPowerCycle);
        create_Node_For_UINT64_From_QWord(
            workloadNode, "# dither pause - random workloads in power cycle, Actuator 1",
            workload->numberDitherHeldOffDueToRandomWorkloadsInCurrentPowerCycleActuator1);
        create_Node_For_UINT64_From_QWord(workloadNode, "# dither pause - sequential workloads in power cycle",
                                          workload->numberDitherHeldOffDueToSequentialWorkloadsInCurrentPowerCycle);
        create_Node_For_UINT64_From_QWord(
            workloadNode, "# dither pause - sequential workloads in power cycle, Actuator 1",
            workload->numberDitherHeldOffDueToSequentialWorkloadsInCurrentPowerCycleActuator1);

        bool atleastOneReadWriteByLBASupported =
            (get_Farm_Status_Byte(workload->numReadsInLBA0To3125PercentRange) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numReadsInLBA3125To25PercentRange) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numReadsInLBA25To50PercentRange) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numReadsInLBA50To100PercentRange) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numWritesInLBA0To3125PercentRange) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numWritesInLBA3125To25PercentRange) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numWritesInLBA25To50PercentRange) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numWritesInLBA50To100PercentRange) & FARM_FIELD_SUPPORTED_BIT);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of read commands between 0-3.125% LBA space",
                                          workload->numReadsInLBA0To3125PercentRange);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of read commands between 3.125-25% LBA space",
                                          workload->numReadsInLBA3125To25PercentRange);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of read commands between 25-50% LBA space",
                                          workload->numReadsInLBA25To50PercentRange);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of read commands between 50-100% LBA space",
                                          workload->numReadsInLBA50To100PercentRange);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of write commands between 0-3.125% LBA space",
                                          workload->numWritesInLBA0To3125PercentRange);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of write commands between 3.125-25% LBA space",
                                          workload->numWritesInLBA3125To25PercentRange);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of write commands between 25-50% LBA space",
                                          workload->numWritesInLBA25To50PercentRange);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of write commands between 50-100% LBA space",
                                          workload->numWritesInLBA50To100PercentRange);
        if (atleastOneReadWriteByLBASupported)
        {
            create_Node_For_Time_From_QWord(workloadNode, "Time that Commands Cover (by LBA Space) (Hours)",
                                            timeRestrictedRangeMS ^ (BIT63 | BIT62), MICRO_SECONDS_PER_MILLI_SECONDS);
        }

        bool atleastOneReadWriteByXferSupported =
            (get_Farm_Status_Byte(workload->numReadsOfXferLenLT16KB) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numReadsOfXferLen16KBTo512KB) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numReadsOfXferLen512KBTo2MB) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numReadsOfXferLenGT2MB) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numWritesOfXferLenLT16KB) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numWritesOfXferLen16KBTo512KB) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numWritesOfXferLen512KBTo2MB) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numWritesOfXferLenGT2MB) & FARM_FIELD_SUPPORTED_BIT);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of read commands with xfer <= 16KiB",
                                          workload->numReadsOfXferLenLT16KB);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of read commands with xfer 16Kib - 512KiB",
                                          workload->numReadsOfXferLen16KBTo512KB);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of read commands with xfer 512KiB - 2MiB",
                                          workload->numReadsOfXferLen512KBTo2MB);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of read commands with xfer > 2MiB",
                                          workload->numReadsOfXferLenGT2MB);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of write commands with xfer <= 16KiB",
                                          workload->numWritesOfXferLenLT16KB);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of write commands with xfer 16Kib - 512KiB",
                                          workload->numWritesOfXferLen16KBTo512KB);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of write commands with xfer 512KiB - 2MiB",
                                          workload->numWritesOfXferLen512KBTo2MB);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of write commands with xfer > 2MiB",
                                          workload->numWritesOfXferLenGT2MB);
        if (atleastOneReadWriteByXferSupported)
        {
            create_Node_For_Time_From_QWord(workloadNode, "Time that Commands Cover (by xfer) (Hours)",
                                            timeRestrictedRangeMS ^ (BIT63 | BIT62), MICRO_SECONDS_PER_MILLI_SECONDS);
        }

        bool atleastOneQueueDepthSupported =
            (get_Farm_Status_Byte(workload->countQD1at30sInterval) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->countQD2at30sInterval) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->countQD3To4at30sInterval) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->countQD5To8at30sInterval) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->countQD9To16at30sInterval) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->countQD17To32at30sInterval) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->countQD33To64at30sInterval) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->countGTQD64at30sInterval) & FARM_FIELD_SUPPORTED_BIT);
        create_Node_For_UINT64_From_QWord(workloadNode, "Queue Depth = 1 in 30s intervals",
                                          workload->countQD1at30sInterval);
        create_Node_For_UINT64_From_QWord(workloadNode, "Queue Depth = 2 in 30s intervals",
                                          workload->countQD2at30sInterval);
        create_Node_For_UINT64_From_QWord(workloadNode, "Queue Depth 3-4 in 30s intervals",
                                          workload->countQD3To4at30sInterval);
        create_Node_For_UINT64_From_QWord(workloadNode, "Queue Depth 5-8 in 30s intervals",
                                          workload->countQD5To8at30sInterval);
        create_Node_For_UINT64_From_QWord(workloadNode, "Queue Depth 9-16 in 30s intervals",
                                          workload->countQD9To16at30sInterval);
        create_Node_For_UINT64_From_QWord(workloadNode, "Queue Depth 17-32 in 30s intervals",
                                          workload->countQD17To32at30sInterval);
        create_Node_For_UINT64_From_QWord(workloadNode, "Queue Depth 33-64 in 30s intervals",
                                          workload->countQD33To64at30sInterval);
        create_Node_For_UINT64_From_QWord(workloadNode, "Queue Depth > 64 in 30s intervals",
                                          workload->countGTQD64at30sInterval);
        if (atleastOneQueueDepthSupported)
        {
            create_Node_For_Time_From_QWord(workloadNode, "Time that Queue Bins Cover (Hours)",
                                            timeRestrictedRangeMS ^ (BIT63 | BIT62), MICRO_SECONDS_PER_MILLI_SECONDS);
        }

        bool atleastOneReadWriteByXferInLastSSFSupported =
            (get_Farm_Status_Byte(workload->numReadsXferLenBin4Last3SMARTSummaryFrames) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numReadsXferLenBin5Last3SMARTSummaryFrames) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numReadsXferLenBin6Last3SMARTSummaryFrames) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numReadsXferLenBin7Last3SMARTSummaryFrames) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numWritesXferLenBin4Last3SMARTSummaryFrames) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numWritesXferLenBin5Last3SMARTSummaryFrames) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numWritesXferLenBin6Last3SMARTSummaryFrames) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numWritesXferLenBin7Last3SMARTSummaryFrames) & FARM_FIELD_SUPPORTED_BIT);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of reads of xfer bin 4, last 3 SSF",
                                          workload->numReadsXferLenBin4Last3SMARTSummaryFrames);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of reads of xfer bin 5, last 3 SSF",
                                          workload->numReadsXferLenBin5Last3SMARTSummaryFrames);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of reads of xfer bin 6, last 3 SSF",
                                          workload->numReadsXferLenBin6Last3SMARTSummaryFrames);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of reads of xfer bin 7, last 3 SSF",
                                          workload->numReadsXferLenBin7Last3SMARTSummaryFrames);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of writes of xfer bin 4, last 3 SSF",
                                          workload->numWritesXferLenBin4Last3SMARTSummaryFrames);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of writes of xfer bin 5, last 3 SSF",
                                          workload->numWritesXferLenBin5Last3SMARTSummaryFrames);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of writes of xfer bin 6, last 3 SSF",
                                          workload->numWritesXferLenBin6Last3SMARTSummaryFrames);
        create_Node_For_UINT64_From_QWord(workloadNode, "# of writes of xfer bin 7, last 3 SSF",
                                          workload->numWritesXferLenBin7Last3SMARTSummaryFrames);
        if (atleastOneReadWriteByXferInLastSSFSupported)
        {
            create_Node_For_Time_From_QWord(workloadNode, "Time that XFer Bins Cover (Hours)",
                                            timeRestrictedRangeMS ^ (BIT63 | BIT62), MICRO_SECONDS_PER_MILLI_SECONDS);
        }

        json_object_object_add(rootObject, "FARM Log Workload Statistics", workloadNode);
    }
}

static void create_Node_For_FARM_Error_Statistics_Page(json_object*         rootObject,
                                                       farmErrorStatistics* error,
                                                       uint64_t             headCount,
                                                       eFARMDriveInterface  driveInterface)
{
    if (error != M_NULLPTR && get_Farm_Qword_Data(error->pageNumber) == FARM_PAGE_ERROR_STATS)
    {
        json_object* errorNode = json_object_new_object();

        create_Node_For_UINT64_From_QWord(errorNode, "# of Unrecoverable Read Errors",
                                          error->numberOfUnrecoverableReadErrors);
        create_Node_For_UINT64_From_QWord(errorNode, "# of Unrecoverable Write Errors",
                                          error->numberOfUnrecoverableWriteErrors);
        create_Node_For_UINT64_From_QWord(errorNode, "# of Reallocated Sectors", error->numberOfReallocatedSectors);
        create_Node_For_UINT64_From_QWord(errorNode, "# of Reallocated Sectors, Actuator 1",
                                          error->numberOfReallocatedSectorsActuator1);
        create_Node_For_UINT64_From_QWord(errorNode, "# of Read Recovery Attempts",
                                          error->numberOfReadRecoveryAttempts);
        create_Node_For_UINT64_From_QWord(errorNode, "# of Mechanical Start Retries",
                                          error->numberOfMechanicalStartRetries);
        create_Node_For_UINT64_From_QWord(errorNode, "# of Reallocation Candidate Sectors",
                                          error->numberOfReallocationCandidateSectors);
        create_Node_For_UINT64_From_QWord(errorNode, "# of Reallocated Candidate Sectors, Actuator 1",
                                          error->numberOfReallocationCandidateSectorsActuator1);
        if (driveInterface == FARM_DRIVE_INTERFACE_SATA)
        {
            // SATA
            create_Node_For_UINT64_From_QWord(errorNode, "# of ASR Events", error->sataErr.numberOfASREvents);
            create_Node_For_UINT64_From_QWord(errorNode, "# of Interface CRC Errors",
                                              error->sataErr.numberOfInterfaceCRCErrors);
            create_Node_For_UINT64_From_QWord(errorNode, "Spin Retry Count", error->sataErr.spinRetryCount);
            create_Node_For_UINT64_From_QWord(errorNode, "Normalized Spin Retry Count",
                                              error->sataErr.spinRetryCountNormalized);
            create_Node_For_UINT64_From_QWord(errorNode, "Worst Ever Spin Retry Count",
                                              error->sataErr.spinRetryCountWorstEver);
            create_Node_For_UINT64_From_QWord(errorNode, "# Of IOEDC Errors", error->sataErr.numberOfIOEDCErrors);
            create_Node_For_UINT64_From_QWord(errorNode, "# Of Command Timeouts", error->sataErr.commandTimeoutTotal);
            create_Node_For_UINT64_From_QWord(errorNode, "# Of Command Timeouts > 5 seconds",
                                              error->sataErr.commandTimeoutOver5s);
            create_Node_For_UINT64_From_QWord(errorNode, "# Of Command Timeouts > 7.5 seconds",
                                              error->sataErr.commandTimeoutOver7pt5s);
        }
        else if (driveInterface == FARM_DRIVE_INTERFACE_SAS)
        {
            // SAS
            create_Node_For_UINT64_Hex_From_QWord(errorNode, "FRU of SMART Trip Most Recent Frame",
                                                  error->sasErr.fruCodeOfSMARTTripMostRecentFrame);
            create_Node_For_UINT64_From_QWord(errorNode, "Port A Invalid Dword Count",
                                              error->sasErr.portAinvDWordCount);
            create_Node_For_UINT64_From_QWord(errorNode, "Port B Invalid Dword Count",
                                              error->sasErr.portBinvDWordCount);
            create_Node_For_UINT64_From_QWord(errorNode, "Port A Disparity Error Count",
                                              error->sasErr.portADisparityErrCount);
            create_Node_For_UINT64_From_QWord(errorNode, "Port B Disparity Error Count",
                                              error->sasErr.portBDisparityErrCount);
            create_Node_For_UINT64_From_QWord(errorNode, "Port A Loss of DWord Sync",
                                              error->sasErr.portAlossOfDWordSync);
            create_Node_For_UINT64_From_QWord(errorNode, "Port B Loss of DWord Sync",
                                              error->sasErr.portBlossOfDWordSync);
            create_Node_For_UINT64_From_QWord(errorNode, "Port A Phy Reset Problem",
                                              error->sasErr.portAphyResetProblem);
            create_Node_For_UINT64_From_QWord(errorNode, "Port B Phy Reset Problem",
                                              error->sasErr.portBphyResetProblem);
        }
        create_Node_For_FARM_Flash_LED_Events(errorNode, error);
        create_Node_For_UINT64_From_QWord(errorNode, "Lifetime # Unrecoverable Read Errors due to ERC",
                                          error->cumulativeLifetimeUnrecoverableReadErrorsDueToERC);
        create_Node_For_Head_Data_From_QWords(errorNode, "Cumulative Lifetime Unrecoverable Read Repeat",
                                              error->cumLTUnrecReadRepeatByHead, headCount,
                                              FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0);
        create_Node_For_Head_Data_From_QWords(errorNode, "Cumulative Lifetime Unrecoverable Read Unique",
                                              error->cumLTUnrecReadUniqueByHead, headCount,
                                              FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0);
        create_Node_For_UINT64_Hex_From_QWord(errorNode, "SMART Trip Flags 1", error->sataPFAAttributes[0]);
        create_Node_For_UINT64_Hex_From_QWord(errorNode, "SMART Trip Flags 2", error->sataPFAAttributes[1]);
        create_Node_For_UINT64_From_QWord(errorNode, "# Reallocated Sectors since last FARM TS Frame",
                                          error->numberReallocatedSectorsSinceLastFARMTimeSeriesFrameSaved);
        create_Node_For_UINT64_From_QWord(errorNode, "# Reallocated Sectors N to N-1 FARM TS Frame",
                                          error->numberReallocatedSectorsBetweenFarmTimeSeriesFrameNandNminus1);
        create_Node_For_UINT64_From_QWord(errorNode, "# Realloc Candidate Sectors since last FARM TS Frame",
                                          error->numberReallocationCandidateSectorsSinceLastFARMTimeSeriesFrameSaved);
        create_Node_For_UINT64_From_QWord(
            errorNode, "# Reallocation Candidate N to N-1 FARM TS Frame",
            error->numberReallocationCandidateSectorsBetweenFarmTimeSeriesFrameNandNminus1);
        create_Node_For_UINT64_From_QWord(errorNode, "# Reallocated Sectors since last FARM TS Frame, Actuator 1",
                                          error->numberReallocatedSectorsSinceLastFARMTimeSeriesFrameSavedActuator1);
        create_Node_For_UINT64_From_QWord(
            errorNode, "# Reallocated Sectors N to N-1 FARM TS Frame, Actuator 1",
            error->numberReallocatedSectorsBetweenFarmTimeSeriesFrameNandNminus1Actuator1);
        create_Node_For_UINT64_From_QWord(
            errorNode, "# Reallocation Candidate Sectors since last FARM TS Frame Actuator 1",
            error->numberReallocationCandidateSectorsSinceLastFARMTimeSeriesFrameSavedActuator1);
        create_Node_For_UINT64_From_QWord(
            errorNode, "# Reallocation Candidate N to N-1 FARM TS Frame Actuator 1",
            error->numberReallocationCandidateSectorsBetweenFarmTimeSeriesFrameNandNminus1Actuator1);
        create_Node_For_Head_Data_From_QWords(
            errorNode, "# Unique Unrec sect since last FARM TS Frame",
            error->numberUniqueUnrecoverableSectorsSinceLastFARMTimeSeriesFrameSavedByHead, headCount,
            FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0);
        create_Node_For_Head_Data_From_QWords(
            errorNode, "# Unique Unrec sect N to N-1 FARM TS Frame",
            error->numberUniqueUnrecoverableSectorsBetweenFarmTimeSeriesFrameNandNminus1ByHead, headCount,
            FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0);

        json_object_object_add(rootObject, "FARM Log Error Statistics", errorNode);
    }
}

static void create_Node_For_FARM_Enviornmental_Statistics_Page(json_object*               rootObject,
                                                               farmEnvironmentStatistics* environment,
                                                               eFARMDriveInterface        driveInterface,
                                                               uint64_t                   timeRestrictedRangeMS)
{
    if (environment != M_NULLPTR && get_Farm_Qword_Data(environment->pageNumber) == FARM_PAGE_ENVIRONMENT_STATS)
    {
        json_object* environmentalNode = json_object_new_object();

        create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Current Temperature (C)",
                                                     environment->currentTemperature,
                                                     driveInterface == FARM_DRIVE_INTERFACE_SAS ? 0.1 : 1.0, true);
        create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Highest Temperature (C)",
                                                     environment->highestTemperature,
                                                     driveInterface == FARM_DRIVE_INTERFACE_SAS ? 0.1 : 1.0, true);
        create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Lowest Temperature (C)",
                                                     environment->lowestTemperature,
                                                     driveInterface == FARM_DRIVE_INTERFACE_SAS ? 0.1 : 1.0, true);
        create_Node_For_INT64_From_QWord(environmentalNode, "Average Short Term Temperature (C)",
                                         environment->avgShortTermTemp);
        create_Node_For_INT64_From_QWord(environmentalNode, "Average Long Term Temperature (C)",
                                         environment->avgLongTermTemp);
        create_Node_For_INT64_From_QWord(environmentalNode, "Highest Average Short Term Temperature (C)",
                                         environment->highestAvgShortTermTemp);
        create_Node_For_INT64_From_QWord(environmentalNode, "Lowest Average Short Term Temperature (C)",
                                         environment->lowestAvgShortTermTemp);
        create_Node_For_INT64_From_QWord(environmentalNode, "Highest Average Long Term Temperature (C)",
                                         environment->highestAvgLongTermTemp);
        create_Node_For_INT64_From_QWord(environmentalNode, "Lowest Average Long Term Temperature (C)",
                                         environment->lowestAvgLongTermTemp);
        create_Node_For_Time_From_QWord(environmentalNode, "Time in Over Temperature (Hours)",
                                        environment->timeOverTemp, MICRO_SECONDS_PER_MINUTE);
        create_Node_For_Time_From_QWord(environmentalNode, "Time in Under Temperature (Hours)",
                                        environment->timeUnderTemp, MICRO_SECONDS_PER_MINUTE);
        create_Node_For_UINT64_From_QWord(environmentalNode, "Specified Max Temperature (C)",
                                          environment->specifiedMaxTemp);
        create_Node_For_UINT64_From_QWord(environmentalNode, "Specified Min Temperature (C)",
                                          environment->specifiedMinTemp);
        create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Current Relative Humidity (%)",
                                                     environment->currentRelativeHumidity, 0.1, false);
        create_Node_For_INT64_From_QWord(environmentalNode, "Current Motor Power Scalar",
                                         environment->currentMotorPowerFromMostRecentSMARTSummaryFrame);
        if ((get_Farm_Status_Byte(environment->currentMotorPowerFromMostRecentSMARTSummaryFrame) &
             FARM_FIELD_SUPPORTED_BIT) > 0)
        {
            create_Node_For_Time_From_QWord(environmentalNode, "Time Coverage for Motor Power (Hours)",
                                            timeRestrictedRangeMS ^ (BIT63 | BIT62), MICRO_SECONDS_PER_MILLI_SECONDS);
        }

        bool atleastOnePowerInputbyVoltageSupported =
            (get_Farm_Status_Byte(environment->current12Vinput) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(environment->min12Vinput) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(environment->max12Vinput) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(environment->current5Vinput) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(environment->min5Vinput) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(environment->max5Vinput) & FARM_FIELD_SUPPORTED_BIT);
        create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Current 12v input (V)",
                                                     environment->current12Vinput, 0.001, false);
        create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Min 12v input (V)", environment->min12Vinput,
                                                     0.001, false);
        create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Max 12v input (V)", environment->max12Vinput,
                                                     0.001, false);
        create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Current 5v input (V)",
                                                     environment->current5Vinput, 0.001, false);
        create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Min 5v input (V)", environment->min5Vinput,
                                                     0.001, false);
        create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Max 5v input (V)", environment->max5Vinput,
                                                     0.001, false);
        if (atleastOnePowerInputbyVoltageSupported)
        {
            create_Node_For_Time_From_QWord(environmentalNode, "Time Coverage for 12v & 5v voltage (Hours)",
                                            timeRestrictedRangeMS ^ (BIT63 | BIT62), MICRO_SECONDS_PER_MILLI_SECONDS);
        }

        bool atleastOnePowerInputbyWattSupported =
            (get_Farm_Status_Byte(environment->average12Vpwr) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(environment->min12VPwr) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(environment->max12VPwr) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(environment->average5Vpwr) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(environment->min5Vpwr) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(environment->max5Vpwr) & FARM_FIELD_SUPPORTED_BIT);
        create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Average 12v power (W)",
                                                     environment->average12Vpwr, 0.001, false);
        create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Min 12v power (W)", environment->min12VPwr,
                                                     0.001, false);
        create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Max 12v power (W)", environment->max12VPwr,
                                                     0.001, false);
        create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Average 5v power (W)",
                                                     environment->average5Vpwr, 0.001, false);
        create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Min 5v power (W)", environment->min5Vpwr,
                                                     0.001, false);
        create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Max 5v power (W)", environment->max5Vpwr,
                                                     0.001, false);
        if (atleastOnePowerInputbyWattSupported)
        {
            create_Node_For_Time_From_QWord(environmentalNode, "Time Coverage for 12v & 5v power (Hours)",
                                            timeRestrictedRangeMS ^ (BIT63 | BIT62), MICRO_SECONDS_PER_MILLI_SECONDS);
        }

        json_object_object_add(rootObject, "FARM Log Environmental Statistics", environmentalNode);
    }
}

static void create_Node_For_FARM_Reliability_Statistics_Page(json_object*               rootObject,
                                                             farmReliabilityStatistics* reliability,
                                                             uint64_t                   headCount,
                                                             eFARMDriveInterface        driveInterface,
                                                             uint64_t                   timeRestrictedRangeMS)
{
    if (reliability != M_NULLPTR && get_Farm_Qword_Data(reliability->pageNumber) == FARM_PAGE_RELIABILITY_STATS)
    {
        json_object* reliabilityNode = json_object_new_object();

        create_Node_For_UINT64_From_QWord(reliabilityNode, "# DOS Scans Performed", reliability->numDOSScansPerformed);
        create_Node_For_UINT64_From_QWord(reliabilityNode, "# LBAs corrected by ISP",
                                          reliability->numLBAsCorrectedByISP);
        create_Node_For_UINT64_From_QWord(reliabilityNode, "# DOS Scans Performed Actuator 1",
                                          reliability->numDOSScansPerformedActuator1);
        create_Node_For_UINT64_From_QWord(reliabilityNode, "# LBAs corrected by ISP Actuator 1",
                                          reliability->numLBAsCorrectedByISPActuator1);
        create_Node_For_Head_Data_From_QWords(reliabilityNode, "DVGA Skip Write Detect",
                                              reliability->dvgaSkipWriteDetectByHead, headCount,
                                              FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0);
        create_Node_For_Head_Data_From_QWords(reliabilityNode, "RVGA Skip Write Detect",
                                              reliability->rvgaSkipWriteDetectByHead, headCount,
                                              FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0);
        create_Node_For_Head_Data_From_QWords(reliabilityNode, "FVGA Skip Write Detect",
                                              reliability->fvgaSkipWriteDetectByHead, headCount,
                                              FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0);
        create_Node_For_Head_Data_From_QWords(reliabilityNode, "Skip Write Detect Threshold Exceeded",
                                              reliability->skipWriteDetectExceedsThresholdByHead, headCount,
                                              FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0);
        if (driveInterface == FARM_DRIVE_INTERFACE_SATA)
        {
            create_Node_For_UINT64_From_QWord(reliabilityNode, "Read Error Rate", reliability->readErrorRate);
        }
        else
        {
            create_Node_For_UINT64_From_QWord(reliabilityNode, "# Read After Write (RAW) Operations",
                                              reliability->numRAWOperations);
        }
        create_Node_For_UINT64_From_QWord(reliabilityNode, "Read Error Rate Normalized",
                                          reliability->readErrorRateNormalized);
        create_Node_For_UINT64_From_QWord(reliabilityNode, "Read Error Rate Worst Ever",
                                          reliability->readErrorRateWorstEver);
        create_Node_For_UINT64_From_QWord(reliabilityNode, "Seek Error Rate", reliability->seekErrorRate);
        create_Node_For_UINT64_From_QWord(reliabilityNode, "Seek Error Rate Normalized",
                                          reliability->seekErrorRateNormalized);
        create_Node_For_UINT64_From_QWord(reliabilityNode, "Seek Error Rate Worst Ever",
                                          reliability->seekErrorRateWorstEver);
        create_Node_For_UINT64_From_QWord(reliabilityNode, "High Priority Unload Events",
                                          reliability->highPriorityUnloadEvents);
        create_Node_For_Head_Data_With_Delta_Or_Unit_From_QWords(reliabilityNode, "MR Head Resistance", "ohms",
                                                                 reliability->mrHeadResistanceByHead, headCount);
        create_Node_For_Head_Data_With_Delta_Or_Unit_From_QWords(reliabilityNode, "2nd MR Head Resistance", "ohms",
                                                                 reliability->secondHeadMRHeadResistanceByHead,
                                                                 headCount);
        create_Node_For_Head_Data_From_QWords(reliabilityNode, "# of Velocity Observer",
                                              reliability->velocityObserverByHead, headCount,
                                              FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0);
        create_Node_For_Head_Data_From_QWords(reliabilityNode, "# of Velocity No TMD",
                                              reliability->numberOfVelocityObserverNoTMDByHead, headCount,
                                              FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0);
        bool atleastOnevelocityObservedSupported = false;
        for (uint8_t headIter = UINT8_C(0); headIter < FARM_MAX_HEADS && headIter < headCount; ++headIter)
        {
            if ((get_Farm_Status_Byte(reliability->velocityObserverByHead[headIter]) & FARM_FIELD_SUPPORTED_BIT) > 0)
            {
                atleastOnevelocityObservedSupported = true;
                break;
            }
            else if ((get_Farm_Status_Byte(reliability->numberOfVelocityObserverNoTMDByHead[headIter]) &
                      FARM_FIELD_SUPPORTED_BIT) > 0)
            {
                atleastOnevelocityObservedSupported = true;
                break;
            }
        }
        if (atleastOnevelocityObservedSupported)
        {
            create_Node_For_Time_From_QWord(reliabilityNode, "Time Coverage for Velocity Observer (Hours)",
                                            timeRestrictedRangeMS ^ (BIT63 | BIT62), MICRO_SECONDS_PER_MILLI_SECONDS);
        }

        char h2satStatsName[MAX_STATS_NAME_LENGTH][THREE_STATS_IN_ONE] = {"Z1", "Z2", "Z3"};
        create_Node_For_Head_Data_With_3_Stat_From_QWord(reliabilityNode, "H2SAT Trimmed Mean Bits in Error",
                                                         h2satStatsName,
                                                         reliability->currentH2SATtrimmedMeanBitsInErrorByHeadZone,
                                                         headCount, FARM_BY_HEAD_TO_UINT64_FACTOR_FROM_QWORD, 0.10);
        create_Node_For_Head_Data_With_3_Stat_From_QWord(reliabilityNode, "H2SAT Iterations to Converge",
                                                         h2satStatsName,
                                                         reliability->currentH2SATiterationsToConvergeByHeadZone,
                                                         headCount, FARM_BY_HEAD_TO_UINT64_FACTOR_FROM_QWORD, 0.10);
        create_Node_For_Head_Data_From_QWords(reliabilityNode, "Average H2SAT % Codeword at Iteration Level",
                                              reliability->currentH2SATpercentCodewordsPerIterByHeadTZAvg, headCount,
                                              FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0);
        create_Node_For_Head_Data_From_QWords(reliabilityNode, "Average H2SAT Amplitude",
                                              reliability->currentH2SATamplitudeByHeadTZAvg, headCount,
                                              FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0);
        create_Node_For_Head_Data_From_QWords(reliabilityNode, "Average H2SAT Asymmetry",
                                              reliability->currentH2SATasymmetryByHeadTZAvg, headCount,
                                              FARM_BY_HEAD_TO_INT64_FACTOR_FROM_QWORD, 0.10);
        char flyStatsName[MAX_STATS_NAME_LENGTH][THREE_STATS_IN_ONE] = {"OD", "ID", "MD"};
        create_Node_For_Head_Data_With_3_Stat_From_QWord(reliabilityNode, "FAFH Appd Clr Delta (1/1000 A)",
                                                         flyStatsName,
                                                         reliability->appliedFlyHeightClearanceDeltaByHead, headCount,
                                                         FARM_BY_HEAD_TO_INT64_FACTOR_FROM_QWORD, 0.001);
        create_Node_For_UINT64_From_QWord(reliabilityNode, "# Disc Slip Recalibrations Performed",
                                          reliability->numDiscSlipRecalibrationsPerformed);
        create_Node_For_Head_Data_From_QWords(reliabilityNode, "# Reallocated Sectors",
                                              reliability->numReallocatedSectorsByHead, headCount,
                                              FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0);
        create_Node_For_Head_Data_From_QWords(reliabilityNode, "# Reallocated Candidate Sectors",
                                              reliability->numReallocationCandidateSectorsByHead, headCount,
                                              FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0);
        create_Node_For_Bool_From_QWord(reliabilityNode, "Helium Pressure Threshold",
                                        reliability->heliumPressureThresholdTrip, "Tripped", "Not Tripped");
        create_Node_For_Head_Data_From_QWords(reliabilityNode, "# DOS Ought To Scan",
                                              reliability->dosOughtScanCountByHead, headCount,
                                              FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0);
        create_Node_For_Head_Data_From_QWords(reliabilityNode, "# DOS Need To Scan",
                                              reliability->dosNeedToScanCountByHead, headCount,
                                              FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0);
        create_Node_For_Head_Data_From_QWords(reliabilityNode, "# DOS Write Fault Scans",
                                              reliability->dosWriteFaultScansByHead, headCount,
                                              FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0);
        create_Node_For_Head_Data_From_QWords(reliabilityNode, "Write Workload Power-on Time (Hours)",
                                              reliability->writeWorkloadPowerOnTimeByHead, headCount,
                                              FARM_BY_HEAD_TO_TIME_FROM_QWORD, MICRO_SECONDS_PER_SECOND);
        create_Node_For_UINT64_From_QWord(reliabilityNode, "# LBAs Corrected By Parity Sector",
                                          reliability->numLBAsCorrectedByParitySector);
        create_Node_For_UINT64_From_QWord(reliabilityNode, "# LBAs Corrected By Parity Sector Actuator 1",
                                          reliability->numLBAsCorrectedByParitySectorActuator1);
        create_Node_For_UINT64_From_QWord(reliabilityNode, "Primary Super Parity Coverage %",
                                          reliability->superParityCoveragePercent);
        create_Node_For_UINT64_From_QWord(reliabilityNode, "Primary Super Parity Coverage SMR/HSMR-SWR %",
                                          reliability->primarySuperParityCoveragePercentageSMR_HSMR_SWR);
        create_Node_For_UINT64_From_QWord(reliabilityNode, "Primary Super Parity Coverage SMR/HSMR-SWR % Actuator 1",
                                          reliability->primarySuperParityCoveragePercentageSMR_HSMR_SWRActuator1);

        json_object_object_add(rootObject, "FARM Log Reliability Statistics", reliabilityNode);
    }
}

eReturnValues create_JSON_Output_For_FARM(tDevice* device, farmLogData* farmdata, char** jsonFormat)
{
    eReturnValues ret = NOT_SUPPORTED;

    if (farmdata == M_NULLPTR)
    {
        return BAD_PARAMETER;
    }

    if (device->drive_info.drive_type == ATA_DRIVE || device->drive_info.drive_type == SCSI_DRIVE)
    {
        json_object* rootNode = json_object_new_object();

        if (rootNode == M_NULLPTR)
            return MEMORY_FAILURE;

        // Add version information
        json_object_object_add(rootNode, "FARM JSON Version", json_object_new_string(FARM_JSON_VERSION));

        // Add general drive information
        json_object_object_add(rootNode, "Model Name",
                               json_object_new_string(device->drive_info.product_identification));
        json_object_object_add(rootNode, "Serial Number", json_object_new_string(device->drive_info.serialNumber));
        json_object_object_add(rootNode, "Firmware Version",
                               json_object_new_string(device->drive_info.product_revision));

        // This is a very overly simple hack to detect interface.
        // It's a string set to "SAS" or "SATA" so this will work for now - TJE
        eFARMDriveInterface farmInterface = FARM_DRIVE_INTERFACE_SATA;
        uint64_t            drvint        = get_Farm_Qword_Data(farmdata->driveinfo.driveInterface);
        if (M_NULLPTR == memchr(&drvint, 'T', sizeof(drvint)))
        {
            farmInterface = FARM_DRIVE_INTERFACE_SAS;
        }
        else
        {
            farmInterface = FARM_DRIVE_INTERFACE_SATA;
        }
        uint64_t timeRestrictedRangeMS =
            get_Farm_Qword_Data(farmdata->driveinfo.highestPOHForTimeRestrictedParameters) -
            get_Farm_Qword_Data(farmdata->driveinfo.lowestPOHForTimeRestrictedParameters);
        if (timeRestrictedRangeMS == 0 ||
            !(get_Farm_Status_Byte(farmdata->driveinfo.highestPOHForTimeRestrictedParameters) &
              (FARM_FIELD_SUPPORTED_BIT | FARM_FIELD_VALID_BIT)) ||
            !(get_Farm_Status_Byte(farmdata->driveinfo.lowestPOHForTimeRestrictedParameters) &
              (FARM_FIELD_SUPPORTED_BIT | FARM_FIELD_VALID_BIT)))
        {
            timeRestrictedRangeMS |= BIT63 | BIT62;
        }
        uint64_t maxHeads  = get_Farm_Qword_Data(farmdata->header.maxDriveHeadsSupported);
        uint64_t numHeads  = get_Farm_Qword_Data(farmdata->driveinfo.numberOfHeads);
        uint64_t headCount = M_Min(M_Min(numHeads, maxHeads), FARM_MAX_HEADS);

        create_Node_For_FARM_Header_Page(rootNode, &farmdata->header);
        create_Node_For_FARM_Drive_Information_Page(rootNode, &farmdata->driveinfo, farmInterface);
        create_Node_For_FARM_Workload_Statistics_Page(rootNode, &farmdata->workload, timeRestrictedRangeMS);
        create_Node_For_FARM_Error_Statistics_Page(rootNode, &farmdata->error, headCount, farmInterface);
        create_Node_For_FARM_Enviornmental_Statistics_Page(rootNode, &farmdata->environment, farmInterface,
                                                           timeRestrictedRangeMS);
        create_Node_For_FARM_Reliability_Statistics_Page(rootNode, &farmdata->reliability, headCount, farmInterface,
                                                         timeRestrictedRangeMS);

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

        ret = SUCCESS;
    }

    return ret;
}