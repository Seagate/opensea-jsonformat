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
// \file farm_json.c
// \brief This file defines types and functions related to the JSON-based output for FARM.

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

#define MAX_UINT64_TO_HEX_STRING_LENGTH  19
#define MAX_DOA_STRING_LENGTH            14
#define MAX_UINT64_TO_DEC_STRING_LENGTH  21
#define MAX_RECORDING_TYPE_STRING_LENGTH 9
#define MAX_DOUBLE_TO_DEC_STRING_LENGTH  21
#define MAX_BOOL_TO_BOOL_STRING_LENGTH   21
#define MAX_INT64_TO_DEC_STRING_LENGTH   21
#define MAX_DEC_TO_DOUBLE_STRING_LENGTH  22
#define MAX_HEAD_NODE_NAME_LENGTH        9
#define MAX_UINT8_TO_DEC_STRING_LENGTH   4
#define MAX_UPDATED_NODE_NAME_LENGTH     41
#define MAX_ACTUATOR_NODE_NAME_LENGTH    11
#define MAX_FLED_EVENT_NODE_NAME_LENGTH  21

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

#define RETURN_ON_FARM_ERROR(expression)                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        eReturnValues _farmResult = (expression);                                                                      \
        if (_farmResult != SUCCESS)                                                                                    \
        {                                                                                                              \
            return _farmResult;                                                                                        \
        }                                                                                                              \
    } while (0)

/*! Same as RETURN_ON_FARM_ERROR, but used inside the by-head loops, where an early return must release the not-yet
    attached head data node and its enclosing (still unowned) head value array. The head data node is always attached
    to a head node before any later failure point, so a single put pair covers every path. */
#define RETURN_ON_FARM_ERROR_HEAD_DATA(headDataNode, headValueArray, expression)                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        eReturnValues _farmResult = (expression);                                                                      \
        if (_farmResult != SUCCESS)                                                                                    \
        {                                                                                                              \
            json_object_put(headDataNode);                                                                             \
            json_object_put(headValueArray);                                                                           \
            return _farmResult;                                                                                        \
        }                                                                                                              \
    } while (0)

M_NODISCARD static eReturnValues create_Node_For_String_Type_From_Multiple_QWords(json_object* node,
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
                M_IGNORE_SAFE_ERRNO_CALL(
                    safe_memcpy(&farmASCIIData[asciioffset], asciilen - asciioffset, &rawdata, sizeof(uint32_t)),
                    "allocation reserves four bytes for each QWord plus a terminator");
            }
            farmASCIIData[asciilen - 1] = 0;
            remove_Leading_And_Trailing_Whitespace(farmASCIIData);
            eReturnValues ret = add_JSON_Object(node, nodeName, json_object_new_string(farmASCIIData));
            safe_free(&farmASCIIData);
            return ret;
        }
        return MEMORY_FAILURE;
    }
    return SUCCESS;
}

M_NODISCARD static eReturnValues create_Node_For_UINT64_Hex_From_2_QWords(json_object* node,
                                                                          const char*  nodeName,
                                                                          uint64_t*    valueQword)
{
    if (valueQword != M_NULLPTR)
    {
        uint64_t finalData =
            M_DWordsTo8ByteValue(w_swap_32(M_DoubleWord0(valueQword[0])), w_swap_32(M_DoubleWord0(valueQword[1])));
        DECLARE_ZERO_INIT_ARRAY(char, finalASCIIValue, MAX_UINT64_TO_HEX_STRING_LENGTH);
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(finalASCIIValue, MAX_UINT64_TO_HEX_STRING_LENGTH, "0x%016" PRIX64 "", finalData),
            "FARM JSON destination buffer is sized for the fixed display format");
        return add_JSON_Object(node, nodeName, json_object_new_string(finalASCIIValue));
    }
    return SUCCESS;
}

M_NODISCARD static eReturnValues create_Node_For_DOA_From_QWord(json_object* node,
                                                                const char*  nodeName,
                                                                uint64_t     valueQword)
{
    uint32_t rawdata       = M_DoubleWord0(valueQword);
    size_t   asciilen      = (SIZE_T_C(5));
    char*    farmASCIIData = M_REINTERPRET_CAST(char*, safe_calloc(asciilen, sizeof(char)));
    if (farmASCIIData != M_NULLPTR)
    {
        M_IGNORE_SAFE_ERRNO_CALL(safe_memcpy(&farmASCIIData[0], asciilen, &rawdata, sizeof(uint32_t)),
                                 "four-byte raw date field fits in the five-byte temporary buffer");
        DECLARE_ZERO_INIT_ARRAY(char, doaYYStr, 5);
        doaYYStr[0] = '2';
        doaYYStr[1] = '0';
        M_IGNORE_SAFE_ERRNO_CALL(safe_memcpy(&doaYYStr[2], 3, farmASCIIData, 2),
                                 "two date digits fit after the fixed year prefix");
        DECLARE_ZERO_INIT_ARRAY(char, doaWWStr, 3);
        M_IGNORE_SAFE_ERRNO_CALL(safe_memcpy(&doaWWStr[0], 3, &farmASCIIData[2], 2),
                                 "two week digits fit in the three-byte temporary buffer");

        DECLARE_ZERO_INIT_ARRAY(char, finalDOAValue, MAX_DOA_STRING_LENGTH);
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(finalDOAValue, MAX_DOA_STRING_LENGTH, "Week %s, %s", doaWWStr, doaYYStr),
            "FARM JSON destination buffer is sized for the fixed display format");
        eReturnValues ret = add_JSON_Object(node, nodeName, json_object_new_string(finalDOAValue));
        safe_free(&farmASCIIData);
        return ret;
    }
    return MEMORY_FAILURE;
}

M_NODISCARD static eReturnValues create_Node_For_UINT64_From_QWord(json_object* node,
                                                                   const char*  nodeName,
                                                                   uint64_t     valueQword)
{
    uint8_t status = get_Farm_Status_Byte(valueQword);
    if ((status & FARM_FIELD_SUPPORTED_BIT) > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, value, MAX_UINT64_TO_DEC_STRING_LENGTH);
        if ((status & FARM_FIELD_VALID_BIT) > 0)
        {
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(value, MAX_UINT64_TO_DEC_STRING_LENGTH, "%" PRIu64 "",
                                                       get_Farm_Qword_Data(valueQword)),
                                   "FARM JSON destination buffer is sized for the fixed display format");
        }
        else
        {
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(value, MAX_UINT64_TO_DEC_STRING_LENGTH, "Invalid"),
                                   "FARM JSON destination buffer is sized for the fixed display format");
        }
        return add_JSON_Object(node, nodeName, json_object_new_string(value));
    }
    return SUCCESS;
}

M_NODISCARD static eReturnValues create_Node_For_Recording_Type_From_QWord(json_object* node,
                                                                           const char*  nodeName,
                                                                           uint64_t     valueQword)
{
    uint8_t status = get_Farm_Status_Byte(valueQword);
    if ((status & FARM_FIELD_SUPPORTED_BIT) > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, value, MAX_RECORDING_TYPE_STRING_LENGTH);
        if ((status & FARM_FIELD_VALID_BIT) > 0)
        {
            uint64_t recordingType = get_Farm_Qword_Data(valueQword);
            if ((recordingType & FARM_DRIVE_RECORDING_SMR) && (recordingType & FARM_DRIVE_RECORDING_CMR))
            {
                M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(value, MAX_RECORDING_TYPE_STRING_LENGTH, "SMR, CMR"),
                                       "FARM JSON destination buffer is sized for the fixed display format");
            }
            else if (recordingType & FARM_DRIVE_RECORDING_SMR)
            {
                M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(value, MAX_RECORDING_TYPE_STRING_LENGTH, "SMR"),
                                       "FARM JSON destination buffer is sized for the fixed display format");
            }
            else if (recordingType & FARM_DRIVE_RECORDING_CMR)
            {
                M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(value, MAX_RECORDING_TYPE_STRING_LENGTH, "CMR"),
                                       "FARM JSON destination buffer is sized for the fixed display format");
            }
            else
            {
                M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(value, MAX_RECORDING_TYPE_STRING_LENGTH, "Invalid"),
                                       "FARM JSON destination buffer is sized for the fixed display format");
            }
        }
        else
        {
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(value, MAX_RECORDING_TYPE_STRING_LENGTH, "Invalid"),
                                   "FARM JSON destination buffer is sized for the fixed display format");
        }
        return add_JSON_Object(node, nodeName, json_object_new_string(value));
    }
    return SUCCESS;
}

M_NODISCARD static eReturnValues create_Node_For_UINT64_Hex_From_QWord(json_object* node,
                                                                       const char*  nodeName,
                                                                       uint64_t     valueQword)
{
    uint8_t status = get_Farm_Status_Byte(valueQword);
    if ((status & FARM_FIELD_SUPPORTED_BIT) > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, value, MAX_UINT64_TO_HEX_STRING_LENGTH);
        if ((status & FARM_FIELD_VALID_BIT) > 0)
        {
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(value, MAX_UINT64_TO_HEX_STRING_LENGTH, "%" PRIX64 "",
                                                       get_Farm_Qword_Data(valueQword)),
                                   "FARM JSON destination buffer is sized for the fixed display format");
        }
        else
        {
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(value, MAX_UINT64_TO_HEX_STRING_LENGTH, "Invalid"),
                                   "FARM JSON destination buffer is sized for the fixed display format");
        }
        return add_JSON_Object(node, nodeName, json_object_new_string(value));
    }
    return SUCCESS;
}

#define MICRO_SECONDS_PER_HOUR          3600000000.0
#define MICRO_SECONDS_PER_MINUTE        60000000.0
#define MICRO_SECONDS_PER_SECOND        1000000.0
#define MICRO_SECONDS_PER_MILLI_SECONDS 1000.0

