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
// \file device_statistics_json.c
// \brief This file defines types and functions related to the JSON-based output for Device Statistics log.

#include <json.h>
#include <json_object.h>
#include "code_attributes.h"
#include "common_types.h"
#include "error_translation.h"
#include "io_utils.h"
#include "math_utils.h"
#include "memory_safety.h"
#include "precision_timer.h"
#include "prng.h"
#include "string_utils.h"
#include "type_conversion.h"

#include "defect.h"
#include "dst.h"
#include "logs.h"
#include "smart.h"
#include "scsi_defect_list_json.h"
#include "io_utils.h"
#include "time_utils.h"

#define COMBINE_DEVICE_STATISTICS_JSON_VERSIONS_(x, y, z) #x "." #y "." #z
#define COMBINE_DEVICE_STATISTICS_JSON_VERSIONS(x, y, z)  COMBINE_DEVICE_STATISTICS_JSON_VERSIONS_(x, y, z)

#define DEVICE_STATISTICS_JSON_MAJOR_VERSION              1
#define DEVICE_STATISTICS_JSON_MINOR_VERSION              0
#define DEVICE_STATISTICS_JSON_PATCH_VERSION              0

#define DEVICE_STATISTICS_JSON_VERSION                                                                                 \
    COMBINE_DEVICE_STATISTICS_JSON_VERSIONS(DEVICE_STATISTICS_JSON_MAJOR_VERSION,                                      \
                                            DEVICE_STATISTICS_JSON_MINOR_VERSION,                                      \
                                            DEVICE_STATISTICS_JSON_PATCH_VERSION)

#define MAX_SEAGATE_VALUE_STRING_LENGHT 25
#define MAX_VALUE_STRING_LENGHT         60

M_DECLARE_ENUM(eStatisticsType,
               /*!< Statistics Type Count. */
               STATISTICS_TYPE_COUNT = 0,
               /*!< Statistics Type Workload Utilization. */
               STATISTICS_TYPE_WORKLOAD_UTILIZATION = 1,
               /*!< Statistics Type Utilixation Usage Rate. */
               STATISTICS_TYPE_UTILIZATION_USAGE_RATE = 2,
               /*!< Statistics Type Temperature. */
               STATISTICS_TYPE_TEMPERATURE = 3,
               /*!< Statistics Type Date and Time. */
               STATISTICS_TYPE_DATA_AND_TIME_TIMESTAMP = 4,
               /*!< Statistics Type Time in minutes. */
               STATISTICS_TYPE_TIME_MINUTES = 5,
               /*!< Statistics Type Resource availability (SATA only). */
               STATISTICS_TYPE_SATA_RESOURCE_AVAILABILITY = 6,
               /*!< Statistics Type Randon Write resource used (SATA only). */
               STATISTICS_TYPE_SATA_RANDOM_WRITE_RESOURCE_USED = 7,
               /*!< Statistics Type Non-volatile Time (SAS only). */
               STATISTICS_TYPE_SCSI_NON_VOLATILE_TIME = 8,
               /*!< Statistics Type Date (SAS only). */
               STATISTICS_TYPE_SCSI_DATE = 9,
               /*!< Statistics Type Time interval (SAS only). */
               STATISTICS_TYPE_SCSI_TIME_INTERVAL = 10,
               /*!< Statistics Type Environmental Temperature (SAS only). */
               STATISTICS_TYPE_SCSI_ENVIRONMENTAL_TEMPERATURE = 11,
               /*!< Statistics Type Humidity (SAS only). */
               STATISTICS_TYPE_SCSI_HUMIDITY = 12);

static void create_Node_For_Seagate_Statistic(eStatisticsType  statisticsType,
                                              json_object*     statisticsNode,
                                              seagateStatistic theStatistic,
                                              const char*      statisticName)
{
    DECLARE_ZERO_INIT_ARRAY(char, valueString, MAX_SEAGATE_VALUE_STRING_LENGHT);
    if (theStatistic.isValueValid)
    {
        if (statisticsType == STATISTICS_TYPE_TIME_MINUTES)
        {
            uint64_t timeInMinutes = C_CAST(uint64_t, theStatistic.statisticsDataValue);
            if (!theStatistic.isTimeStampsInMinutes)
                timeInMinutes *= UINT64_C(60);
            snprintf_err_handle(valueString, MAX_SEAGATE_VALUE_STRING_LENGHT, "%" PRIu64 " minutes", timeInMinutes);
        }
        else
        {
            snprintf_err_handle(valueString, MAX_SEAGATE_VALUE_STRING_LENGHT, "%" PRIu32 "",
                                theStatistic.statisticsDataValue);
        }
    }
    else
        snprintf_err_handle(valueString, MAX_SEAGATE_VALUE_STRING_LENGHT, "%s", "Not Avaliable");

    json_object_object_add(statisticsNode, statisticName, json_object_new_string(valueString));
}