M_NODISCARD static eReturnValues create_Node_For_Time_From_QWord(json_object* node,
                                                                 const char*  nodeName,
                                                                 uint64_t     valueQword,
                                                                 double       conversionToMicroseconds)
{
    uint8_t status = get_Farm_Status_Byte(valueQword);
    if ((status & FARM_FIELD_SUPPORTED_BIT) > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, value, MAX_DOUBLE_TO_DEC_STRING_LENGTH);
        if ((status & FARM_FIELD_VALID_BIT) > 0)
        {
            double timeMicroseconds = M_STATIC_CAST(double, get_Farm_Qword_Data(valueQword)) * conversionToMicroseconds;
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(value, MAX_DOUBLE_TO_DEC_STRING_LENGTH, "%0.02f",
                                                       (timeMicroseconds / MICRO_SECONDS_PER_HOUR)),
                                   "FARM JSON destination buffer is sized for the fixed display format");
        }
        else
        {
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(value, MAX_DOUBLE_TO_DEC_STRING_LENGTH, "Invalid"),
                                   "FARM JSON destination buffer is sized for the fixed display format");
        }
        return add_JSON_Object(node, nodeName, json_object_new_string(value));
    }
    return SUCCESS;
}

M_NODISCARD static eReturnValues create_Node_For_Bool_From_QWord(json_object* node,
                                                                 const char*  nodeName,
                                                                 uint64_t     valueQword,
                                                                 const char*  trueString,
                                                                 const char*  falseString)
{
    uint8_t status = get_Farm_Status_Byte(valueQword);
    if ((status & FARM_FIELD_SUPPORTED_BIT) > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, value, MAX_BOOL_TO_BOOL_STRING_LENGTH);
        if ((status & FARM_FIELD_VALID_BIT) > 0)
        {
            if (get_Farm_Qword_Data(valueQword) > 0)
            {
                if (trueString != M_NULLPTR)
                {
                    if (0 != safe_strcpy(value, MAX_BOOL_TO_BOOL_STRING_LENGTH, trueString))
                        M_UNLIKELY
                        {
                            perror("Error coping FARM bool string for JSON output");
                        }
                }
                else
                {
                    if (0 != safe_strcpy(value, MAX_BOOL_TO_BOOL_STRING_LENGTH, "true"))
                        M_UNLIKELY
                        {
                            perror("Error coping FARM bool string for JSON output");
                        }
                }
            }
            else
            {
                if (falseString != M_NULLPTR)
                {
                    if (0 != safe_strcpy(value, MAX_BOOL_TO_BOOL_STRING_LENGTH, falseString))
                        M_UNLIKELY
                        {
                            perror("Error coping FARM bool string for JSON output");
                        }
                }
                else
                {
                    if (0 != safe_strcpy(value, MAX_BOOL_TO_BOOL_STRING_LENGTH, "false"))
                        M_UNLIKELY
                        {
                            perror("Error coping FARM bool string for JSON output");
                        }
                }
            }
        }
        else
        {
            if (0 != safe_strcpy(value, MAX_BOOL_TO_BOOL_STRING_LENGTH, "Invalid"))
                M_UNLIKELY
                {
                    perror("Error coping FARM bool string for JSON output");
                }
        }
        return add_JSON_Object(node, nodeName, json_object_new_string(value));
    }
    return SUCCESS;
}

M_NODISCARD static eReturnValues create_Node_For_INT64_From_QWord(json_object* node,
                                                                  const char*  nodeName,
                                                                  uint64_t     valueQword)
{
    uint8_t status = get_Farm_Status_Byte(valueQword);
    if ((status & FARM_FIELD_SUPPORTED_BIT) > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, value, MAX_INT64_TO_DEC_STRING_LENGTH);
        if ((status & FARM_FIELD_VALID_BIT) > 0)
        {
            int64_t signedval = M_STATIC_CAST(int64_t, get_Farm_Qword_Data(valueQword));
            if (M_Byte6(M_STATIC_CAST(uint64_t, signedval)) & BIT7)
            {
                // sign bit is set. To make sure this converts as we expect it to we need to make sure the int64_t sign
                // bit of the host is set properly.
                signedval = M_STATIC_CAST(int64_t, M_STATIC_CAST(uint64_t, signedval) | UINT64_C(0xFFFF000000000000));
            }
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(value, MAX_INT64_TO_DEC_STRING_LENGTH, "%" PRId64 "", signedval),
                                   "FARM JSON destination buffer is sized for the fixed display format");
        }
        else
        {
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(value, MAX_INT64_TO_DEC_STRING_LENGTH, "Invalid"),
                                   "FARM JSON destination buffer is sized for the fixed display format");
        }
        return add_JSON_Object(node, nodeName, json_object_new_string(value));
    }
    return SUCCESS;
}

M_NODISCARD static eReturnValues create_Node_For_Float_With_Factor_From_QWord(json_object* node,
                                                                              const char*  nodeName,
                                                                              uint64_t     valueQword,
                                                                              double       conversionFactor,
                                                                              bool         isSignedValue)
{
    uint8_t status = get_Farm_Status_Byte(valueQword);
    if ((status & FARM_FIELD_SUPPORTED_BIT) > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, value, MAX_DEC_TO_DOUBLE_STRING_LENGTH);
        bool formatSucceeded = false;
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
                int result      = snprintf_err_handle(value, MAX_DEC_TO_DOUBLE_STRING_LENGTH, "%0.*f", precision,
                                                      M_STATIC_CAST(double, signedval) * conversionFactor);
                formatSucceeded = result >= 0 && C_CAST(size_t, result) < MAX_DEC_TO_DOUBLE_STRING_LENGTH;
            }
            else
            {
                int result =
                    snprintf_err_handle(value, MAX_DEC_TO_DOUBLE_STRING_LENGTH, "%0.*f", precision,
                                        M_STATIC_CAST(double, get_Farm_Qword_Data(valueQword)) * conversionFactor);
                formatSucceeded = result >= 0 && C_CAST(size_t, result) < MAX_DEC_TO_DOUBLE_STRING_LENGTH;
            }
        }
        return add_JSON_Object(node, nodeName, json_object_new_string(formatSucceeded ? value : "Invalid"));
    }
    return SUCCESS;
}

#define FARM_FLOAT_PERCENT_DELTA_FACTORY_BIT BIT0
#define FARM_FLOAT_NEGATIVE_BIT              BIT1

M_NODISCARD static M_INLINE uint8_t get_Farm_Float_Bits(uint64_t floatData)
{
    return M_Byte6(floatData);
}

M_NODISCARD static eReturnValues create_Node_For_Float_From_QWord(json_object* node,
                                                                  const char*  nodeName,
                                                                  uint64_t     valueQword)
{
    uint8_t status = get_Farm_Status_Byte(valueQword);
    if ((status & FARM_FIELD_SUPPORTED_BIT) > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, value, MAX_DEC_TO_DOUBLE_STRING_LENGTH);
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
                M_IGNORE_SAFE_INT_CALL(
                    snprintf_err_handle(value, MAX_DEC_TO_DOUBLE_STRING_LENGTH, "%0.02f", calculated),
                    "FARM JSON destination buffer is sized for the fixed display format");
            }
            else
            {
                M_IGNORE_SAFE_INT_CALL(
                    snprintf_err_handle(value, MAX_DEC_TO_DOUBLE_STRING_LENGTH, "%" PRIu16 "", M_Word0(valueQword)),
                    "FARM JSON destination buffer is sized for the fixed display format");
            }
        }
        else
        {
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(value, MAX_DEC_TO_DOUBLE_STRING_LENGTH, "Invalid"),
                                   "FARM JSON destination buffer is sized for the fixed display format");
        }
        return add_JSON_Object(node, nodeName, json_object_new_string(value));
    }
    return SUCCESS;
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

M_NODISCARD static eReturnValues create_Node_For_Head_Data_From_QWords(json_object*            node,
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
        if (headValueArray == M_NULLPTR)
        {
            return MEMORY_FAILURE;
        }

        uint8_t maxIter = (numberOfHeads < FARM_MAX_HEADS) ? M_STATIC_CAST(uint8_t, numberOfHeads)
                                                           : M_STATIC_CAST(uint8_t, FARM_MAX_HEADS);
        for (uint8_t headIter = UINT8_C(0); headIter < maxIter; ++headIter)
        {
            uint8_t status = get_Farm_Status_Byte(byHead[headIter]);
            if ((status & FARM_FIELD_SUPPORTED_BIT) > 0)
            {
                headDataAdded = true; // atleast one head data has support bit enabled
                // to add value for each head
                json_object* headDataNode = json_object_new_object();
                if (headDataNode == M_NULLPTR)
                {
                    json_object_put(headValueArray);
                    return MEMORY_FAILURE;
                }

                if ((status & FARM_FIELD_VALID_BIT) > 0)
                {
                    switch (outputFormat)
                    {
                    case FARM_BY_HEAD_TO_UINT64_FROM_QWORD:
                        RETURN_ON_FARM_ERROR_HEAD_DATA(
                            headDataNode, headValueArray,
                            create_Node_For_UINT64_From_QWord(headDataNode, "value", byHead[headIter]));
                        break;

                    case FARM_BY_HEAD_TO_UINT64_FACTOR_FROM_QWORD:
                        RETURN_ON_FARM_ERROR_HEAD_DATA(
                            headDataNode, headValueArray,
                            create_Node_For_Float_With_Factor_From_QWord(headDataNode, "value", byHead[headIter],
                                                                         conversionFactor, false));
                        break;

                    case FARM_BY_HEAD_TO_INT64_FROM_QWORD:
                        RETURN_ON_FARM_ERROR_HEAD_DATA(
                            headDataNode, headValueArray,
                            create_Node_For_INT64_From_QWord(headDataNode, "value", byHead[headIter]));
                        break;

                    case FARM_BY_HEAD_TO_INT64_FACTOR_FROM_QWORD:
                        RETURN_ON_FARM_ERROR_HEAD_DATA(
                            headDataNode, headValueArray,
                            create_Node_For_Float_With_Factor_From_QWord(headDataNode, "value", byHead[headIter],
                                                                         conversionFactor, true));
                        break;

                    case FARM_BY_HEAD_TO_UINT64_HEX_FROM_QWORD:
                        RETURN_ON_FARM_ERROR_HEAD_DATA(
                            headDataNode, headValueArray,
                            create_Node_For_UINT64_Hex_From_QWord(headDataNode, "value", byHead[headIter]));
                        break;

                    case FARM_BY_HEAD_TO_FLOAT_FROM_QWORD:
                        RETURN_ON_FARM_ERROR_HEAD_DATA(
                            headDataNode, headValueArray,
                            create_Node_For_Float_From_QWord(headDataNode, "value", byHead[headIter]));
                        break;

                    case FARM_BY_HEAD_TO_TIME_FROM_QWORD:
                        RETURN_ON_FARM_ERROR_HEAD_DATA(
                            headDataNode, headValueArray,
                            create_Node_For_Time_From_QWord(headDataNode, "value", byHead[headIter], conversionFactor));
                        break;

                    case FARM_BY_HEAD_TO_GPES_FROM_QWORD:
                    {
                        // add health
                        uint8_t health = M_Byte0(get_Farm_Qword_Data(byHead[headIter]));
                        DECLARE_ZERO_INIT_ARRAY(char, healthValue, MAX_UINT8_TO_DEC_STRING_LENGTH);
                        M_IGNORE_SAFE_INT_CALL(
                            snprintf_err_handle(healthValue, MAX_UINT8_TO_DEC_STRING_LENGTH, "%" PRId8 "", health),
                            "FARM JSON destination buffer is sized for the fixed display format");
                        RETURN_ON_FARM_ERROR_HEAD_DATA(
                            headDataNode, headValueArray,
                            add_JSON_Object(headDataNode, "health", json_object_new_string(healthValue)));

                        // add timestamp
                        uint64_t timestamp = get_bit_range_uint64(get_Farm_Qword_Data(byHead[headIter]), 39, 8);
                        DECLARE_ZERO_INIT_ARRAY(char, timestampValue, MAX_UINT64_TO_DEC_STRING_LENGTH);
                        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(timestampValue, MAX_UINT64_TO_DEC_STRING_LENGTH,
                                                                   "%" PRId64 "", timestamp),
                                               "FARM JSON destination buffer is sized for the fixed display format");
                        RETURN_ON_FARM_ERROR_HEAD_DATA(
                            headDataNode, headValueArray,
                            add_JSON_Object(headDataNode, "timestamp", json_object_new_string(timestampValue)));
                    }
                    break;

                    default:
                        // invalid format
                        RETURN_ON_FARM_ERROR_HEAD_DATA(
                            headDataNode, headValueArray,
                            add_JSON_Object(headDataNode, "value", json_object_new_string("Invalid")));
                        break;
                    }
                }
                else
                {
                    // invalid data
                    RETURN_ON_FARM_ERROR_HEAD_DATA(
                        headDataNode, headValueArray,
                        add_JSON_Object(headDataNode, "value", json_object_new_string("Invalid")));
                }

                // create new node, name if Head #?
                json_object* head = json_object_new_object();
                if (head == M_NULLPTR)
                {
                    json_object_put(headDataNode);
                    json_object_put(headValueArray);
                    return MEMORY_FAILURE;
                }
                DECLARE_ZERO_INIT_ARRAY(char, headNodeName, MAX_HEAD_NODE_NAME_LENGTH);
                M_IGNORE_SAFE_INT_CALL(
                    snprintf_err_handle(headNodeName, MAX_HEAD_NODE_NAME_LENGTH, "Head %" PRIu8, (headIter + 1)),
                    "FARM JSON destination buffer is sized for the fixed display format");
                if (add_JSON_Object(head, headNodeName, headDataNode) != 0)
                {
                    json_object_put(head);
                    json_object_put(headValueArray);
                    return MEMORY_FAILURE;
                }

                // Add it into array
                if (add_JSON_Array_Element(headValueArray, head) != 0)
                {
                    json_object_put(headValueArray);
                    return MEMORY_FAILURE;
                }
            }
        }

        if (headDataAdded)
        {
            if (add_JSON_Object(node, nodeName, headValueArray) != 0)
            {
                json_object_put(headValueArray);
                return MEMORY_FAILURE;
            }
        }
        else
        {
            json_object_put(headValueArray);
        }
    }
    return SUCCESS;
}

M_NODISCARD static eReturnValues create_Node_For_Head_Data_With_Delta_Or_Unit_From_QWords(json_object* node,
                                                                                          const char*  nodeName,
                                                                                          const char*  unitName,
                                                                                          uint64_t*    byHead,
                                                                                          uint64_t     numberOfHeads)
{
    DECLARE_ZERO_INIT_ARRAY(char, updatedNodeName, MAX_UPDATED_NODE_NAME_LENGTH);
    if (get_Farm_Float_Bits(byHead[0]) & FARM_FLOAT_PERCENT_DELTA_FACTORY_BIT ||
        get_Farm_Float_Bits(byHead[0]) & FARM_FLOAT_NEGATIVE_BIT)
    {
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(updatedNodeName, MAX_UPDATED_NODE_NAME_LENGTH, "%s (%% delta)", nodeName),
            "FARM JSON destination buffer is sized for the fixed display format");
        return create_Node_For_Head_Data_From_QWords(node, updatedNodeName, byHead, numberOfHeads,
                                                     FARM_BY_HEAD_TO_FLOAT_FROM_QWORD, 0.0);
    }
    else
    {
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(updatedNodeName, MAX_UPDATED_NODE_NAME_LENGTH, "%s (%s)", nodeName, unitName),
            "FARM JSON destination buffer is sized for the fixed display format");
        return create_Node_For_Head_Data_From_QWords(node, updatedNodeName, byHead, numberOfHeads,
                                                     FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.001);
    }
    return SUCCESS;
}

#define THREE_STATS_IN_ONE    3
#define MAX_STATS_NAME_LENGTH 3
M_NODISCARD static eReturnValues create_Node_For_Head_Data_With_3_Stat_From_QWord(
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
        if (headValueArray == M_NULLPTR)
        {
            return MEMORY_FAILURE;
        }

        uint8_t maxIter = (numberOfHeads < FARM_MAX_HEADS) ? M_STATIC_CAST(uint8_t, numberOfHeads)
                                                           : M_STATIC_CAST(uint8_t, FARM_MAX_HEADS);
        for (uint8_t headIter = UINT8_C(0); headIter < maxIter; ++headIter)
        {
            uint8_t status = get_Farm_Status_Byte(byHead[headIter][0]);
            if ((status & FARM_FIELD_SUPPORTED_BIT) > 0)
            {
                headDataAdded = true; // atleast one head data has support bit enabled
                // to add value for each head
                json_object* headDataNode = json_object_new_object();
                if (headDataNode == M_NULLPTR)
                {
                    json_object_put(headValueArray);
                    return MEMORY_FAILURE;
                }

                if ((status & FARM_FIELD_VALID_BIT) > 0)
                {
                    switch (outputFormat)
                    {
                    case FARM_BY_HEAD_TO_UINT64_FACTOR_FROM_QWORD:
                        for (uint8_t statNum = 0; statNum < THREE_STATS_IN_ONE; ++statNum)
                        {
                            RETURN_ON_FARM_ERROR_HEAD_DATA(headDataNode, headValueArray,
                                                           create_Node_For_Float_With_Factor_From_QWord(
                                                               headDataNode, statisticsName[statNum],
                                                               byHead[headIter][statNum], conversionFactor, false));
                        }
                        break;

                    case FARM_BY_HEAD_TO_INT64_FACTOR_FROM_QWORD:
                        for (uint8_t statNum = 0; statNum < THREE_STATS_IN_ONE; ++statNum)
                        {
                            RETURN_ON_FARM_ERROR_HEAD_DATA(headDataNode, headValueArray,
                                                           create_Node_For_Float_With_Factor_From_QWord(
                                                               headDataNode, statisticsName[statNum],
                                                               byHead[headIter][statNum], conversionFactor, true));
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
                        RETURN_ON_FARM_ERROR_HEAD_DATA(
                            headDataNode, headValueArray,
                            add_JSON_Object(headDataNode, "value", json_object_new_string("Invalid")));
                        break;
                    }
                }
                else
                {
                    // invalid data
                    RETURN_ON_FARM_ERROR_HEAD_DATA(
                        headDataNode, headValueArray,
                        add_JSON_Object(headDataNode, "value", json_object_new_string("Invalid")));
                }

                // create new node, name if Head #?
                json_object* head = json_object_new_object();
                if (head == M_NULLPTR)
                {
                    json_object_put(headDataNode);
                    json_object_put(headValueArray);
                    return MEMORY_FAILURE;
                }
                DECLARE_ZERO_INIT_ARRAY(char, headNodeName, MAX_HEAD_NODE_NAME_LENGTH);
                M_IGNORE_SAFE_INT_CALL(
                    snprintf_err_handle(headNodeName, MAX_HEAD_NODE_NAME_LENGTH, "Head %" PRIu8, (headIter + 1)),
                    "FARM JSON destination buffer is sized for the fixed display format");
                if (add_JSON_Object(head, headNodeName, headDataNode) != 0)
                {
                    json_object_put(head);
                    json_object_put(headValueArray);
                    return MEMORY_FAILURE;
                }

                // Add it into array
                if (add_JSON_Array_Element(headValueArray, head) != 0)
                {
                    json_object_put(headValueArray);
                    return MEMORY_FAILURE;
                }
            }
        }

        if (headDataAdded)
        {
            if (add_JSON_Object(node, nodeName, headValueArray) != 0)
            {
                json_object_put(headValueArray);
                return MEMORY_FAILURE;
            }
        }
        else
        {
            json_object_put(headValueArray);
        }
    }
    return SUCCESS;
}