static void create_Node_For_Statistic(eStatisticsType statisticsType,
                                      json_object*    statisticsObject,
                                      statistic       theStatistic,
                                      const char*     statisticName,
                                      const char*     statisticUnit,
                                      bool            isLimit)
{
    if (theStatistic.isSupported)
    {
        json_object* statisticsNode = json_object_new_object();

        if (theStatistic.supportsNotification)
        {
            json_object_object_add(statisticsNode, "Supports Notification", json_object_new_string("Yes"));
        }
        else
        {
            json_object_object_add(statisticsNode, "Supports Notification", json_object_new_string("No"));
        }

        if (theStatistic.monitoredConditionMet)
        {
            json_object_object_add(statisticsNode, "Monitored Condition Met", json_object_new_string("Yes"));
        }
        else
        {
            json_object_object_add(statisticsNode, "Monitored Condition Met", json_object_new_string("No"));
        }

        if (theStatistic.isThresholdValid)
        {
            json_object_object_add(statisticsNode, "Condition Monitored With Threshold", json_object_new_string("Yes"));
            switch (theStatistic.threshType)
            {
            case THRESHOLD_TYPE_ALWAYS_TRIGGER_ON_UPDATE:
                json_object_object_add(statisticsNode, "Threshold Trigger Type",
                                       json_object_new_string("Trigger when update"));
                break;
            case THRESHOLD_TYPE_TRIGGER_WHEN_EQUAL:
                json_object_object_add(statisticsNode, "Threshold Trigger Type",
                                       json_object_new_string("Trigger when equal"));
                break;
            case THRESHOLD_TYPE_TRIGGER_WHEN_NOT_EQUAL:
                json_object_object_add(statisticsNode, "Threshold Trigger Type",
                                       json_object_new_string("Trigger when not equal"));
                break;
            case THRESHOLD_TYPE_TRIGGER_WHEN_GREATER:
                json_object_object_add(statisticsNode, "Threshold Trigger Type",
                                       json_object_new_string("Trigger when greater"));
                break;
            case THRESHOLD_TYPE_TRIGGER_WHEN_LESS:
                json_object_object_add(statisticsNode, "Threshold Trigger Type",
                                       json_object_new_string("Trigger when lesser"));
                break;
            case THRESHOLD_TYPE_NO_TRIGGER:
            case THRESHOLD_TYPE_RESERVED:
            default:
                json_object_object_add(statisticsNode, "Threshold Trigger Type", json_object_new_string("No Trigger"));
                break;
            }
        }
        else
        {
            json_object_object_add(statisticsNode, "Condition Monitored With Threshold", json_object_new_string("No"));
            json_object_object_add(statisticsNode, "Threshold Trigger Type", json_object_new_string("N/A"));
        }

        DECLARE_ZERO_INIT_ARRAY(char, valueString, MAX_VALUE_STRING_LENGHT);
        if (theStatistic.isValueValid)
        {
            switch (statisticsType)
            {
            case STATISTICS_TYPE_WORKLOAD_UTILIZATION:
                if (theStatistic.statisticValue != 65535)
                {
                    double workloadUtilization = C_CAST(double, theStatistic.statisticValue);
                    workloadUtilization *= 0.01; // convert to fractional percentage
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%0.02f%%", workloadUtilization);
                }
                else
                {
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%s", ">655.34%%");
                }
                break;

            case STATISTICS_TYPE_UTILIZATION_USAGE_RATE:
            {
                uint8_t utilizationUsageRate = M_Byte0(theStatistic.statisticValue);
                uint8_t rateValidity         = M_Byte5(theStatistic.statisticValue);
                uint8_t rateBasis            = M_Nibble9(theStatistic.statisticValue);
                switch (rateValidity)
                {
                case 0: // valid
                {
                    DECLARE_ZERO_INIT_ARRAY(char, utilizationUsageRateString, 10);
                    if (utilizationUsageRate == 255)
                    {
                        snprintf_err_handle(utilizationUsageRateString, 10, "%s", ">254%%");
                    }
                    else
                    {
                        snprintf_err_handle(utilizationUsageRateString, 10, "%" PRIu8 "%%", utilizationUsageRate);
                    }

                    DECLARE_ZERO_INIT_ARRAY(char, rateBasisString, 25);
                    switch (rateBasis)
                    {
                    case 0: // since manufacture
                        snprintf_err_handle(rateBasisString, 25, "%s", "since manufacture");
                        break;
                    case 4: // since power on reset
                        snprintf_err_handle(rateBasisString, 25, "%s", "since power on reset");
                        break;
                    case 8: // power on hours
                        snprintf_err_handle(rateBasisString, 25, "%s", "for POH");
                        break;
                    case 0xF: // undetermined
                    default:
                        snprintf_err_handle(rateBasisString, 25, "%s", "undetermined");
                        break;
                    }

                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%s %s", utilizationUsageRateString,
                                        rateBasisString);
                }
                break;

                case 0x10: // invalid due to insufficient info
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%s",
                                        "Invalid - insufficient info collected");
                    break;

                case 0x81: // unreasonable due to date and time timestamp
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%s",
                                        "Unreasonable due to date and time timestamp");
                    break;

                case 0xFF:
                default: // invalid for unknown reason
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%s", "Invalid for unknown reason");
                    break;
                }
            }
            break;

            case STATISTICS_TYPE_TEMPERATURE:
                snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%" PRId8 " C",
                                    C_CAST(int8_t, theStatistic.statisticValue));
                break;

            case STATISTICS_TYPE_DATA_AND_TIME_TIMESTAMP:
            case STATISTICS_TYPE_TIME_MINUTES:
                if (theStatistic.statisticValue > 0)
                {
                    uint16_t days           = 0;
                    uint8_t  years          = 0;
                    uint8_t  hours          = 0;
                    uint8_t  minutes        = 0;
                    uint8_t  seconds        = 0;
                    uint64_t statisticValue = 0;
                    if (statisticsType == STATISTICS_TYPE_DATA_AND_TIME_TIMESTAMP)
                    {
                        // this is reported in milliseconds...convert to other displayable.
                        statisticValue = theStatistic.statisticValue / UINT64_C(1000);
                    }
                    else
                    {
                        // this is reported in minutes...convert to other displayable.
                        statisticValue = theStatistic.statisticValue * UINT64_C(60);
                    }

                    convert_Seconds_To_Displayable_Time(statisticValue, &years, &days, &hours, &minutes, &seconds);
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT,
                                        "%" PRIu8 " years %" PRIu16 " days %" PRIu8 " hours %" PRIu8 " minutes %" PRIu8
                                        " seconds",
                                        years, days, hours, minutes, seconds);
                }
                else
                {
                    if (statisticsType == STATISTICS_TYPE_DATA_AND_TIME_TIMESTAMP)
                    {
                        snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%s", "0 milliseconds");
                    }
                    else
                    {
                        snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%s", "0 minutes");
                    }
                }
                break;

            case STATISTICS_TYPE_SATA_RESOURCE_AVAILABILITY:
            {
                double fractionAvailable = C_CAST(double, M_Word0(theStatistic.statisticValue)) / 65535.0;
                snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%0.02f%% Available", fractionAvailable);
            }
            break;

            case STATISTICS_TYPE_SATA_RANDOM_WRITE_RESOURCE_USED:
            {
                uint8_t resourceValue = M_Byte0(theStatistic.statisticValue);
                if (/* resourceValue >= 0 && */ resourceValue <= 0x7F)
                {
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "Within nominal bounds (%" PRIX8 "h)",
                                        resourceValue);
                }
                else
                {
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "Exceeds nominal bounds (%" PRIX8 "h)",
                                        resourceValue);
                }
            }
            break;

            case STATISTICS_TYPE_SCSI_NON_VOLATILE_TIME:
                switch (theStatistic.statisticValue)
                {
                case 0:
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%s", "Volatile");
                    break;
                case 1:
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%s", "Nonvolatile for unknown time");
                    break;
                case 0xFFFFFF:
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%s", "Nonvolatile indefinitely");
                    break;
                default: // time in minutes
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "Nonvolatile for %" PRIu64 "minutes",
                                        theStatistic.statisticValue);
                    break;
                }
                break;

            case STATISTICS_TYPE_SCSI_DATE:
            {
                DECLARE_ZERO_INIT_ARRAY(char, week, 3);
                DECLARE_ZERO_INIT_ARRAY(char, year, 5);
                year[0] = C_CAST(char, M_Byte3(theStatistic.statisticValue));
                year[1] = C_CAST(char, M_Byte2(theStatistic.statisticValue));
                year[2] = C_CAST(char, M_Byte1(theStatistic.statisticValue));
                year[3] = C_CAST(char, M_Byte0(theStatistic.statisticValue));
                year[4] = '\0';
                week[0] = C_CAST(char, M_Byte5(theStatistic.statisticValue));
                week[1] = C_CAST(char, M_Byte4(theStatistic.statisticValue));
                week[2] = '\0';
                if (strcmp(year, "    ") == 0 && strcmp(week, "  ") == 0)
                {
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%s", "Not set");
                }
                else
                {
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "Week %s, %s", week, year);
                }
            }
            break;

            case STATISTICS_TYPE_SCSI_TIME_INTERVAL:
            {
                uint32_t exponent = M_DoubleWord0(theStatistic.statisticValue);
                uint32_t integer  = M_DoubleWord1(theStatistic.statisticValue);
                // now byteswap the double words to get the correct endianness (for LSB machines)
                byte_Swap_32(&exponent);
                byte_Swap_32(&integer);
                switch (exponent)
                {
                case 1: // deci
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu32 " %s", integer,
                                        "deci seconds");
                    break;
                case 2: // centi
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu32 " %s", integer,
                                        "centi seconds");
                    break;
                case 3: // milli
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu32 " %s", integer,
                                        "milli seconds");
                    break;
                case 6: // micro
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu32 " %s", integer,
                                        "micro seconds");
                    break;
                case 9: // nano
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu32 " %s", integer,
                                        "nano seconds");
                    break;
                case 12: // pico
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu32 " %s", integer,
                                        "pico seconds");
                    break;
                case 15: // femto
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu32 " %s", integer,
                                        "femto seconds");
                    break;
                case 18: // atto
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu32 " %s", integer,
                                        "atto seconds");
                    break;
                default:
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu32 " Unknown exponent value",
                                        integer);
                    break;
                }
            }
            break;

            case STATISTICS_TYPE_SCSI_ENVIRONMENTAL_TEMPERATURE:
                if (C_CAST(int8_t, theStatistic.statisticValue) == -128)
                {
                    if (isLimit)
                    {
                        snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%s", "No Temperature Limit");
                    }
                    else
                    {
                        snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%s", "No Valid Temperature");
                    }
                }
                else
                {
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%" PRId8 " C",
                                        C_CAST(int8_t, theStatistic.statisticValue));
                }
                break;

            case STATISTICS_TYPE_SCSI_HUMIDITY:
                if (/*theStatistic.statisticValue >= 0 &&*/ theStatistic.statisticValue <= 100)
                {
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu8 "",
                                        C_CAST(uint8_t, theStatistic.statisticValue));
                }
                else if (theStatistic.statisticValue == 255)
                {
                    if (isLimit)
                    {
                        snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%s", "No relative humidity limit");
                    }
                    else
                    {
                        snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%s", "No valid relative humidity");
                    }
                }
                else
                {
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%s", "Reserved value reported");
                }
                break;

            case STATISTICS_TYPE_COUNT:
            default:
                if (statisticUnit != M_NULLPTR)
                {
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu64 " %s",
                                        theStatistic.statisticValue, statisticUnit);
                }
                else
                {
                    snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu64 "",
                                        theStatistic.statisticValue);
                }
                break;
            }
        }
        else
        {
            snprintf_err_handle(valueString, MAX_VALUE_STRING_LENGHT, "%s", "Invalid");
        }
        json_object_object_add(statisticsNode, "Value", json_object_new_string(valueString));

        json_object_object_add(statisticsObject, statisticName, statisticsNode);
    }
}