M_NODISCARD static eReturnValues create_Node_For_FARM_Flash_LED_Actuator(json_object*    actuatorArray,
                                                                         const char*     actuatorName,
                                                                         uint64_t        totalFlashLEDEvents,
                                                                         uint64_t        lastFLEDIndex,
                                                                         const uint64_t* lastFLEDEvents,
                                                                         const uint64_t* lastReadWriteRetryEvents,
                                                                         const uint64_t* timestampsOfLastFLEDs,
                                                                         const uint64_t* powerCyclesOfLastFLEDs,
                                                                         bool*           addedActuator)
{
    uint8_t status = get_Farm_Status_Byte(totalFlashLEDEvents);
    if (!(status & FARM_FIELD_SUPPORTED_BIT) || !(status & FARM_FIELD_VALID_BIT))
    {
        return SUCCESS;
    }

    json_object* outerNode = json_object_new_object();
    if (outerNode == M_NULLPTR)
    {
        return MEMORY_FAILURE;
    }
    RETURN_ON_FARM_ERROR(add_JSON_Array_Element(actuatorArray, outerNode));

    json_object* actuatorNode = json_object_new_object();
    if (actuatorNode == M_NULLPTR)
    {
        return MEMORY_FAILURE;
    }
    RETURN_ON_FARM_ERROR(add_JSON_Object(outerNode, actuatorName, actuatorNode));
    *addedActuator = true;

    RETURN_ON_FARM_ERROR(
        create_Node_For_UINT64_From_QWord(actuatorNode, "Total Flash LED Events", totalFlashLEDEvents));

    status = get_Farm_Status_Byte(lastFLEDIndex);
    if (status & FARM_FIELD_SUPPORTED_BIT && status & FARM_FIELD_VALID_BIT)
    {
        json_object* eventArray = json_object_new_array();
        if (eventArray == M_NULLPTR)
        {
            return MEMORY_FAILURE;
        }
        RETURN_ON_FARM_ERROR(add_JSON_Object(actuatorNode, "Flash LED Events", eventArray));

        int64_t index      = M_STATIC_CAST(int64_t, get_Farm_Qword_Data(lastFLEDIndex));
        uint8_t eventCount = UINT8_C(0);
        while (eventCount < FARM_FLED_EVENTS && index < FARM_FLED_EVENTS && index >= INT64_C(0))
        {
            json_object* event = json_object_new_object();
            if (event == M_NULLPTR)
            {
                return MEMORY_FAILURE;
            }
            RETURN_ON_FARM_ERROR(add_JSON_Array_Element(eventArray, event));

            json_object* fLEDNode = json_object_new_object();
            if (fLEDNode == M_NULLPTR)
            {
                return MEMORY_FAILURE;
            }
            DECLARE_ZERO_INIT_ARRAY(char, eventNodeName, MAX_FLED_EVENT_NODE_NAME_LENGTH);
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(eventNodeName, MAX_FLED_EVENT_NODE_NAME_LENGTH,
                                                       "Flash LED Event # %" PRIu8, eventCount + UINT8_C(1)),
                                   "FARM JSON destination buffer is sized for the fixed display format");
            RETURN_ON_FARM_ERROR(add_JSON_Object(event, eventNodeName, fLEDNode));

            const uint64_t eventValues[] = {lastFLEDEvents[index], lastReadWriteRetryEvents[index],
                                            timestampsOfLastFLEDs[index], powerCyclesOfLastFLEDs[index]};
            const char*    eventNames[]  = {"FLED", "RW Retry", "Timestamp", "Power Cycle"};
            for (size_t valueIndex = SIZE_T_C(0); valueIndex < SIZE_T_C(4); ++valueIndex)
            {
                DECLARE_ZERO_INIT_ARRAY(char, value, MAX_UINT64_TO_DEC_STRING_LENGTH);
                uint8_t  valueStatus = get_Farm_Status_Byte(eventValues[valueIndex]);
                uint64_t valueData   = get_Farm_Qword_Data(eventValues[valueIndex]);
                if ((valueStatus & FARM_FIELD_SUPPORTED_BIT) > UINT8_C(0) &&
                    (valueStatus & FARM_FIELD_VALID_BIT) > UINT8_C(0))
                {
                    M_IGNORE_SAFE_INT_CALL(
                        snprintf_err_handle(value, MAX_UINT64_TO_DEC_STRING_LENGTH, "%" PRIu64, valueData),
                        "FARM JSON destination buffer is sized for the fixed display format");
                }
                else
                {
                    M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(value, MAX_UINT64_TO_DEC_STRING_LENGTH, "-"),
                                           "FARM JSON destination buffer is sized for the fixed display format");
                }
                RETURN_ON_FARM_ERROR(add_JSON_Object(fLEDNode, eventNames[valueIndex], json_object_new_string(value)));
            }

            --index;
            if (index < INT64_C(0))
            {
                index = FARM_FLED_EVENTS;
            }
            ++eventCount;
        }
    }

    return SUCCESS;
}

M_NODISCARD static eReturnValues create_Node_For_FARM_Flash_LED_Events(json_object* node, farmErrorStatistics* error)
{
    if (error != M_NULLPTR && get_Farm_Qword_Data(error->pageNumber) == FARM_PAGE_ERROR_STATS)
    {
        json_object* actuatorArray = json_object_new_array();
        if (actuatorArray == M_NULLPTR)
        {
            return MEMORY_FAILURE;
        }

        bool          addedActuator = false;
        eReturnValues ret           = create_Node_For_FARM_Flash_LED_Actuator(
            actuatorArray, "Actuator 0", error->totalFlashLEDEvents, error->lastFLEDIndex, error->last8FLEDEvents,
            error->last8ReadWriteRetryEvents, error->timestampOfLast8FLEDs, error->powerCycleOfLast8FLEDs,
            &addedActuator);
        if (ret == SUCCESS)
        {
            ret = create_Node_For_FARM_Flash_LED_Actuator(
                actuatorArray, "Actuator 1", error->totalFlashLEDEventsActuator1, error->lastFLEDIndexActuator1,
                error->last8FLEDEventsActuator1, error->last8ReadWriteRetryEventsActuator1,
                error->timestampOfLast8FLEDsActuator1, error->powerCycleOfLast8FLEDsActuator1, &addedActuator);
        }
        if (ret != SUCCESS)
        {
            json_object_put(actuatorArray);
            return ret;
        }

        if (addedActuator)
        {
            return add_JSON_Object(node, "Flash LED Events", actuatorArray);
        }
        json_object_put(actuatorArray);
    }
    return SUCCESS;
}

M_NODISCARD static eReturnValues create_Node_For_FARM_Header_Page(json_object* rootObject, farmHeader* header)
{
    json_object* headerNode = json_object_new_object();
    if (headerNode == M_NULLPTR)
    {
        return MEMORY_FAILURE;
    }
    RETURN_ON_FARM_ERROR(add_JSON_Object(rootObject, "FARM Log Header", headerNode));

    DECLARE_ZERO_INIT_ARRAY(char, value, MAX_UINT64_TO_DEC_STRING_LENGTH + MAX_UINT64_TO_DEC_STRING_LENGTH + 1);
    M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(value,
                                               (MAX_UINT64_TO_DEC_STRING_LENGTH + MAX_UINT64_TO_DEC_STRING_LENGTH + 1),
                                               "%" PRIu64 ".%" PRIu64 "", get_Farm_Qword_Data(header->majorVersion),
                                               get_Farm_Qword_Data(header->minorVersion)),
                           "FARM JSON destination buffer is sized for the fixed display format");
    RETURN_ON_FARM_ERROR(add_JSON_Object(headerNode, "FARM Version", json_object_new_string(value)));

    return SUCCESS;
}

M_NODISCARD static eReturnValues create_Node_For_FARM_Drive_Information_Page(json_object*        rootObject,
                                                                             farmDriveInfo*      driveInfo,
                                                                             eFARMDriveInterface farmInterface)
{
    if (driveInfo != M_NULLPTR && get_Farm_Qword_Data(driveInfo->pageNumber) == FARM_PAGE_DRIVE_INFO)
    {
        json_object* driveInfoNode = json_object_new_object();
        if (driveInfoNode == M_NULLPTR)
        {
            return MEMORY_FAILURE;
        }
        RETURN_ON_FARM_ERROR(add_JSON_Object(rootObject, "FARM Log Drive Information", driveInfoNode));

        RETURN_ON_FARM_ERROR(create_Node_For_String_Type_From_Multiple_QWords(
            driveInfoNode, "Model Number", &driveInfo->modelNumber[0], FARM_DRIVE_INFO_MN_ASCII_LEN));
        RETURN_ON_FARM_ERROR(create_Node_For_String_Type_From_Multiple_QWords(
            driveInfoNode, "Serial Number", &driveInfo->sn[0], FARM_DRIVE_INFO_SN_ASCII_LEN));
        RETURN_ON_FARM_ERROR(create_Node_For_String_Type_From_Multiple_QWords(
            driveInfoNode, "Firmware Revision", &driveInfo->fwrev[0], FARM_DRIVE_INFO_FWREV_ASCII_LEN));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_Hex_From_2_QWords(driveInfoNode, "World Wide Name", &driveInfo->wwn[0]));
        RETURN_ON_FARM_ERROR(
            create_Node_For_DOA_From_QWord(driveInfoNode, "Date Of Assembly", driveInfo->dateOfAssembly));
        RETURN_ON_FARM_ERROR(add_JSON_Object(driveInfoNode, "Drive Interface",
                                             farmInterface == FARM_DRIVE_INTERFACE_SATA
                                                 ? json_object_new_string("SATA")
                                                 : json_object_new_string("SAS")));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(driveInfoNode, "Device Capacity (LBAs)", driveInfo->driveCapacity));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(driveInfoNode, "Number of LBAs (HSMR SWR capacity)",
                                                               driveInfo->numberOfLBAs));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(driveInfoNode, "Physical Sector Size (B)",
                                                               driveInfo->physicalSectorSize));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(driveInfoNode, "Logical Sector Size (B)", driveInfo->logicalSectorSize));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(driveInfoNode, "Device Buffer Size (B)", driveInfo->deviceBufferSize));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(driveInfoNode, "Number Of Heads", driveInfo->numberOfHeads));
        RETURN_ON_FARM_ERROR(create_Node_For_Recording_Type_From_QWord(driveInfoNode, "Drive Recording Type",
                                                                       driveInfo->driveRecordingType));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_Hex_From_QWord(driveInfoNode, "Form Factor", driveInfo->deviceFormFactor));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(driveInfoNode, "Rotation Rate", driveInfo->rotationRate));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_Hex_From_QWord(driveInfoNode, "ATA Security State", driveInfo->ataSecurityState));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_Hex_From_QWord(driveInfoNode, "ATA Features Supported",
                                                                   driveInfo->ataFeaturesSupported));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_Hex_From_QWord(driveInfoNode, "ATA Features Enabled",
                                                                   driveInfo->ataFeaturesEnabled));
        RETURN_ON_FARM_ERROR(create_Node_For_Time_From_QWord(driveInfoNode, "Power On Hours", driveInfo->powerOnHours,
                                                             MICRO_SECONDS_PER_HOUR));
        RETURN_ON_FARM_ERROR(create_Node_For_Time_From_QWord(driveInfoNode, "Spindle Power On Hours",
                                                             driveInfo->spindlePowerOnHours, MICRO_SECONDS_PER_HOUR));
        RETURN_ON_FARM_ERROR(create_Node_For_Time_From_QWord(driveInfoNode, "Head Flight Hours",
                                                             driveInfo->headFlightHours, MICRO_SECONDS_PER_HOUR));
        RETURN_ON_FARM_ERROR(create_Node_For_Time_From_QWord(driveInfoNode, "Head Flight Hours, Actuator 1",
                                                             driveInfo->headFlightHoursActuator1,
                                                             MICRO_SECONDS_PER_HOUR));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(driveInfoNode, "Head Load Events", driveInfo->headLoadEvents));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(driveInfoNode, "Head Load Events, Actuator 1",
                                                               driveInfo->headLoadEventsActuator1));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(driveInfoNode, "Power Cycle Count", driveInfo->powerCycleCount));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(driveInfoNode, "Hardware Reset Count", driveInfo->hardwareResetCount));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(driveInfoNode, "Spin up time (ms)", driveInfo->spinUpTimeMilliseconds));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(driveInfoNode, "Time to ready, last power cycle (ms)",
                                                               driveInfo->timeToReadyOfLastPowerCycle));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(driveInfoNode, "Time in staggered spinup, last power on sequence (ms)",
                                              driveInfo->timeDriveHeldInStaggeredSpinDuringLastPowerOnSequence));
        if (farmInterface == FARM_DRIVE_INTERFACE_SAS)
        {
            RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(driveInfoNode, "NVC Status at Power On",
                                                                   driveInfo->nvcStatusOnPoweron));
            RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(driveInfoNode,
                                                                   "Time Availabe to Save User Data To NV Mem",
                                                                   driveInfo->timeAvailableToSaveUDToNVMem)); // 100us
        }
        RETURN_ON_FARM_ERROR(create_Node_For_Time_From_QWord(driveInfoNode, "Lowest POH timestamp (Hours)",
                                                             driveInfo->lowestPOHForTimeRestrictedParameters,
                                                             MICRO_SECONDS_PER_MILLI_SECONDS));
        RETURN_ON_FARM_ERROR(create_Node_For_Time_From_QWord(driveInfoNode, "Highest POH timestamp (Hours)",
                                                             driveInfo->highestPOHForTimeRestrictedParameters,
                                                             MICRO_SECONDS_PER_MILLI_SECONDS));
        RETURN_ON_FARM_ERROR(create_Node_For_Bool_From_QWord(
            driveInfoNode, "Depopulation Status", driveInfo->isDriveDepopulated, "Depopulated", "Not Depopulated"));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_Hex_From_QWord(driveInfoNode, "Depopulation Head Mask",
                                                                   driveInfo->depopulationHeadMask));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_Hex_From_QWord(driveInfoNode, "Regeneration Head Mask", driveInfo->regenHeadMask));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_From_QWords(
            driveInfoNode, "Physical Element Status (Health/Timestamp)", driveInfo->getPhysicalElementStatusByHead,
            driveInfo->numberOfHeads, FARM_BY_HEAD_TO_GPES_FROM_QWORD, 0.0));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(driveInfoNode,
                                                               "Max # Available Disc Sectors for Reassignment",
                                                               driveInfo->maxAvailableSectorsForReassignment));
        RETURN_ON_FARM_ERROR(create_Node_For_Bool_From_QWord(driveInfoNode, "HAMR Data Protect Status",
                                                             driveInfo->hamrDataProtectStatus, "Data Protect",
                                                             "No Data Protect"));
        RETURN_ON_FARM_ERROR(create_Node_For_Time_From_QWord(driveInfoNode, "POH of Most Recent FARM TS Frame",
                                                             driveInfo->pohOfMostRecentTimeseriesFrame,
                                                             MICRO_SECONDS_PER_MILLI_SECONDS));
        RETURN_ON_FARM_ERROR(create_Node_For_Time_From_QWord(driveInfoNode, "POH of 2nd Most Recent FARM TS Frame",
                                                             driveInfo->pohOfSecondMostRecentTimeseriesFrame,
                                                             MICRO_SECONDS_PER_MILLI_SECONDS));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(driveInfoNode, "Seq or Before Req for Active Zone Config",
                                              driveInfo->sequentialOrBeforeWriteRequiredForActiveZoneConfiguration));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(driveInfoNode, "Seq Write Req Active Zone Config",
                                              driveInfo->sequentialWriteRequiredForActiveZoneConfiguration));
    }

    return SUCCESS;
}

M_NODISCARD static eReturnValues create_Node_For_FARM_Workload_Statistics_Page(json_object*  rootObject,
                                                                               farmWorkload* workload,
                                                                               uint64_t      timeRestrictedRangeMS)
{
    if (workload != M_NULLPTR && get_Farm_Qword_Data(workload->pageNumber) == FARM_PAGE_WORKLOAD)
    {
        json_object* workloadNode = json_object_new_object();
        if (workloadNode == M_NULLPTR)
        {
            return MEMORY_FAILURE;
        }
        RETURN_ON_FARM_ERROR(add_JSON_Object(rootObject, "FARM Log Workload Statistics", workloadNode));

        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(workloadNode, "Rated Workload (%)", workload->ratedWorkloadPercentage));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(workloadNode, "Total # of Read Commands", workload->totalReadCommands));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(workloadNode, "Total # of Write Commands", workload->totalWriteCommands));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "Total # of Random Read Commands",
                                                               workload->totalRandomReadCommands));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "Total # of Random Write Commands",
                                                               workload->totalRandomWriteCommands));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(workloadNode, "Total # of Other Commands", workload->totalOtherCommands));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(workloadNode, "LBAs Written", workload->logicalSectorsWritten));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(workloadNode, "LBAs Read", workload->logicalSectorsRead));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "# of Dither events in power cycle",
                                                               workload->numberOfDitherEventsInCurrentPowerCycle));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(workloadNode, "# of Dither events in power cycle, Actuator 1",
                                              workload->numberOfDitherEventsInCurrentPowerCycleActuator1));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(workloadNode, "# dither pause - random workloads in power cycle",
                                              workload->numberDitherHeldOffDueToRandomWorkloadsInCurrentPowerCycle));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(
            workloadNode, "# dither pause - random workloads in power cycle, Actuator 1",
            workload->numberDitherHeldOffDueToRandomWorkloadsInCurrentPowerCycleActuator1));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(
            workloadNode, "# dither pause - sequential workloads in power cycle",
            workload->numberDitherHeldOffDueToSequentialWorkloadsInCurrentPowerCycle));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(
            workloadNode, "# dither pause - sequential workloads in power cycle, Actuator 1",
            workload->numberDitherHeldOffDueToSequentialWorkloadsInCurrentPowerCycleActuator1));

        bool atleastOneReadWriteByLBASupported =
            (get_Farm_Status_Byte(workload->numReadsInLBA0To3125PercentRange) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numReadsInLBA3125To25PercentRange) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numReadsInLBA25To50PercentRange) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numReadsInLBA50To100PercentRange) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numWritesInLBA0To3125PercentRange) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numWritesInLBA3125To25PercentRange) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numWritesInLBA25To50PercentRange) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(workload->numWritesInLBA50To100PercentRange) & FARM_FIELD_SUPPORTED_BIT);
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(
            workloadNode, "# of read commands between 0-3.125% LBA space", workload->numReadsInLBA0To3125PercentRange));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode,
                                                               "# of read commands between 3.125-25% LBA space",
                                                               workload->numReadsInLBA3125To25PercentRange));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(
            workloadNode, "# of read commands between 25-50% LBA space", workload->numReadsInLBA25To50PercentRange));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(
            workloadNode, "# of read commands between 50-100% LBA space", workload->numReadsInLBA50To100PercentRange));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode,
                                                               "# of write commands between 0-3.125% LBA space",
                                                               workload->numWritesInLBA0To3125PercentRange));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode,
                                                               "# of write commands between 3.125-25% LBA space",
                                                               workload->numWritesInLBA3125To25PercentRange));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(
            workloadNode, "# of write commands between 25-50% LBA space", workload->numWritesInLBA25To50PercentRange));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode,
                                                               "# of write commands between 50-100% LBA space",
                                                               workload->numWritesInLBA50To100PercentRange));
        if (atleastOneReadWriteByLBASupported)
        {
            RETURN_ON_FARM_ERROR(create_Node_For_Time_From_QWord(
                workloadNode, "Time that Commands Cover (by LBA Space) (Hours)",
                timeRestrictedRangeMS ^ (BIT63 | BIT62), MICRO_SECONDS_PER_MILLI_SECONDS));
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
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "# of read commands with xfer <= 16KiB",
                                                               workload->numReadsOfXferLenLT16KB));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(
            workloadNode, "# of read commands with xfer 16Kib - 512KiB", workload->numReadsOfXferLen16KBTo512KB));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(
            workloadNode, "# of read commands with xfer 512KiB - 2MiB", workload->numReadsOfXferLen512KBTo2MB));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "# of read commands with xfer > 2MiB",
                                                               workload->numReadsOfXferLenGT2MB));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "# of write commands with xfer <= 16KiB",
                                                               workload->numWritesOfXferLenLT16KB));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(
            workloadNode, "# of write commands with xfer 16Kib - 512KiB", workload->numWritesOfXferLen16KBTo512KB));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(
            workloadNode, "# of write commands with xfer 512KiB - 2MiB", workload->numWritesOfXferLen512KBTo2MB));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "# of write commands with xfer > 2MiB",
                                                               workload->numWritesOfXferLenGT2MB));
        if (atleastOneReadWriteByXferSupported)
        {
            RETURN_ON_FARM_ERROR(create_Node_For_Time_From_QWord(
                workloadNode, "Time that Commands Cover (by xfer) (Hours)", timeRestrictedRangeMS ^ (BIT63 | BIT62),
                MICRO_SECONDS_PER_MILLI_SECONDS));
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
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "Queue Depth = 1 in 30s intervals",
                                                               workload->countQD1at30sInterval));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "Queue Depth = 2 in 30s intervals",
                                                               workload->countQD2at30sInterval));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "Queue Depth 3-4 in 30s intervals",
                                                               workload->countQD3To4at30sInterval));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "Queue Depth 5-8 in 30s intervals",
                                                               workload->countQD5To8at30sInterval));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "Queue Depth 9-16 in 30s intervals",
                                                               workload->countQD9To16at30sInterval));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "Queue Depth 17-32 in 30s intervals",
                                                               workload->countQD17To32at30sInterval));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "Queue Depth 33-64 in 30s intervals",
                                                               workload->countQD33To64at30sInterval));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "Queue Depth > 64 in 30s intervals",
                                                               workload->countGTQD64at30sInterval));
        if (atleastOneQueueDepthSupported)
        {
            RETURN_ON_FARM_ERROR(create_Node_For_Time_From_QWord(workloadNode, "Time that Queue Bins Cover (Hours)",
                                                                 timeRestrictedRangeMS ^ (BIT63 | BIT62),
                                                                 MICRO_SECONDS_PER_MILLI_SECONDS));
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
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "# of reads of xfer bin 4, last 3 SSF",
                                                               workload->numReadsXferLenBin4Last3SMARTSummaryFrames));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "# of reads of xfer bin 5, last 3 SSF",
                                                               workload->numReadsXferLenBin5Last3SMARTSummaryFrames));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "# of reads of xfer bin 6, last 3 SSF",
                                                               workload->numReadsXferLenBin6Last3SMARTSummaryFrames));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "# of reads of xfer bin 7, last 3 SSF",
                                                               workload->numReadsXferLenBin7Last3SMARTSummaryFrames));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "# of writes of xfer bin 4, last 3 SSF",
                                                               workload->numWritesXferLenBin4Last3SMARTSummaryFrames));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "# of writes of xfer bin 5, last 3 SSF",
                                                               workload->numWritesXferLenBin5Last3SMARTSummaryFrames));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "# of writes of xfer bin 6, last 3 SSF",
                                                               workload->numWritesXferLenBin6Last3SMARTSummaryFrames));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(workloadNode, "# of writes of xfer bin 7, last 3 SSF",
                                                               workload->numWritesXferLenBin7Last3SMARTSummaryFrames));
        if (atleastOneReadWriteByXferInLastSSFSupported)
        {
            RETURN_ON_FARM_ERROR(create_Node_For_Time_From_QWord(workloadNode, "Time that XFer Bins Cover (Hours)",
                                                                 timeRestrictedRangeMS ^ (BIT63 | BIT62),
                                                                 MICRO_SECONDS_PER_MILLI_SECONDS));
        }
    }
    return SUCCESS;
}