eReturnValues create_JSON_Output_For_SCSI_Defect_List(ptrSCSIDefectList defects, char** jsonFormat)
{
    eReturnValues ret = NOT_SUPPORTED;

    if (defects == M_NULLPTR)
    {
        return BAD_PARAMETER;
    }

    json_object* scsiDefectJson = json_object_new_object();

    if (defects->containsPrimaryList)
    {
        json_object_object_add(scsiDefectJson, "Contains Primary List", json_object_new_boolean(true));
        ret = SUCCESS;
    }

    if (defects->containsGrownList)
    {
        json_object_object_add(scsiDefectJson, "Contains Grown List", json_object_new_boolean(true));
        ret = SUCCESS;
    }

    if (defects->generation > 0)
    {
        json_object_object_add(scsiDefectJson, "Generation Code", json_object_new_int(defects->generation));
        ret = SUCCESS;
    }

    if (defects->deviceHasMultipleLogicalUnits)
    {
        json_object_object_add(scsiDefectJson, "Device Has Multiple Logical Units", json_object_new_boolean(true));
        ret = SUCCESS;
    }

    switch (defects->format)
    {
    case AD_SHORT_BLOCK_FORMAT_ADDRESS_DESCRIPTOR:
    {
        json_object_object_add(scsiDefectJson, "Format", json_object_new_string("Short Block Format"));

        if (defects->numberOfElements > UINT32_C(0))
        {
            json_object_object_add(scsiDefectJson, "Total Defects in List",
                                   json_object_new_int(defects->numberOfElements));

            struct json_object* defectList = json_object_new_array();
            for (uint64_t iter = UINT64_C(0); iter < defects->numberOfElements; ++iter)
            {
                json_object_array_add(defectList, json_object_new_int(defects->block[iter].shortBlockAddress));
            }
            json_object_object_add(scsiDefectJson, "Defect Addresses", defectList);
        }
        else
        {
            json_object_object_add(scsiDefectJson, "Total Defects in List", json_object_new_int(0));
            json_object_object_add(scsiDefectJson, "Defect Addresses", json_object_new_array());
        }
        break;
    }
    case AD_LONG_BLOCK_FORMAT_ADDRESS_DESCRIPTOR:
    {
        json_object_object_add(scsiDefectJson, "Format", json_object_new_string("Long Block Format"));

        if (defects->numberOfElements > UINT32_C(0))
        {
            json_object_object_add(scsiDefectJson, "Total Defects in List",
                                   json_object_new_int(defects->numberOfElements));

            struct json_object* defectList = json_object_new_array();
            for (uint64_t iter = UINT64_C(0); iter < defects->numberOfElements; ++iter)
            {
                json_object_array_add(defectList, json_object_new_int64(defects->block[iter].longBlockAddress));
            }
            json_object_object_add(scsiDefectJson, "Defect Addresses", defectList);
        }
        else
        {
            json_object_object_add(scsiDefectJson, "Total Defects in List", json_object_new_int(0));
            json_object_object_add(scsiDefectJson, "Defect Addresses", json_object_new_array());
        }
        break;
    }

    case AD_EXTENDED_PHYSICAL_SECTOR_FORMAT_ADDRESS_DESCRIPTOR:
    {
        json_object_object_add(scsiDefectJson, "Format", json_object_new_string("Extended Physical Sector Format"));
        json_object_object_add(scsiDefectJson, "Total Defects in List", json_object_new_int(defects->numberOfElements));

        struct json_object* defectRanges = json_object_new_array();

        if (defects->numberOfElements > 0)
        {
            uint32_t startCylinder = defects->physical[0].cylinderNumber;
            uint8_t  startHead     = defects->physical[0].headNumber;
            uint32_t startSector   = defects->physical[0].sectorNumber;
            uint32_t endSector     = startSector;

            for (uint64_t iter = 1; iter < defects->numberOfElements; ++iter)
            {
                uint32_t currCylinder = defects->physical[iter].cylinderNumber;
                uint8_t  currHead     = defects->physical[iter].headNumber;
                uint32_t currSector   = defects->physical[iter].sectorNumber;

                bool isSequential =
                    (currCylinder == startCylinder) && (currHead == startHead) && (currSector == endSector + 1);

                if (isSequential)
                {
                    endSector = currSector;
                }
                else
                {
                    struct json_object* rangeObj = json_object_new_object();
                    json_object_object_add(rangeObj, "Cylinder", json_object_new_int(startCylinder));
                    json_object_object_add(rangeObj, "Head", json_object_new_int(startHead));
                    json_object_object_add(rangeObj, "Start Sector",
                                           startSector == MAX_28BIT ? json_object_new_string("Full Track")
                                                                    : json_object_new_int(startSector));
                    if (startSector != MAX_28BIT)
                    {
                        json_object_object_add(rangeObj, "Sector Length",
                                               json_object_new_int(endSector - startSector + 1));
                    }
                    json_object_array_add(defectRanges, rangeObj);

                    startCylinder = currCylinder;
                    startHead     = currHead;
                    startSector   = currSector;
                    endSector     = currSector;
                }
            }

            // Add the final range
            struct json_object* rangeObj = json_object_new_object();
            json_object_object_add(rangeObj, "Cylinder", json_object_new_int(startCylinder));
            json_object_object_add(rangeObj, "Head", json_object_new_int(startHead));
            json_object_object_add(rangeObj, "Start Sector",
                                   startSector == MAX_28BIT ? json_object_new_string("Full Track")
                                                            : json_object_new_int(startSector));
            if (startSector != MAX_28BIT)
            {
                json_object_object_add(rangeObj, "Sector Length", json_object_new_int(endSector - startSector + 1));
            }
            json_object_array_add(defectRanges, rangeObj);
        }

        json_object_object_add(scsiDefectJson, "Defect Ranges", defectRanges);
        break;
    }

    case AD_PHYSICAL_SECTOR_FORMAT_ADDRESS_DESCRIPTOR:
    {
        json_object_object_add(scsiDefectJson, "Format", json_object_new_string("Physical Sector Format"));
        json_object_object_add(scsiDefectJson, "Total Defects in List", json_object_new_int(defects->numberOfElements));

        struct json_object* defectRanges = json_object_new_array();

        if (defects->numberOfElements > 0)
        {
            uint32_t startCylinder = defects->physical[0].cylinderNumber;
            uint8_t  startHead     = defects->physical[0].headNumber;
            uint32_t startSector   = defects->physical[0].sectorNumber;
            uint32_t endSector     = startSector;

            for (uint64_t iter = 1; iter < defects->numberOfElements; ++iter)
            {
                uint32_t currCylinder = defects->physical[iter].cylinderNumber;
                uint8_t  currHead     = defects->physical[iter].headNumber;
                uint32_t currSector   = defects->physical[iter].sectorNumber;

                bool isSequential =
                    (currCylinder == startCylinder) && (currHead == startHead) && (currSector == endSector + 1);

                if (isSequential)
                {
                    endSector = currSector;
                }
                else
                {
                    struct json_object* rangeObj = json_object_new_object();
                    json_object_object_add(rangeObj, "Cylinder", json_object_new_int(startCylinder));
                    json_object_object_add(rangeObj, "Head", json_object_new_int(startHead));
                    json_object_object_add(rangeObj, "Start Sector",
                                           startSector == UINT32_MAX ? json_object_new_string("Full Track")
                                                                     : json_object_new_int(startSector));
                    if (startSector != UINT32_MAX)
                    {
                        json_object_object_add(rangeObj, "Sector Length",
                                               json_object_new_int(endSector - startSector + 1));
                    }
                    json_object_array_add(defectRanges, rangeObj);

                    startCylinder = currCylinder;
                    startHead     = currHead;
                    startSector   = currSector;
                    endSector     = currSector;
                }
            }

            // Add the final range
            struct json_object* rangeObj = json_object_new_object();
            json_object_object_add(rangeObj, "Cylinder", json_object_new_int(startCylinder));
            json_object_object_add(rangeObj, "Head", json_object_new_int(startHead));
            json_object_object_add(rangeObj, "Start Sector",
                                   startSector == UINT32_MAX ? json_object_new_string("Full Track")
                                                             : json_object_new_int(startSector));
            if (startSector != UINT32_MAX)
            {
                json_object_object_add(rangeObj, "Sector Length", json_object_new_int(endSector - startSector + 1));
            }
            json_object_array_add(defectRanges, rangeObj);
        }

        json_object_object_add(scsiDefectJson, "Defect Ranges", defectRanges);
        break;
    }


    case AD_EXTENDED_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR:
    {
        json_object_object_add(scsiDefectJson, "Format", json_object_new_string("Extended Bytes From Index Format"));

        if (defects->numberOfElements > UINT32_C(0))
        {
            json_object_object_add(scsiDefectJson, "Total Defects in List",
                                   json_object_new_int(defects->numberOfElements));

            struct json_object* defectList = json_object_new_array();
            bool                multiBit   = false;

            for (uint64_t iter = UINT64_C(0); iter < defects->numberOfElements; ++iter)
            {
                struct json_object* defectEntry = json_object_new_object();

                if (defects->bfi[iter].multiAddressDescriptorStart)
                {
                    multiBit = true;
                    json_object_object_add(defectEntry, "Multi Descriptor Start", json_object_new_boolean(true));
                }
                else if (multiBit)
                {
                    json_object_object_add(defectEntry, "Multi Descriptor Continuation", json_object_new_boolean(true));
                    multiBit = false;
                }

                json_object_object_add(defectEntry, "Cylinder", json_object_new_int(defects->bfi[iter].cylinderNumber));
                json_object_object_add(defectEntry, "Head", json_object_new_int(defects->bfi[iter].headNumber));

                if (defects->bfi[iter].bytesFromIndex == MAX_28BIT)
                {
                    json_object_object_add(defectEntry, "Bytes From Index", json_object_new_string("Full Track"));
                }
                else
                {
                    json_object_object_add(defectEntry, "Bytes From Index",
                                           json_object_new_int(defects->bfi[iter].bytesFromIndex));
                }

                json_object_array_add(defectList, defectEntry);
            }

            json_object_object_add(scsiDefectJson, "Defect Entries", defectList);
        }
        else
        {
            json_object_object_add(scsiDefectJson, "Total Defects in List", json_object_new_int(0));
            json_object_object_add(scsiDefectJson, "Defect Entries", json_object_new_array());
        }

        break;
    }
    case AD_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR:
    {
        json_object_object_add(scsiDefectJson, "Format", json_object_new_string("Bytes From Index Format"));

        if (defects->numberOfElements > UINT32_C(0))
        {
            json_object_object_add(scsiDefectJson, "Total Defects in List",
                                   json_object_new_int(defects->numberOfElements));

            struct json_object* defectList = json_object_new_array();

            for (uint64_t iter = UINT64_C(0); iter < defects->numberOfElements; ++iter)
            {
                struct json_object* defectEntry = json_object_new_object();

                json_object_object_add(defectEntry, "Cylinder", json_object_new_int(defects->bfi[iter].cylinderNumber));
                json_object_object_add(defectEntry, "Head", json_object_new_int(defects->bfi[iter].headNumber));

                if (defects->bfi[iter].bytesFromIndex == UINT32_MAX)
                {
                    json_object_object_add(defectEntry, "Bytes From Index", json_object_new_string("Full Track"));
                }
                else
                {
                    json_object_object_add(defectEntry, "Bytes From Index",
                                           json_object_new_int(defects->bfi[iter].bytesFromIndex));
                }

                json_object_array_add(defectList, defectEntry);
            }

            json_object_object_add(scsiDefectJson, "Defect Entries", defectList);
        }
        else
        {
            json_object_object_add(scsiDefectJson, "Total Defects in List", json_object_new_int(0));
            json_object_object_add(scsiDefectJson, "Defect Entries", json_object_new_array());
        }

        break;
    }



        // Add other formats here as needed
    }


        const char* jstr =
        json_object_to_json_string_ext(scsiDefectJson, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);

                if (asprintf(jsonFormat, "%s", jstr) < 0)
        {
            ret = MEMORY_FAILURE;
        }

    return ret;
}