M_NODISCARD static eReturnValues create_Node_For_FARM_Error_Statistics_Page(json_object*         rootObject,
                                                                            farmErrorStatistics* error,
                                                                            uint64_t             headCount,
                                                                            eFARMDriveInterface  driveInterface)
{
    if (error != M_NULLPTR && get_Farm_Qword_Data(error->pageNumber) == FARM_PAGE_ERROR_STATS)
    {
        json_object* errorNode = json_object_new_object();
        if (errorNode == M_NULLPTR)
        {
            return MEMORY_FAILURE;
        }
        RETURN_ON_FARM_ERROR(add_JSON_Object(rootObject, "FARM Log Error Statistics", errorNode));

        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode, "# of Unrecoverable Read Errors",
                                                               error->numberOfUnrecoverableReadErrors));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode, "# of Unrecoverable Write Errors",
                                                               error->numberOfUnrecoverableWriteErrors));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode, "# of Reallocated Sectors",
                                                               error->numberOfReallocatedSectors));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode, "# of Reallocated Sectors, Actuator 1",
                                                               error->numberOfReallocatedSectorsActuator1));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode, "# of Read Recovery Attempts",
                                                               error->numberOfReadRecoveryAttempts));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode, "# of Mechanical Start Retries",
                                                               error->numberOfMechanicalStartRetries));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode, "# of Reallocation Candidate Sectors",
                                                               error->numberOfReallocationCandidateSectors));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode,
                                                               "# of Reallocated Candidate Sectors, Actuator 1",
                                                               error->numberOfReallocationCandidateSectorsActuator1));
        if (driveInterface == FARM_DRIVE_INTERFACE_SATA)
        {
            // SATA
            RETURN_ON_FARM_ERROR(
                create_Node_For_UINT64_From_QWord(errorNode, "# of ASR Events", error->sataErr.numberOfASREvents));
            RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode, "# of Interface CRC Errors",
                                                                   error->sataErr.numberOfInterfaceCRCErrors));
            RETURN_ON_FARM_ERROR(
                create_Node_For_UINT64_From_QWord(errorNode, "Spin Retry Count", error->sataErr.spinRetryCount));
            RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode, "Normalized Spin Retry Count",
                                                                   error->sataErr.spinRetryCountNormalized));
            RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode, "Worst Ever Spin Retry Count",
                                                                   error->sataErr.spinRetryCountWorstEver));
            RETURN_ON_FARM_ERROR(
                create_Node_For_UINT64_From_QWord(errorNode, "# Of IOEDC Errors", error->sataErr.numberOfIOEDCErrors));
            RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode, "# Of Command Timeouts",
                                                                   error->sataErr.commandTimeoutTotal));
            RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode, "# Of Command Timeouts > 5 seconds",
                                                                   error->sataErr.commandTimeoutOver5s));
            RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode, "# Of Command Timeouts > 7.5 seconds",
                                                                   error->sataErr.commandTimeoutOver7pt5s));
        }
        else if (driveInterface == FARM_DRIVE_INTERFACE_SAS)
        {
            // SAS
            RETURN_ON_FARM_ERROR(create_Node_For_UINT64_Hex_From_QWord(
                errorNode, "FRU of SMART Trip Most Recent Frame", error->sasErr.fruCodeOfSMARTTripMostRecentFrame));
            RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode, "Port A Invalid Dword Count",
                                                                   error->sasErr.portAinvDWordCount));
            RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode, "Port B Invalid Dword Count",
                                                                   error->sasErr.portBinvDWordCount));
            RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode, "Port A Disparity Error Count",
                                                                   error->sasErr.portADisparityErrCount));
            RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode, "Port B Disparity Error Count",
                                                                   error->sasErr.portBDisparityErrCount));
            RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode, "Port A Loss of DWord Sync",
                                                                   error->sasErr.portAlossOfDWordSync));
            RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode, "Port B Loss of DWord Sync",
                                                                   error->sasErr.portBlossOfDWordSync));
            RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode, "Port A Phy Reset Problem",
                                                                   error->sasErr.portAphyResetProblem));
            RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(errorNode, "Port B Phy Reset Problem",
                                                                   error->sasErr.portBphyResetProblem));
        }
        RETURN_ON_FARM_ERROR(create_Node_For_FARM_Flash_LED_Events(errorNode, error));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(errorNode, "Lifetime # Unrecoverable Read Errors due to ERC",
                                              error->cumulativeLifetimeUnrecoverableReadErrorsDueToERC));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_From_QWords(
            errorNode, "Cumulative Lifetime Unrecoverable Read Repeat", error->cumLTUnrecReadRepeatByHead, headCount,
            FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_From_QWords(
            errorNode, "Cumulative Lifetime Unrecoverable Read Unique", error->cumLTUnrecReadUniqueByHead, headCount,
            FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_Hex_From_QWord(errorNode, "SMART Trip Flags 1", error->sataPFAAttributes[0]));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_Hex_From_QWord(errorNode, "SMART Trip Flags 2", error->sataPFAAttributes[1]));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(errorNode, "# Reallocated Sectors since last FARM TS Frame",
                                              error->numberReallocatedSectorsSinceLastFARMTimeSeriesFrameSaved));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(errorNode, "# Reallocated Sectors N to N-1 FARM TS Frame",
                                              error->numberReallocatedSectorsBetweenFarmTimeSeriesFrameNandNminus1));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(
            errorNode, "# Realloc Candidate Sectors since last FARM TS Frame",
            error->numberReallocationCandidateSectorsSinceLastFARMTimeSeriesFrameSaved));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(
            errorNode, "# Reallocation Candidate N to N-1 FARM TS Frame",
            error->numberReallocationCandidateSectorsBetweenFarmTimeSeriesFrameNandNminus1));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(
            errorNode, "# Reallocated Sectors since last FARM TS Frame, Actuator 1",
            error->numberReallocatedSectorsSinceLastFARMTimeSeriesFrameSavedActuator1));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(
            errorNode, "# Reallocated Sectors N to N-1 FARM TS Frame, Actuator 1",
            error->numberReallocatedSectorsBetweenFarmTimeSeriesFrameNandNminus1Actuator1));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(
            errorNode, "# Reallocation Candidate Sectors since last FARM TS Frame Actuator 1",
            error->numberReallocationCandidateSectorsSinceLastFARMTimeSeriesFrameSavedActuator1));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(
            errorNode, "# Reallocation Candidate N to N-1 FARM TS Frame Actuator 1",
            error->numberReallocationCandidateSectorsBetweenFarmTimeSeriesFrameNandNminus1Actuator1));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_From_QWords(
            errorNode, "# Unique Unrec sect since last FARM TS Frame",
            error->numberUniqueUnrecoverableSectorsSinceLastFARMTimeSeriesFrameSavedByHead, headCount,
            FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_From_QWords(
            errorNode, "# Unique Unrec sect N to N-1 FARM TS Frame",
            error->numberUniqueUnrecoverableSectorsBetweenFarmTimeSeriesFrameNandNminus1ByHead, headCount,
            FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0));
    }

    return SUCCESS;
}

M_NODISCARD static eReturnValues create_Node_For_FARM_Environmental_Statistics_Page(
    json_object*               rootObject,
    farmEnvironmentStatistics* environment,
    eFARMDriveInterface        driveInterface,
    uint64_t                   timeRestrictedRangeMS)
{
    if (environment != M_NULLPTR && get_Farm_Qword_Data(environment->pageNumber) == FARM_PAGE_ENVIRONMENT_STATS)
    {
        json_object* environmentalNode = json_object_new_object();
        if (environmentalNode == M_NULLPTR)
        {
            return MEMORY_FAILURE;
        }
        RETURN_ON_FARM_ERROR(add_JSON_Object(rootObject, "FARM Log Environmental Statistics", environmentalNode));

        RETURN_ON_FARM_ERROR(create_Node_For_Float_With_Factor_From_QWord(
            environmentalNode, "Current Temperature (C)", environment->currentTemperature,
            driveInterface == FARM_DRIVE_INTERFACE_SAS ? 0.1 : 1.0, true));
        RETURN_ON_FARM_ERROR(create_Node_For_Float_With_Factor_From_QWord(
            environmentalNode, "Highest Temperature (C)", environment->highestTemperature,
            driveInterface == FARM_DRIVE_INTERFACE_SAS ? 0.1 : 1.0, true));
        RETURN_ON_FARM_ERROR(create_Node_For_Float_With_Factor_From_QWord(
            environmentalNode, "Lowest Temperature (C)", environment->lowestTemperature,
            driveInterface == FARM_DRIVE_INTERFACE_SAS ? 0.1 : 1.0, true));
        RETURN_ON_FARM_ERROR(create_Node_For_INT64_From_QWord(environmentalNode, "Average Short Term Temperature (C)",
                                                              environment->avgShortTermTemp));
        RETURN_ON_FARM_ERROR(create_Node_For_INT64_From_QWord(environmentalNode, "Average Long Term Temperature (C)",
                                                              environment->avgLongTermTemp));
        RETURN_ON_FARM_ERROR(create_Node_For_INT64_From_QWord(
            environmentalNode, "Highest Average Short Term Temperature (C)", environment->highestAvgShortTermTemp));
        RETURN_ON_FARM_ERROR(create_Node_For_INT64_From_QWord(
            environmentalNode, "Lowest Average Short Term Temperature (C)", environment->lowestAvgShortTermTemp));
        RETURN_ON_FARM_ERROR(create_Node_For_INT64_From_QWord(
            environmentalNode, "Highest Average Long Term Temperature (C)", environment->highestAvgLongTermTemp));
        RETURN_ON_FARM_ERROR(create_Node_For_INT64_From_QWord(
            environmentalNode, "Lowest Average Long Term Temperature (C)", environment->lowestAvgLongTermTemp));
        RETURN_ON_FARM_ERROR(create_Node_For_Time_From_QWord(environmentalNode, "Time in Over Temperature (Hours)",
                                                             environment->timeOverTemp, MICRO_SECONDS_PER_MINUTE));
        RETURN_ON_FARM_ERROR(create_Node_For_Time_From_QWord(environmentalNode, "Time in Under Temperature (Hours)",
                                                             environment->timeUnderTemp, MICRO_SECONDS_PER_MINUTE));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(environmentalNode, "Specified Max Temperature (C)",
                                                               environment->specifiedMaxTemp));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(environmentalNode, "Specified Min Temperature (C)",
                                                               environment->specifiedMinTemp));
        RETURN_ON_FARM_ERROR(create_Node_For_Float_With_Factor_From_QWord(
            environmentalNode, "Current Relative Humidity (%)", environment->currentRelativeHumidity, 0.1, false));
        RETURN_ON_FARM_ERROR(
            create_Node_For_INT64_From_QWord(environmentalNode, "Current Motor Power Scalar",
                                             environment->currentMotorPowerFromMostRecentSMARTSummaryFrame));
        if ((get_Farm_Status_Byte(environment->currentMotorPowerFromMostRecentSMARTSummaryFrame) &
             FARM_FIELD_SUPPORTED_BIT) > 0)
        {
            RETURN_ON_FARM_ERROR(create_Node_For_Time_From_QWord(
                environmentalNode, "Time Coverage for Motor Power (Hours)", timeRestrictedRangeMS ^ (BIT63 | BIT62),
                MICRO_SECONDS_PER_MILLI_SECONDS));
        }

        bool atleastOnePowerInputbyVoltageSupported =
            (get_Farm_Status_Byte(environment->current12Vinput) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(environment->min12Vinput) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(environment->max12Vinput) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(environment->current5Vinput) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(environment->min5Vinput) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(environment->max5Vinput) & FARM_FIELD_SUPPORTED_BIT);
        RETURN_ON_FARM_ERROR(create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Current 12v input (V)",
                                                                          environment->current12Vinput, 0.001, false));
        RETURN_ON_FARM_ERROR(create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Min 12v input (V)",
                                                                          environment->min12Vinput, 0.001, false));
        RETURN_ON_FARM_ERROR(create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Max 12v input (V)",
                                                                          environment->max12Vinput, 0.001, false));
        RETURN_ON_FARM_ERROR(create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Current 5v input (V)",
                                                                          environment->current5Vinput, 0.001, false));
        RETURN_ON_FARM_ERROR(create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Min 5v input (V)",
                                                                          environment->min5Vinput, 0.001, false));
        RETURN_ON_FARM_ERROR(create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Max 5v input (V)",
                                                                          environment->max5Vinput, 0.001, false));
        if (atleastOnePowerInputbyVoltageSupported)
        {
            RETURN_ON_FARM_ERROR(create_Node_For_Time_From_QWord(
                environmentalNode, "Time Coverage for 12v & 5v voltage (Hours)",
                timeRestrictedRangeMS ^ (BIT63 | BIT62), MICRO_SECONDS_PER_MILLI_SECONDS));
        }

        bool atleastOnePowerInputbyWattSupported =
            (get_Farm_Status_Byte(environment->average12Vpwr) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(environment->min12VPwr) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(environment->max12VPwr) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(environment->average5Vpwr) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(environment->min5Vpwr) & FARM_FIELD_SUPPORTED_BIT) ||
            (get_Farm_Status_Byte(environment->max5Vpwr) & FARM_FIELD_SUPPORTED_BIT);
        RETURN_ON_FARM_ERROR(create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Average 12v power (W)",
                                                                          environment->average12Vpwr, 0.001, false));
        RETURN_ON_FARM_ERROR(create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Min 12v power (W)",
                                                                          environment->min12VPwr, 0.001, false));
        RETURN_ON_FARM_ERROR(create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Max 12v power (W)",
                                                                          environment->max12VPwr, 0.001, false));
        RETURN_ON_FARM_ERROR(create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Average 5v power (W)",
                                                                          environment->average5Vpwr, 0.001, false));
        RETURN_ON_FARM_ERROR(create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Min 5v power (W)",
                                                                          environment->min5Vpwr, 0.001, false));
        RETURN_ON_FARM_ERROR(create_Node_For_Float_With_Factor_From_QWord(environmentalNode, "Max 5v power (W)",
                                                                          environment->max5Vpwr, 0.001, false));
        if (atleastOnePowerInputbyWattSupported)
        {
            RETURN_ON_FARM_ERROR(create_Node_For_Time_From_QWord(
                environmentalNode, "Time Coverage for 12v & 5v power (Hours)", timeRestrictedRangeMS ^ (BIT63 | BIT62),
                MICRO_SECONDS_PER_MILLI_SECONDS));
        }
    }
    return SUCCESS;
}

M_NODISCARD static eReturnValues create_Node_For_FARM_Reliability_Statistics_Page(
    json_object*               rootObject,
    farmReliabilityStatistics* reliability,
    uint64_t                   headCount,
    eFARMDriveInterface        driveInterface,
    uint64_t                   timeRestrictedRangeMS)
{
    if (reliability != M_NULLPTR && get_Farm_Qword_Data(reliability->pageNumber) == FARM_PAGE_RELIABILITY_STATS)
    {
        json_object* reliabilityNode = json_object_new_object();
        if (reliabilityNode == M_NULLPTR)
        {
            return MEMORY_FAILURE;
        }
        RETURN_ON_FARM_ERROR(add_JSON_Object(rootObject, "FARM Log Reliability Statistics", reliabilityNode));

        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(reliabilityNode, "# DOS Scans Performed",
                                                               reliability->numDOSScansPerformed));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(reliabilityNode, "# LBAs corrected by ISP",
                                                               reliability->numLBAsCorrectedByISP));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(reliabilityNode, "# DOS Scans Performed Actuator 1",
                                                               reliability->numDOSScansPerformedActuator1));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(reliabilityNode, "# LBAs corrected by ISP Actuator 1",
                                                               reliability->numLBAsCorrectedByISPActuator1));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_From_QWords(reliabilityNode, "DVGA Skip Write Detect",
                                                                   reliability->dvgaSkipWriteDetectByHead, headCount,
                                                                   FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_From_QWords(reliabilityNode, "RVGA Skip Write Detect",
                                                                   reliability->rvgaSkipWriteDetectByHead, headCount,
                                                                   FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_From_QWords(reliabilityNode, "FVGA Skip Write Detect",
                                                                   reliability->fvgaSkipWriteDetectByHead, headCount,
                                                                   FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_From_QWords(
            reliabilityNode, "Skip Write Detect Threshold Exceeded", reliability->skipWriteDetectExceedsThresholdByHead,
            headCount, FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0));
        if (driveInterface == FARM_DRIVE_INTERFACE_SATA)
        {
            RETURN_ON_FARM_ERROR(
                create_Node_For_UINT64_From_QWord(reliabilityNode, "Read Error Rate", reliability->readErrorRate));
        }
        else
        {
            RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(
                reliabilityNode, "# Read After Write (RAW) Operations", reliability->numRAWOperations));
        }
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(reliabilityNode, "Read Error Rate Normalized",
                                                               reliability->readErrorRateNormalized));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(reliabilityNode, "Read Error Rate Worst Ever",
                                                               reliability->readErrorRateWorstEver));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(reliabilityNode, "Seek Error Rate", reliability->seekErrorRate));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(reliabilityNode, "Seek Error Rate Normalized",
                                                               reliability->seekErrorRateNormalized));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(reliabilityNode, "Seek Error Rate Worst Ever",
                                                               reliability->seekErrorRateWorstEver));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(reliabilityNode, "High Priority Unload Events",
                                                               reliability->highPriorityUnloadEvents));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_With_Delta_Or_Unit_From_QWords(
            reliabilityNode, "MR Head Resistance", "ohms", reliability->mrHeadResistanceByHead, headCount));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_With_Delta_Or_Unit_From_QWords(
            reliabilityNode, "2nd MR Head Resistance", "ohms", reliability->secondHeadMRHeadResistanceByHead,
            headCount));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_From_QWords(reliabilityNode, "# of Velocity Observer",
                                                                   reliability->velocityObserverByHead, headCount,
                                                                   FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_From_QWords(reliabilityNode, "# of Velocity No TMD",
                                                                   reliability->numberOfVelocityObserverNoTMDByHead,
                                                                   headCount, FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0));
        bool    atleastOnevelocityObservedSupported = false;
        uint8_t maxIter =
            (headCount < FARM_MAX_HEADS) ? M_STATIC_CAST(uint8_t, headCount) : M_STATIC_CAST(uint8_t, FARM_MAX_HEADS);
        for (uint8_t headIter = UINT8_C(0); headIter < maxIter; ++headIter)
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
            RETURN_ON_FARM_ERROR(create_Node_For_Time_From_QWord(
                reliabilityNode, "Time Coverage for Velocity Observer (Hours)", timeRestrictedRangeMS ^ (BIT63 | BIT62),
                MICRO_SECONDS_PER_MILLI_SECONDS));
        }

        char h2satStatsName[MAX_STATS_NAME_LENGTH][THREE_STATS_IN_ONE] = {"Z1", "Z2", "Z3"};
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_With_3_Stat_From_QWord(
            reliabilityNode, "H2SAT Trimmed Mean Bits in Error", h2satStatsName,
            reliability->currentH2SATtrimmedMeanBitsInErrorByHeadZone, headCount,
            FARM_BY_HEAD_TO_UINT64_FACTOR_FROM_QWORD, 0.10));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_With_3_Stat_From_QWord(
            reliabilityNode, "H2SAT Iterations to Converge", h2satStatsName,
            reliability->currentH2SATiterationsToConvergeByHeadZone, headCount,
            FARM_BY_HEAD_TO_UINT64_FACTOR_FROM_QWORD, 0.10));
        RETURN_ON_FARM_ERROR(
            create_Node_For_Head_Data_From_QWords(reliabilityNode, "Average H2SAT % Codeword at Iteration Level",
                                                  reliability->currentH2SATpercentCodewordsPerIterByHeadTZAvg,
                                                  headCount, FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_From_QWords(reliabilityNode, "Average H2SAT Amplitude",
                                                                   reliability->currentH2SATamplitudeByHeadTZAvg,
                                                                   headCount, FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_From_QWords(
            reliabilityNode, "Average H2SAT Asymmetry", reliability->currentH2SATasymmetryByHeadTZAvg, headCount,
            FARM_BY_HEAD_TO_INT64_FACTOR_FROM_QWORD, 0.10));
        char flyStatsName[MAX_STATS_NAME_LENGTH][THREE_STATS_IN_ONE] = {"OD", "ID", "MD"};
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_With_3_Stat_From_QWord(
            reliabilityNode, "FAFH Appd Clr Delta (1/1000 A)", flyStatsName,
            reliability->appliedFlyHeightClearanceDeltaByHead, headCount, FARM_BY_HEAD_TO_INT64_FACTOR_FROM_QWORD,
            0.001));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(reliabilityNode, "# Disc Slip Recalibrations Performed",
                                                               reliability->numDiscSlipRecalibrationsPerformed));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_From_QWords(reliabilityNode, "# Reallocated Sectors",
                                                                   reliability->numReallocatedSectorsByHead, headCount,
                                                                   FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_From_QWords(reliabilityNode, "# Reallocated Candidate Sectors",
                                                                   reliability->numReallocationCandidateSectorsByHead,
                                                                   headCount, FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0));
        RETURN_ON_FARM_ERROR(create_Node_For_Bool_From_QWord(reliabilityNode, "Helium Pressure Threshold",
                                                             reliability->heliumPressureThresholdTrip, "Tripped",
                                                             "Not Tripped"));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_From_QWords(reliabilityNode, "# DOS Ought To Scan",
                                                                   reliability->dosOughtScanCountByHead, headCount,
                                                                   FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_From_QWords(reliabilityNode, "# DOS Need To Scan",
                                                                   reliability->dosNeedToScanCountByHead, headCount,
                                                                   FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_From_QWords(reliabilityNode, "# DOS Write Fault Scans",
                                                                   reliability->dosWriteFaultScansByHead, headCount,
                                                                   FARM_BY_HEAD_TO_UINT64_FROM_QWORD, 0.0));
        RETURN_ON_FARM_ERROR(create_Node_For_Head_Data_From_QWords(
            reliabilityNode, "Write Workload Power-on Time (Hours)", reliability->writeWorkloadPowerOnTimeByHead,
            headCount, FARM_BY_HEAD_TO_TIME_FROM_QWORD, MICRO_SECONDS_PER_SECOND));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(reliabilityNode, "# LBAs Corrected By Parity Sector",
                                                               reliability->numLBAsCorrectedByParitySector));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(reliabilityNode,
                                                               "# LBAs Corrected By Parity Sector Actuator 1",
                                                               reliability->numLBAsCorrectedByParitySectorActuator1));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(reliabilityNode, "Primary Super Parity Coverage %",
                                                               reliability->superParityCoveragePercent));
        RETURN_ON_FARM_ERROR(
            create_Node_For_UINT64_From_QWord(reliabilityNode, "Primary Super Parity Coverage SMR/HSMR-SWR %",
                                              reliability->primarySuperParityCoveragePercentageSMR_HSMR_SWR));
        RETURN_ON_FARM_ERROR(create_Node_For_UINT64_From_QWord(
            reliabilityNode, "Primary Super Parity Coverage SMR/HSMR-SWR % Actuator 1",
            reliability->primarySuperParityCoveragePercentageSMR_HSMR_SWRActuator1));
    }

    return SUCCESS;
}

M_PARAM_RO(1)
M_PARAM_RO(2)
M_PARAM_RO(3)
M_PARAM_RO(4)
M_PARAM_WO(5)
OPENSEA_JSONFORMAT_API eReturnValues create_JSON_Output_For_FARM(const tDevice* M_NONNULL device,
                                                                 farmLogData* M_NONNULL   farmdata,
                                                                 const char* M_NONNULL    utilityName,
                                                                 const char* M_NONNULL    buildVersion,
                                                                 char**                   jsonFormat)
{
    eReturnValues ret = NOT_SUPPORTED;

    if (device == M_NULLPTR || farmdata == M_NULLPTR || jsonFormat == M_NULLPTR)
    {
        return BAD_PARAMETER;
    }

    *jsonFormat = M_NULLPTR;

    if (get_Device_DriveType(device) == ATA_DRIVE || get_Device_DriveType(device) == SCSI_DRIVE)
    {
        json_object* rootNode = json_object_new_object();

        if (rootNode == M_NULLPTR)
        {
            return MEMORY_FAILURE;
        }

        if (create_Node_For_Utility_Version(rootNode, utilityName, buildVersion, "FARM", FARM_JSON_VERSION) !=
                SUCCESS ||
            create_Node_For_Drive_Information(rootNode, device) != SUCCESS)
        {
            json_object_put(rootNode);
            return MEMORY_FAILURE;
        }

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

        ret = create_Node_For_FARM_Header_Page(rootNode, &farmdata->header);
        if (ret != SUCCESS)
        {
            json_object_put(rootNode);
            return ret;
        }
        ret = create_Node_For_FARM_Drive_Information_Page(rootNode, &farmdata->driveinfo, farmInterface);
        if (ret != SUCCESS)
        {
            json_object_put(rootNode);
            return ret;
        }
        ret = create_Node_For_FARM_Workload_Statistics_Page(rootNode, &farmdata->workload, timeRestrictedRangeMS);
        if (ret != SUCCESS)
        {
            json_object_put(rootNode);
            return ret;
        }
        ret = create_Node_For_FARM_Error_Statistics_Page(rootNode, &farmdata->error, headCount, farmInterface);
        if (ret != SUCCESS)
        {
            json_object_put(rootNode);
            return ret;
        }
        ret = create_Node_For_FARM_Environmental_Statistics_Page(rootNode, &farmdata->environment, farmInterface,
                                                                 timeRestrictedRangeMS);
        if (ret != SUCCESS)
        {
            json_object_put(rootNode);
            return ret;
        }
        ret = create_Node_For_FARM_Reliability_Statistics_Page(rootNode, &farmdata->reliability, headCount,
                                                               farmInterface, timeRestrictedRangeMS);
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

        ret = SUCCESS;
    }

    return ret;
}
