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

// \file device_statistics_json.c
// \brief This file defines types and functions related to the JSON-based Device Statistics log file process.

#include <json_object.h>
#include <json.h>

#include "io_utils.h"
#include "time_utils.h"
#include "device_statistics_json.h"
#include "secure_file.h"
#include "logs.h"

#define COMBINE_DEVICE_STATISTICS_JSON_VERSIONS_(x,y,z)     #x "." #y "." #z
#define COMBINE_DEVICE_STATISTICS_JSON_VERSIONS(x,y,z)      COMBINE_DEVICE_STATISTICS_JSON_VERSIONS_(x,y,z)

#define DEVICE_STATISTICS_JSON_MAJOR_VERSION                1
#define DEVICE_STATISTICS_JSON_MINOR_VERSION                0
#define DEVICE_STATISTICS_JSON_PATCH_VERSION                0

#define DEVICE_STATISTICS_JSON_VERSION                      COMBINE_DEVICE_STATISTICS_JSON_VERSIONS(DEVICE_STATISTICS_JSON_MAJOR_VERSION,DEVICE_STATISTICS_JSON_MINOR_VERSION,DEVICE_STATISTICS_JSON_PATCH_VERSION)

#define MAX_SEAGATE_VALUE_STRING_LENGHT                     25
#define MAX_VALUE_STRING_LENGHT                             60

typedef enum _eStatisticsType
{
    STATISTICS_TYPE_COUNT,
    STATISTICS_TYPE_WORKLOAD_UTILIZATION,
    STATISTICS_TYPE_UTILIZATION_USAGE_RATE,
    STATISTICS_TYPE_TEMPERATURE,
    STATISTICS_TYPE_DATA_AND_TIME_TIMESTAMP,
    STATISTICS_TYPE_TIME_MINUTES,
    //SATA Specific
    STATISTICS_TYPE_SATA_RESOURCE_AVAILABILITY,
    STATISTICS_TYPE_SATA_RANDOM_WRITE_RESOURCE_USED,
    //SCSI Specific
    STATISTICS_TYPE_SCSI_NON_VOLATILE_TIME,
    STATISTICS_TYPE_SCSI_DATE,
    STATISTICS_TYPE_SCSI_TIME_INTERVAL,
    STATISTICS_TYPE_SCSI_ENVIRONMENTAL_TEMPERATURE,
    STATISTICS_TYPE_SCSI_HUMIDITY,
} eStatisticsType;

static void create_Node_For_Seagate_Statistic(eStatisticsType statisticsType, json_object *statisticsNode, seagateStatistic theStatistic, const char *statisticName)
{
    DECLARE_ZERO_INIT_ARRAY(char, valueString, MAX_SEAGATE_VALUE_STRING_LENGHT);
    if (theStatistic.isValueValid)
    {
        if (statisticsType == STATISTICS_TYPE_TIME_MINUTES)
        {
            uint64_t timeInMinutes = C_CAST(uint64_t, theStatistic.statisticsDataValue);
            if (!theStatistic.isTimeStampsInMinutes)
                timeInMinutes *= UINT64_C(60);
            snprintf(valueString, MAX_SEAGATE_VALUE_STRING_LENGHT, "%" PRIu64 " minutes", timeInMinutes);
        }
        else
        {
            snprintf(valueString, MAX_SEAGATE_VALUE_STRING_LENGHT, "%" PRIu32 "", theStatistic.statisticsDataValue);
        }
    }
    else
        snprintf(valueString, MAX_SEAGATE_VALUE_STRING_LENGHT, "%s", "Not Avaliable");

    json_object_object_add(statisticsNode, statisticName, json_object_new_string(valueString));
}

static void create_Node_For_Statistic(eStatisticsType statisticsType, json_object *statisticsObject, statistic theStatistic, const char *statisticName, const char *statisticUnit, bool isLimit)
{
    if (theStatistic.isSupported)
    {
        json_object *statisticsNode = json_object_new_object();

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
                json_object_object_add(statisticsNode, "Threshold Trigger Type", json_object_new_string("Trigger when update"));
                break;
            case THRESHOLD_TYPE_TRIGGER_WHEN_EQUAL:
                json_object_object_add(statisticsNode, "Threshold Trigger Type", json_object_new_string("Trigger when equal"));
                break;
            case THRESHOLD_TYPE_TRIGGER_WHEN_NOT_EQUAL:
                json_object_object_add(statisticsNode, "Threshold Trigger Type", json_object_new_string("Trigger when not equal"));
                break;
            case THRESHOLD_TYPE_TRIGGER_WHEN_GREATER:
                json_object_object_add(statisticsNode, "Threshold Trigger Type", json_object_new_string("Trigger when greater"));
                break;
            case THRESHOLD_TYPE_TRIGGER_WHEN_LESS:
                json_object_object_add(statisticsNode, "Threshold Trigger Type", json_object_new_string("Trigger when lesser"));
                break;
            case THRESHOLD_TYPE_NO_TRIGGER:
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
                    workloadUtilization *= 0.01;//convert to fractional percentage
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%0.02f%%", workloadUtilization);
                }
                else
                {
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%s", ">655.34%%");
                }
                break;

            case STATISTICS_TYPE_UTILIZATION_USAGE_RATE:
            {
                uint8_t utilizationUsageRate = M_Byte0(theStatistic.statisticValue);
                uint8_t rateValidity = M_Byte5(theStatistic.statisticValue);
                uint8_t rateBasis = M_Nibble9(theStatistic.statisticValue);
                switch (rateValidity)
                {
                case 0://valid
                {
                    DECLARE_ZERO_INIT_ARRAY(char, utilizationUsageRateString, 10);
                    if (utilizationUsageRate == 255)
                    {
                        snprintf(utilizationUsageRateString, 10, "%s", ">254%%");
                    }
                    else
                    {
                        snprintf(utilizationUsageRateString, 10, "%" PRIu8 "%%", utilizationUsageRate);
                    }

                    DECLARE_ZERO_INIT_ARRAY(char, rateBasisString, 25);
                    switch (rateBasis)
                    {
                    case 0://since manufacture
                        snprintf(rateBasisString, 25, "%s", "since manufacture");
                        break;
                    case 4://since power on reset
                        snprintf(rateBasisString, 25, "%s", "since power on reset");
                        break;
                    case 8://power on hours
                        snprintf(rateBasisString, 25, "%s", "for POH");
                        break;
                    case 0xF://undetermined
                    default:
                        snprintf(rateBasisString, 25, "%s", "undetermined");
                        break;
                    }

                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%s %s", utilizationUsageRateString, rateBasisString);
                }
                break;

                case 0x10://invalid due to insufficient info
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%s", "Invalid - insufficient info collected");
                    break;

                case 0x81://unreasonable due to date and time timestamp
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%s", "Unreasonable due to date and time timestamp");
                    break;

                case 0xFF:
                default://invalid for unknown reason
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%s", "Invalid for unknown reason");
                    break;
                }
            }
            break;

            case STATISTICS_TYPE_TEMPERATURE:
                snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%" PRId8 " C", C_CAST(int8_t, theStatistic.statisticValue));
                break;

            case STATISTICS_TYPE_DATA_AND_TIME_TIMESTAMP:
            case STATISTICS_TYPE_TIME_MINUTES:
                if (theStatistic.statisticValue > 0)
                {
                    uint16_t days = 0;
                    uint8_t years = 0;
                    uint8_t hours = 0;
                    uint8_t minutes = 0;
                    uint8_t seconds = 0;
                    uint64_t statisticValue = 0;
                    if (statisticsType == STATISTICS_TYPE_DATA_AND_TIME_TIMESTAMP)
                    {
                        //this is reported in milliseconds...convert to other displayable.
                        statisticValue = theStatistic.statisticValue / UINT64_C(1000);
                    }
                    else
                    {
                        //this is reported in minutes...convert to other displayable.
                        statisticValue = theStatistic.statisticValue * UINT64_C(60);
                    }

                    convert_Seconds_To_Displayable_Time(statisticValue, &years, &days, &hours, &minutes, &seconds);
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu8 " years %" PRIu16 " days %" PRIu8 " hours %" PRIu8 " minutes %" PRIu8 " seconds", years, days, hours, minutes, seconds);
                }
                else
                {
                    if (statisticsType == STATISTICS_TYPE_DATA_AND_TIME_TIMESTAMP)
                    {
                        snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%s", "0 milliseconds");
                    }
                    else
                    {
                        snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%s", "0 minutes");
                    }
                }
                break;

            case STATISTICS_TYPE_SATA_RESOURCE_AVAILABILITY:
            {
                double fractionAvailable = C_CAST(double, M_Word0(theStatistic.statisticValue)) / 65535.0;
                snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%0.02f%% Available", fractionAvailable);
            }
            break;

            case STATISTICS_TYPE_SATA_RANDOM_WRITE_RESOURCE_USED:
            {
                uint8_t resourceValue = M_Byte0(theStatistic.statisticValue);
                if (/* resourceValue >= 0 && */ resourceValue <= 0x7F)
                {
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "Within nominal bounds (%" PRIX8 "h)", resourceValue);
                }
                else
                {
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "Exceeds nominal bounds (%" PRIX8 "h)", resourceValue);
                }
            }
            break;

            case STATISTICS_TYPE_SCSI_NON_VOLATILE_TIME:
                switch (theStatistic.statisticValue)
                {
                case 0:
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%s", "Volatile");
                    break;
                case 1:
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%s", "Nonvolatile for unknown time");
                    break;
                case 0xFFFFFF:
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%s", "Nonvolatile indefinitely");
                    break;
                default://time in minutes
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "Nonvolatile for %" PRIu64 "minutes", theStatistic.statisticValue);
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
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%s", "Not set");
                }
                else
                {
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "Week %s, %s", week, year);
                }
            }
            break;

            case STATISTICS_TYPE_SCSI_TIME_INTERVAL:
            {
                uint32_t exponent = M_DoubleWord0(theStatistic.statisticValue);
                uint32_t integer = M_DoubleWord1(theStatistic.statisticValue);
                //now byteswap the double words to get the correct endianness (for LSB machines)
                byte_Swap_32(&exponent);
                byte_Swap_32(&integer);
                switch (exponent)
                {
                case 1://deci
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu32 " %s", integer, "deci seconds");
                    break;
                case 2://centi
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu32 " %s", integer, "centi seconds");
                    break;
                case 3://milli
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu32 " %s", integer, "milli seconds");
                    break;
                case 6://micro
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu32 " %s", integer, "micro seconds");
                    break;
                case 9://nano
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu32 " %s", integer, "nano seconds");
                    break;
                case 12://pico
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu32 " %s", integer, "pico seconds");
                    break;
                case 15://femto
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu32 " %s", integer, "femto seconds");
                    break;
                case 18://atto
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu32 " %s", integer, "atto seconds");
                    break;
                default:
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu32 " Unknown exponent value", integer);
                    break;
                }
            }
            break;

            case STATISTICS_TYPE_SCSI_ENVIRONMENTAL_TEMPERATURE:
                if (C_CAST(int8_t, theStatistic.statisticValue) == -128)
                {
                    if (isLimit)
                    {
                        snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%s", "No Temperature Limit");
                    }
                    else
                    {
                        snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%s", "No Valid Temperature");
                    }
                }
                else
                {
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%" PRId8 " C", C_CAST(int8_t, theStatistic.statisticValue));
                }
                break;

            case STATISTICS_TYPE_SCSI_HUMIDITY:
                if (/*theStatistic.statisticValue >= 0 &&*/ theStatistic.statisticValue <= 100)
                {
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu8 "", C_CAST(uint8_t, theStatistic.statisticValue));
                }
                else if (theStatistic.statisticValue == 255)
                {
                    if (isLimit)
                    {
                        snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%s", "No relative humidity limit");
                    }
                    else
                    {
                        snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%s", "No valid relative humidity");
                    }
                }
                else
                {
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%s", "Reserved value reported");
                }
                break;

            case STATISTICS_TYPE_COUNT:
            default:
                if (statisticUnit != M_NULLPTR)
                {
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu64 " %s", theStatistic.statisticValue, statisticUnit);
                }
                else
                {
                    snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%" PRIu64 "", theStatistic.statisticValue);
                }
                break;
            }
        }
        else
        {
            snprintf(valueString, MAX_VALUE_STRING_LENGHT, "%s", "Invalid");
        }
        json_object_object_add(statisticsNode, "Value", json_object_new_string(valueString));

        json_object_object_add(statisticsObject, statisticName, statisticsNode);
    }
}

static eReturnValues create_JSON_File_For_ATA_Device_Statistics(tDevice *device, ptrDeviceStatistics deviceStatictics, ptrSeagateDeviceStatistics seagateDeviceStatistics, bool seagateDeviceStatisticsAvailable, char** jsonFormat)
{
    eReturnValues ret = NOT_SUPPORTED;
    bool atleastOneStatisticsAvailable = false;

    if (deviceStatictics == M_NULLPTR || (seagateDeviceStatisticsAvailable && seagateDeviceStatistics == M_NULLPTR))
    {
        return BAD_PARAMETER;
    }

    json_object *rootNode = json_object_new_object();

    if (rootNode == M_NULLPTR)
        return MEMORY_FAILURE;

    //Add version information
    json_object_object_add(rootNode, "Device Statistics Version", json_object_new_string(DEVICE_STATISTICS_JSON_VERSION));

    if (deviceStatictics->sataStatistics.generalStatisticsSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *generalStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalStatistics, deviceStatictics->sataStatistics.lifetimePoweronResets, "LifeTime Power-On Resets", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalStatistics, deviceStatictics->sataStatistics.powerOnHours, "Power-On Hours", "hours", false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalStatistics, deviceStatictics->sataStatistics.logicalSectorsWritten, "Logical Sectors Written", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalStatistics, deviceStatictics->sataStatistics.numberOfWriteCommands, "Number Of Write Commands", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalStatistics, deviceStatictics->sataStatistics.logicalSectorsRead, "Logical Sectors Read", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalStatistics, deviceStatictics->sataStatistics.numberOfReadCommands, "Number Of Read Commands", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_DATA_AND_TIME_TIMESTAMP, generalStatistics, deviceStatictics->sataStatistics.dateAndTimeTimestamp, "Date And Time Timestamp", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalStatistics, deviceStatictics->sataStatistics.pendingErrorCount, "Pending Error Count", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_WORKLOAD_UTILIZATION, generalStatistics, deviceStatictics->sataStatistics.workloadUtilization, "Workload Utilization", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_UTILIZATION_USAGE_RATE, generalStatistics, deviceStatictics->sataStatistics.utilizationUsageRate, "Utilization Usage Rate", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_SATA_RESOURCE_AVAILABILITY, generalStatistics, deviceStatictics->sataStatistics.resourceAvailability, "Resource Availability", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_SATA_RANDOM_WRITE_RESOURCE_USED, generalStatistics, deviceStatictics->sataStatistics.randomWriteResourcesUsed, "Random Write Resources Used", M_NULLPTR, false);

        json_object_object_add(rootNode, "General Statistics", generalStatistics);
    }

    if (deviceStatictics->sataStatistics.freeFallStatisticsSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *freeFallStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, freeFallStatistics, deviceStatictics->sataStatistics.numberOfFreeFallEventsDetected, "Number Of Free-Fall Events Detected", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, freeFallStatistics, deviceStatictics->sataStatistics.overlimitShockEvents, "Overlimit Shock Events", M_NULLPTR, false);

        json_object_object_add(rootNode, "Free Fall Statistics", freeFallStatistics);
    }

    if (deviceStatictics->sataStatistics.rotatingMediaStatisticsSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *rotatingMediaStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, rotatingMediaStatistics, deviceStatictics->sataStatistics.spindleMotorPoweronHours, "Spindle Motor Power-On Hours", "hours", false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, rotatingMediaStatistics, deviceStatictics->sataStatistics.headFlyingHours, "Head Flying Hours", "hours", false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, rotatingMediaStatistics, deviceStatictics->sataStatistics.headLoadEvents, "Head Load Events", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, rotatingMediaStatistics, deviceStatictics->sataStatistics.numberOfReallocatedLogicalSectors, "Number Of Reallocated Logical Sectors", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, rotatingMediaStatistics, deviceStatictics->sataStatistics.readRecoveryAttempts, "Read Recovery Attempts", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, rotatingMediaStatistics, deviceStatictics->sataStatistics.numberOfMechanicalStartFailures, "Number Of Mechanical Start Failures", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, rotatingMediaStatistics, deviceStatictics->sataStatistics.numberOfReallocationCandidateLogicalSectors, "Number Of Reallocation Candidate Logical Sectors", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, rotatingMediaStatistics, deviceStatictics->sataStatistics.numberOfHighPriorityUnloadEvents, "Number Of High Priority Unload Events", M_NULLPTR, false);

        json_object_object_add(rootNode, "Rotating Media Statistics", rotatingMediaStatistics);
    }

    if (deviceStatictics->sataStatistics.generalErrorsStatisticsSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *generalErrorStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalErrorStatistics, deviceStatictics->sataStatistics.numberOfReportedUncorrectableErrors, "Number Of Reported Uncorrectable Errors", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalErrorStatistics, deviceStatictics->sataStatistics.numberOfResetsBetweenCommandAcceptanceAndCommandCompletion, "Number Of Resets Between Command Acceptance and Completion", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalErrorStatistics, deviceStatictics->sataStatistics.physicalElementStatusChanged, "Physical Element Status Changed", M_NULLPTR, false);

        json_object_object_add(rootNode, "General Error Statistics", generalErrorStatistics);
    }

    if (deviceStatictics->sataStatistics.temperatureStatisticsSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *temperatureStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_TEMPERATURE, temperatureStatistics, deviceStatictics->sataStatistics.currentTemperature, "Current Temperature", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_TEMPERATURE, temperatureStatistics, deviceStatictics->sataStatistics.averageShortTermTemperature, "Average Short Term Temperature", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_TEMPERATURE, temperatureStatistics, deviceStatictics->sataStatistics.averageLongTermTemperature, "Average Long Term Temperature", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_TEMPERATURE, temperatureStatistics, deviceStatictics->sataStatistics.highestTemperature, "Highest Temperature", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_TEMPERATURE, temperatureStatistics, deviceStatictics->sataStatistics.lowestTemperature, "Lowest Temperature", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_TEMPERATURE, temperatureStatistics, deviceStatictics->sataStatistics.highestAverageShortTermTemperature, "Highest Average Short Term Temperature", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_TEMPERATURE, temperatureStatistics, deviceStatictics->sataStatistics.lowestAverageShortTermTemperature, "Lowest Average Short Term Temperature", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_TEMPERATURE, temperatureStatistics, deviceStatictics->sataStatistics.highestAverageLongTermTemperature, "Highest Average Long Term Temperature", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_TEMPERATURE, temperatureStatistics, deviceStatictics->sataStatistics.lowestAverageLongTermTemperature, "Lowest Average Long Term Temperature", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_TIME_MINUTES, temperatureStatistics, deviceStatictics->sataStatistics.timeInOverTemperature, "Time In Over Temperature", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_TEMPERATURE, temperatureStatistics, deviceStatictics->sataStatistics.specifiedMaximumOperatingTemperature, "Specified Maximum Operating Temperature", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_TIME_MINUTES, temperatureStatistics, deviceStatictics->sataStatistics.timeInUnderTemperature, "Time In Under Temperature", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_TEMPERATURE, temperatureStatistics, deviceStatictics->sataStatistics.specifiedMinimumOperatingTemperature, "Specified Minimum Operating Temperature", M_NULLPTR, false);

        json_object_object_add(rootNode, "Temperature Statistics", temperatureStatistics);
    }

    if (deviceStatictics->sataStatistics.transportStatisticsSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *transportStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, transportStatistics, deviceStatictics->sataStatistics.numberOfHardwareResets, "Number Of Hardware Resets", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, transportStatistics, deviceStatictics->sataStatistics.numberOfASREvents, "Number Of ASR Events", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, transportStatistics, deviceStatictics->sataStatistics.numberOfInterfaceCRCErrors, "Number Of Interface CRC Errors", M_NULLPTR, false);

        json_object_object_add(rootNode, "Transport Statistics", transportStatistics);
    }

    if (deviceStatictics->sataStatistics.ssdStatisticsSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *ssdStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, ssdStatistics, deviceStatictics->sataStatistics.percentageUsedIndicator, "Percent Used Indicator", "%", false);

        json_object_object_add(rootNode, "Solid State Device Statistics", ssdStatistics);
    }

    if (deviceStatictics->sataStatistics.zonedDeviceStatisticsSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *zonedDeviceStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sataStatistics.maximumOpenZones, "Maximum Open Zones", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sataStatistics.maximumExplicitlyOpenZones, "Maximum Explicitly Open Zones", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sataStatistics.maximumImplicitlyOpenZones, "Maximum Implicitly Open Zones", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sataStatistics.minimumEmptyZones, "Minumum Empty Zones", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sataStatistics.maximumNonSequentialZones, "Maximum Non-sequential Zones", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sataStatistics.zonesEmptied, "Zones Emptied", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sataStatistics.suboptimalWriteCommands, "Suboptimal Write Commands", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sataStatistics.commandsExceedingOptimalLimit, "Commands Exceeding Optimal Limit", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sataStatistics.failedExplicitOpens, "Failed Explicit Opens", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sataStatistics.readRuleViolations, "Read Rule Violations", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sataStatistics.writeRuleViolations, "Write Rule Violations", M_NULLPTR, false);

        json_object_object_add(rootNode, "Zoned Device Statistics", zonedDeviceStatistics);
    }

    if (deviceStatictics->sataStatistics.vendorSpecificStatisticsSupported)
    {
        json_object *vendorSpecificStatistics = json_object_new_object();

        for (uint8_t vendorSpecificIter = 0, statisticsFound = 0; vendorSpecificIter < 64 && statisticsFound < deviceStatictics->sataStatistics.vendorSpecificStatisticsPopulated; ++vendorSpecificIter)
        {
            if (deviceStatictics->sataStatistics.vendorSpecificStatistics[vendorSpecificIter].isSupported)
            {
                atleastOneStatisticsAvailable = true;
                DECLARE_ZERO_INIT_ARRAY(char, statisticName, 64);
                if (SEAGATE == is_Seagate_Family(device))
                {
                    switch (vendorSpecificIter + 1)
                    {
                    case 1://pressure
                        snprintf(statisticName, 64, "Pressure Min/Max Reached");
                        break;
                    default:
                        snprintf(statisticName, 64, "Vendor Specific Statistic %" PRIu8, vendorSpecificIter + 1);
                        break;
                    }
                }
                else
                {
                    snprintf(statisticName, 64, "Vendor Specific Statistic %" PRIu8, vendorSpecificIter + 1);
                }

                create_Node_For_Statistic(STATISTICS_TYPE_COUNT, vendorSpecificStatistics, deviceStatictics->sataStatistics.vendorSpecificStatistics[vendorSpecificIter], statisticName, M_NULLPTR, false);
                ++statisticsFound;
            }
        }

        DECLARE_ZERO_INIT_ARRAY(char, statisticName, 30);
        if (SEAGATE == is_Seagate_Family(device))
        {
            snprintf(statisticName, 30, "Seagate Specific Statistics");
        }
        else
        {
            snprintf(statisticName, 30, "Vendor Specific Statistics");
        }

        json_object_object_add(rootNode, statisticName, vendorSpecificStatistics);
    }

    if (seagateDeviceStatisticsAvailable)
    {
        uint8_t maxLogEntries = seagateDeviceStatistics->sataStatistics.version;
        if (maxLogEntries != 0)
        {
            atleastOneStatisticsAvailable = true;

            json_object *segateDeviceStatistics = json_object_new_object();

            for (uint8_t logEntry = 0; logEntry < maxLogEntries; ++logEntry)
            {
                switch (logEntry)
                {
                case 0:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_COUNT, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.sanitizeCryptoErasePassCount, "Sanitize Crypto Erase Count");
                    break;

                case 1:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_TIME_MINUTES, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.sanitizeCryptoErasePassTimeStamp, "Sanitize Crypto Erase Timestamp");
                    break;

                case 2:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_COUNT, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.sanitizeOverwriteErasePassCount, "Sanitize Overwrite Erase Count");
                    break;

                case 3:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_TIME_MINUTES, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.sanitizeOverwriteErasePassTimeStamp, "Sanitize Overwrite Erase Timestamp");
                    break;

                case 4:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_COUNT, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.sanitizeBlockErasePassCount, "Sanitize Block Erase Count");
                    break;

                case 5:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_TIME_MINUTES, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.sanitizeBlockErasePassTimeStamp, "Sanitize Block Erase Timestamp");
                    break;

                case 6:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_COUNT, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.ataSecurityEraseUnitPassCount, "ATA Security Erase Unit Count");
                    break;

                case 7:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_TIME_MINUTES, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.ataSecurityEraseUnitPassTimeStamp, "ATA Security Erase Unit Timestamp");
                    break;

                case 8:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_COUNT, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.eraseSecurityFileFailureCount, "Erase Security File Failure Count");
                    break;

                case 9:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_TIME_MINUTES, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.eraseSecurityFileFailureTimeStamp, "Erase Security File Failure Timestamp");
                    break;

                case 10:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_COUNT, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.ataSecurityEraseUnitEnhancedPassCount, "ATA Security Erase Unit Enhanced Count");
                    break;

                case 11:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_TIME_MINUTES, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.ataSecurityEraseUnitEnhancedPassTimeStamp, "ATA Security Erase Unit Enhanced Timestamp");
                    break;

                case 12:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_COUNT, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.sanitizeCryptoEraseFailCount, "Sanitize Crypto Erase Failure Count");
                    break;

                case 13:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_TIME_MINUTES, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.sanitizeCryptoEraseFailTimeStamp, "Sanitize Crypto Erase Failure Timestamp");
                    break;

                case 14:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_COUNT, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.sanitizeOverwriteEraseFailCount, "Sanitize Overwrite Erase Failure Count");
                    break;

                case 15:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_TIME_MINUTES, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.sanitizeOverwriteEraseFailTimeStamp, "Sanitize Overwrite Erase Failure Timestamp");
                    break;

                case 16:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_COUNT, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.sanitizeBlockEraseFailCount, "Sanitize Block Erase Failure Count");
                    break;

                case 17:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_TIME_MINUTES, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.sanitizeBlockEraseFailTimeStamp, "Sanitize Block Erase Failure Timestamp");
                    break;

                case 18:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_COUNT, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.ataSecurityEraseUnitFailCount, "ATA Security Erase Unit Failure Count");
                    break;

                case 19:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_TIME_MINUTES, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.ataSecurityEraseUnitFailTimeStamp, "ATA Security Erase Unit Failure Timestamp");
                    break;

                case 20:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_COUNT, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.ataSecurityEraseUnitEnhancedFailCount, "ATA Security Erase Unit Enhanced Failure Count");
                    break;

                case 21:
                    create_Node_For_Seagate_Statistic(STATISTICS_TYPE_TIME_MINUTES, segateDeviceStatistics, seagateDeviceStatistics->sataStatistics.ataSecurityEraseUnitEnhancedFailTimeStamp, "ATA Security Erase Unit Enhanced Failure Timestamp");
                    break;

                default:
                    break;
                }
            }

            json_object_object_add(rootNode, "Seagate Device Statistics", segateDeviceStatistics);
        }
    }

    //Convert JSON object to formatted string
    const char *jstr = json_object_to_json_string_ext(rootNode, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);

    if (atleastOneStatisticsAvailable)
    {
        ret = SUCCESS;

        //copy the json output into string
        if (asprintf(jsonFormat, "%s", jstr) < 0)
        {
            ret = MEMORY_FAILURE;
        }
    }

    //Free the JSON object
    json_object_put(rootNode);

    return ret;
}

static eReturnValues create_JSON_File_For_SCSI_Device_Statistics(ptrDeviceStatistics deviceStatictics, ptrSeagateDeviceStatistics seagateDeviceStatistics, bool seagateDeviceStatisticsAvailable, char** jsonFormat)
{
    eReturnValues ret = NOT_SUPPORTED;
    bool atleastOneStatisticsAvailable = false;

    if (deviceStatictics == M_NULLPTR || (seagateDeviceStatisticsAvailable && seagateDeviceStatistics == M_NULLPTR))
    {
        return BAD_PARAMETER;
    }

    json_object *rootNode = json_object_new_object();

    if (rootNode == M_NULLPTR)
        return MEMORY_FAILURE;

    //Add version information
    json_object_object_add(rootNode, "Device Statistics Version", json_object_new_string(DEVICE_STATISTICS_JSON_VERSION));

    if (deviceStatictics->sasStatistics.writeErrorCountersSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *writeErrorCountersStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, writeErrorCountersStatistics, deviceStatictics->sasStatistics.writeErrorsCorrectedWithoutSubstantialDelay, "Write Errors Corrected Without Substantial Delay", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, writeErrorCountersStatistics, deviceStatictics->sasStatistics.writeErrorsCorrectedWithPossibleDelays, "Write Errors Corrected With Possible Delay", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, writeErrorCountersStatistics, deviceStatictics->sasStatistics.writeTotalReWrites, "Write Total Rewrites", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, writeErrorCountersStatistics, deviceStatictics->sasStatistics.writeErrorsCorrected, "Write Errors Corrected", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, writeErrorCountersStatistics, deviceStatictics->sasStatistics.writeTotalTimeCorrectionAlgorithmProcessed, "Write Total Times Corrective Algorithm Processed", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, writeErrorCountersStatistics, deviceStatictics->sasStatistics.writeTotalBytesProcessed, "Write Total Bytes Processed", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, writeErrorCountersStatistics, deviceStatictics->sasStatistics.writeTotalUncorrectedErrors, "Write Total Uncorrected Errors", M_NULLPTR, false);

        json_object_object_add(rootNode, "Write Error Counters", writeErrorCountersStatistics);
    }

    if (deviceStatictics->sasStatistics.readErrorCountersSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *readErrorCountersStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, readErrorCountersStatistics, deviceStatictics->sasStatistics.readErrorsCorrectedWithPossibleDelays, "Read Errors Corrected With Possible Delay", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, readErrorCountersStatistics, deviceStatictics->sasStatistics.readTotalRereads, "Read Total Rereads", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, readErrorCountersStatistics, deviceStatictics->sasStatistics.readErrorsCorrected, "Read Errors Corrected", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, readErrorCountersStatistics, deviceStatictics->sasStatistics.readTotalTimeCorrectionAlgorithmProcessed, "Read Total Times Corrective Algorithm Processed", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, readErrorCountersStatistics, deviceStatictics->sasStatistics.readTotalBytesProcessed, "Read Total Bytes Processed", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, readErrorCountersStatistics, deviceStatictics->sasStatistics.readTotalUncorrectedErrors, "Read Total Uncorrected Errors", M_NULLPTR, false);

        json_object_object_add(rootNode, "Read Error Counters", readErrorCountersStatistics);
    }

    if (deviceStatictics->sasStatistics.readReverseErrorCountersSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *readReverseErrorCountersStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, readReverseErrorCountersStatistics, deviceStatictics->sasStatistics.readReverseErrorsCorrectedWithoutSubstantialDelay, "Read Reverse Errors Corrected Without Substantial Delay", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, readReverseErrorCountersStatistics, deviceStatictics->sasStatistics.readReverseErrorsCorrectedWithPossibleDelays, "Read Reverse Errors Corrected With Possible Delay", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, readReverseErrorCountersStatistics, deviceStatictics->sasStatistics.readReverseTotalReReads, "Read Reverse Total Rereads", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, readReverseErrorCountersStatistics, deviceStatictics->sasStatistics.readReverseErrorsCorrected, "Read Reverse Errors Corrected", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, readReverseErrorCountersStatistics, deviceStatictics->sasStatistics.readReverseTotalTimeCorrectionAlgorithmProcessed, "Read Reverse Total Times Corrective Algorithm Processed", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, readReverseErrorCountersStatistics, deviceStatictics->sasStatistics.readReverseTotalBytesProcessed, "Read Reverse Total Bytes Processed", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, readReverseErrorCountersStatistics, deviceStatictics->sasStatistics.readReverseTotalUncorrectedErrors, "Read Reverse Total Uncorrected Errors", M_NULLPTR, false);

        json_object_object_add(rootNode, "Read Reverse Error Counters", readReverseErrorCountersStatistics);
    }

    if (deviceStatictics->sasStatistics.verifyErrorCountersSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *verifyErrorCountersStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, verifyErrorCountersStatistics, deviceStatictics->sasStatistics.verifyErrorsCorrectedWithoutSubstantialDelay, "Verify Errors Corrected Without Substantial Delay", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, verifyErrorCountersStatistics, deviceStatictics->sasStatistics.verifyErrorsCorrectedWithPossibleDelays, "Verify Errors Corrected With Possible Delay", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, verifyErrorCountersStatistics, deviceStatictics->sasStatistics.verifyTotalReVerifies, "Verify Total Rereads", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, verifyErrorCountersStatistics, deviceStatictics->sasStatistics.verifyErrorsCorrected, "Verify Errors Corrected", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, verifyErrorCountersStatistics, deviceStatictics->sasStatistics.verifyTotalTimeCorrectionAlgorithmProcessed, "Verify Total Times Corrective Algorithm Processed", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, verifyErrorCountersStatistics, deviceStatictics->sasStatistics.verifyTotalBytesProcessed, "Verify Total Bytes Processed", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, verifyErrorCountersStatistics, deviceStatictics->sasStatistics.verifyTotalUncorrectedErrors, "Verify Total Uncorrected Errors", M_NULLPTR, false);

        json_object_object_add(rootNode, "Verify Error Counters", verifyErrorCountersStatistics);
    }

    if (deviceStatictics->sasStatistics.nonMediumErrorSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *nonMediumErrorStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, nonMediumErrorStatistics, deviceStatictics->sasStatistics.nonMediumErrorCount, "Non-Medium Error Count", M_NULLPTR, false);

        json_object_object_add(rootNode, "Non Medium Error", nonMediumErrorStatistics);
    }

    if (deviceStatictics->sasStatistics.formatStatusSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *formatStatusStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, formatStatusStatistics, deviceStatictics->sasStatistics.grownDefectsDuringCertification, "Grown Defects During Certification", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, formatStatusStatistics, deviceStatictics->sasStatistics.totalBlocksReassignedDuringFormat, "Total Blocks Reassigned During Format", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, formatStatusStatistics, deviceStatictics->sasStatistics.totalNewBlocksReassigned, "Total New Blocks Reassigned", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, formatStatusStatistics, deviceStatictics->sasStatistics.powerOnMinutesSinceFormat, "Power On Minutes Since Last Format", "minutes", false);

        json_object_object_add(rootNode, "Format Status", formatStatusStatistics);
    }

    if (deviceStatictics->sasStatistics.logicalBlockProvisioningSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *logicalBlockProvisioningStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, logicalBlockProvisioningStatistics, deviceStatictics->sasStatistics.availableLBAMappingresourceCount, "Available LBA Mapping Resource Count", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, logicalBlockProvisioningStatistics, deviceStatictics->sasStatistics.usedLBAMappingResourceCount, "Used LBA Mapping Resource Count", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, logicalBlockProvisioningStatistics, deviceStatictics->sasStatistics.availableProvisioningResourcePercentage, "Available Provisioning Resource Percentage", "%", false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, logicalBlockProvisioningStatistics, deviceStatictics->sasStatistics.deduplicatedLBAResourceCount, "De-duplicted LBA Resource Count", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, logicalBlockProvisioningStatistics, deviceStatictics->sasStatistics.compressedLBAResourceCount, "Compressed LBA Resource Count", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, logicalBlockProvisioningStatistics, deviceStatictics->sasStatistics.totalEfficiencyLBAResourceCount, "Total Efficiency LBA Resource Count", M_NULLPTR, false);

        json_object_object_add(rootNode, "Logical Block Provisioning", logicalBlockProvisioningStatistics);
    }

    if (deviceStatictics->sasStatistics.temperatureSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *temperatureStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_TEMPERATURE, temperatureStatistics, deviceStatictics->sasStatistics.temperature, "Temperature", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_TEMPERATURE, temperatureStatistics, deviceStatictics->sasStatistics.referenceTemperature, "Reference Temperature", M_NULLPTR, false);

        json_object_object_add(rootNode, "Temperature", temperatureStatistics);
    }

    if (deviceStatictics->sasStatistics.environmentReportingSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *environmentalReportingStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_ENVIRONMENTAL_TEMPERATURE, environmentalReportingStatistics, deviceStatictics->sasStatistics.currentTemperature, "Temperature", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_ENVIRONMENTAL_TEMPERATURE, environmentalReportingStatistics, deviceStatictics->sasStatistics.lifetimeMaximumTemperature, "Lifetime Maximum Temperature", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_ENVIRONMENTAL_TEMPERATURE, environmentalReportingStatistics, deviceStatictics->sasStatistics.lifetimeMinimumTemperature, "Lifetime Minimum Temperature", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_ENVIRONMENTAL_TEMPERATURE, environmentalReportingStatistics, deviceStatictics->sasStatistics.maximumTemperatureSincePowerOn, "Maximum Temperature Since Power On", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_ENVIRONMENTAL_TEMPERATURE, environmentalReportingStatistics, deviceStatictics->sasStatistics.minimumTemperatureSincePowerOn, "Minimum Temperature Since Power On", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_ENVIRONMENTAL_TEMPERATURE, environmentalReportingStatistics, deviceStatictics->sasStatistics.maximumOtherTemperature, "Maximum Other Temperature", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_ENVIRONMENTAL_TEMPERATURE, environmentalReportingStatistics, deviceStatictics->sasStatistics.minimumOtherTemperature, "Minimum Other Temperature", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_HUMIDITY, environmentalReportingStatistics, deviceStatictics->sasStatistics.currentRelativeHumidity, "Relative Humidity", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_HUMIDITY, environmentalReportingStatistics, deviceStatictics->sasStatistics.lifetimeMaximumRelativeHumidity, "Lifetime Maximum Relative Humidity", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_HUMIDITY, environmentalReportingStatistics, deviceStatictics->sasStatistics.lifetimeMinumumRelativeHumidity, "Lifetime Minimum Relative Humidity", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_HUMIDITY, environmentalReportingStatistics, deviceStatictics->sasStatistics.maximumRelativeHumiditySincePoweron, "Maximum Relative Humidity Since Power On", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_HUMIDITY, environmentalReportingStatistics, deviceStatictics->sasStatistics.minimumRelativeHumiditySincePoweron, "Minimum Relative Humidity Since Power On", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_HUMIDITY, environmentalReportingStatistics, deviceStatictics->sasStatistics.maximumOtherRelativeHumidity, "Maximum Other Relative Humidity", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_HUMIDITY, environmentalReportingStatistics, deviceStatictics->sasStatistics.minimumOtherRelativeHumidity, "Minimum Other Relative Humidity", M_NULLPTR, false);

        json_object_object_add(rootNode, "Environmental Reporting", environmentalReportingStatistics);
    }

    if (deviceStatictics->sasStatistics.environmentReportingSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *environmentalLimitsStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_ENVIRONMENTAL_TEMPERATURE, environmentalLimitsStatistics, deviceStatictics->sasStatistics.highCriticalTemperatureLimitTrigger, "High Critical Temperature Limit Trigger", M_NULLPTR, true);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_ENVIRONMENTAL_TEMPERATURE, environmentalLimitsStatistics, deviceStatictics->sasStatistics.highCriticalTemperatureLimitReset, "High Critical Temperature Limit Reset", M_NULLPTR, true);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_ENVIRONMENTAL_TEMPERATURE, environmentalLimitsStatistics, deviceStatictics->sasStatistics.lowCriticalTemperatureLimitReset, "Low Critical Temperature Limit Reset", M_NULLPTR, true);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_ENVIRONMENTAL_TEMPERATURE, environmentalLimitsStatistics, deviceStatictics->sasStatistics.lowCriticalTemperatureLimitTrigger, "Low Critical Temperature Limit Trigger", M_NULLPTR, true);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_ENVIRONMENTAL_TEMPERATURE, environmentalLimitsStatistics, deviceStatictics->sasStatistics.highOperatingTemperatureLimitTrigger, "High Operating Temperature Limit Trigger", M_NULLPTR, true);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_ENVIRONMENTAL_TEMPERATURE, environmentalLimitsStatistics, deviceStatictics->sasStatistics.highOperatingTemperatureLimitReset, "High Operating Temperature Limit Reset", M_NULLPTR, true);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_ENVIRONMENTAL_TEMPERATURE, environmentalLimitsStatistics, deviceStatictics->sasStatistics.lowOperatingTemperatureLimitReset, "Low Operating Temperature Limit Reset", M_NULLPTR, true);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_ENVIRONMENTAL_TEMPERATURE, environmentalLimitsStatistics, deviceStatictics->sasStatistics.lowOperatingTemperatureLimitTrigger, "Low Operating Temperature Limit Trigger", M_NULLPTR, true);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_HUMIDITY, environmentalLimitsStatistics, deviceStatictics->sasStatistics.highCriticalHumidityLimitTrigger, "High Critical Relative Humidity Limit Trigger", M_NULLPTR, true);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_HUMIDITY, environmentalLimitsStatistics, deviceStatictics->sasStatistics.highCriticalHumidityLimitReset, "High Critical Relative Humidity Limit Reset", M_NULLPTR, true);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_HUMIDITY, environmentalLimitsStatistics, deviceStatictics->sasStatistics.lowCriticalHumidityLimitReset, "Low Critical Relative Humidity Limit Reset", M_NULLPTR, true);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_HUMIDITY, environmentalLimitsStatistics, deviceStatictics->sasStatistics.lowCriticalHumidityLimitTrigger, "Low Critical Relative Humidity Limit Trigger", M_NULLPTR, true);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_HUMIDITY, environmentalLimitsStatistics, deviceStatictics->sasStatistics.highOperatingHumidityLimitTrigger, "High Operating Relative Humidity Limit Trigger", M_NULLPTR, true);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_HUMIDITY, environmentalLimitsStatistics, deviceStatictics->sasStatistics.highOperatingHumidityLimitReset, "High Operating Relative Humidity Limit Reset", M_NULLPTR, true);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_HUMIDITY, environmentalLimitsStatistics, deviceStatictics->sasStatistics.lowOperatingHumidityLimitReset, "Low Operating Relative Humidity Limit Reset", M_NULLPTR, true);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_HUMIDITY, environmentalLimitsStatistics, deviceStatictics->sasStatistics.lowOperatingHumidityLimitTrigger, "Low Operating Relative Humidity Limit Trigger", M_NULLPTR, true);

        json_object_object_add(rootNode, "Environmental Limits", environmentalLimitsStatistics);
    }

    if (deviceStatictics->sasStatistics.startStopCycleCounterSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *startStopCycleCounterStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_DATE, startStopCycleCounterStatistics, deviceStatictics->sasStatistics.dateOfManufacture, "Date Of Manufacture", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_DATE, startStopCycleCounterStatistics, deviceStatictics->sasStatistics.accountingDate, "Accounting Date", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, startStopCycleCounterStatistics, deviceStatictics->sasStatistics.specifiedCycleCountOverDeviceLifetime, "Specified Cycle Count Over Device Lifetime", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, startStopCycleCounterStatistics, deviceStatictics->sasStatistics.accumulatedStartStopCycles, "Accumulated Start-Stop Cycles", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, startStopCycleCounterStatistics, deviceStatictics->sasStatistics.specifiedLoadUnloadCountOverDeviceLifetime, "Specified Load-Unload Count Over Device Lifetime", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, startStopCycleCounterStatistics, deviceStatictics->sasStatistics.accumulatedLoadUnloadCycles, "Accumulated Load-Unload Cycles", M_NULLPTR, false);

        json_object_object_add(rootNode, "Start-Stop Cycle Counter", startStopCycleCounterStatistics);
    }

    if (deviceStatictics->sasStatistics.powerConditionTransitionsSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *powerConditionTransitionStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, powerConditionTransitionStatistics, deviceStatictics->sasStatistics.transitionsToActive, "Accumulated Transitions to Active", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, powerConditionTransitionStatistics, deviceStatictics->sasStatistics.transitionsToIdleA, "Accumulated Transitions to Idle A", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, powerConditionTransitionStatistics, deviceStatictics->sasStatistics.transitionsToIdleB, "Accumulated Transitions to Idle B", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, powerConditionTransitionStatistics, deviceStatictics->sasStatistics.transitionsToIdleC, "Accumulated Transitions to Idle C", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, powerConditionTransitionStatistics, deviceStatictics->sasStatistics.transitionsToStandbyZ, "Accumulated Transitions to Standby Z", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, powerConditionTransitionStatistics, deviceStatictics->sasStatistics.transitionsToStandbyY, "Accumulated Transitions to Standby Y", M_NULLPTR, false);

        json_object_object_add(rootNode, "Power Condition Transitions", powerConditionTransitionStatistics);
    }

    if (deviceStatictics->sasStatistics.utilizationSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *utilizationStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_WORKLOAD_UTILIZATION, utilizationStatistics, deviceStatictics->sasStatistics.workloadUtilization, "Workload Utilization", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_UTILIZATION_USAGE_RATE, utilizationStatistics, deviceStatictics->sasStatistics.utilizationUsageRateBasedOnDateAndTime, "Utilization Usage Rate", M_NULLPTR, false);

        json_object_object_add(rootNode, "Utilization", utilizationStatistics);
    }

    if (deviceStatictics->sasStatistics.solidStateMediaSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *solidStateMediaStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, solidStateMediaStatistics, deviceStatictics->sasStatistics.percentUsedEndurance, "Percent Used Endurance", "%", false);

        json_object_object_add(rootNode, "Solid State Media", solidStateMediaStatistics);
    }

    if (deviceStatictics->sasStatistics.backgroundScanResultsSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *backgroundScanResultsStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, backgroundScanResultsStatistics, deviceStatictics->sasStatistics.accumulatedPowerOnMinutes, "Accumulated Power On Minutes", "minutes", false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, backgroundScanResultsStatistics, deviceStatictics->sasStatistics.numberOfBackgroundScansPerformed, "Number Of Background Scans Performed", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, backgroundScanResultsStatistics, deviceStatictics->sasStatistics.numberOfBackgroundMediaScansPerformed, "Number Of Background Media Scans Performed", M_NULLPTR, false);

        json_object_object_add(rootNode, "Background Scan Results", backgroundScanResultsStatistics);
    }

    if (deviceStatictics->sasStatistics.defectStatisticsSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *defectStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, defectStatistics, deviceStatictics->sasStatistics.grownDefects, "Grown Defects", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, defectStatistics, deviceStatictics->sasStatistics.primaryDefects, "Primary Defects", M_NULLPTR, false);

        json_object_object_add(rootNode, "Defect Statistics", defectStatistics);
    }

    if (deviceStatictics->sasStatistics.pendingDefectsSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *pendingDefectStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, pendingDefectStatistics, deviceStatictics->sasStatistics.pendingDefectCount, "Pending Defect Count", M_NULLPTR, false);

        json_object_object_add(rootNode, "Pending Defect", pendingDefectStatistics);
    }

    if (deviceStatictics->sasStatistics.lpsMisalignmentSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *lpsMisalignmentStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, lpsMisalignmentStatistics, deviceStatictics->sasStatistics.lpsMisalignmentCount, "LPS Misalignment Count", M_NULLPTR, false);

        json_object_object_add(rootNode, "LPS Misalignment", lpsMisalignmentStatistics);
    }

    if (deviceStatictics->sasStatistics.nvCacheSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *lpsMisalignmentStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_NON_VOLATILE_TIME, lpsMisalignmentStatistics, deviceStatictics->sasStatistics.remainingNonvolatileTime, "Remaining Non-Volatile Time", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_NON_VOLATILE_TIME, lpsMisalignmentStatistics, deviceStatictics->sasStatistics.maximumNonvolatileTime, "Maximum Non-Volatile Time", M_NULLPTR, false);

        json_object_object_add(rootNode, "Non-Volatile Cache", lpsMisalignmentStatistics);
    }

    if (deviceStatictics->sasStatistics.generalStatisticsAndPerformanceSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *generalAndPerformanceStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalAndPerformanceStatistics, deviceStatictics->sasStatistics.numberOfReadCommands, "Number Of Read Commands", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalAndPerformanceStatistics, deviceStatictics->sasStatistics.numberOfWriteCommands, "Number Of Write Commands", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalAndPerformanceStatistics, deviceStatictics->sasStatistics.numberOfLogicalBlocksReceived, "Number Of Logical Blocks Received", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalAndPerformanceStatistics, deviceStatictics->sasStatistics.numberOfLogicalBlocksTransmitted, "Number Of Logical Blocks Transmitted", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalAndPerformanceStatistics, deviceStatictics->sasStatistics.readCommandProcessingIntervals, "Read Command Processing Intervals", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalAndPerformanceStatistics, deviceStatictics->sasStatistics.writeCommandProcessingIntervals, "Write Command Processing Intervals", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalAndPerformanceStatistics, deviceStatictics->sasStatistics.weightedNumberOfReadCommandsPlusWriteCommands, "Weighted Number Of Read Commands Plus Write Commands", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalAndPerformanceStatistics, deviceStatictics->sasStatistics.weightedReadCommandProcessingPlusWriteCommandProcessing, "Weighted Number Of Read Command Processing Plus Write Command Processing", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalAndPerformanceStatistics, deviceStatictics->sasStatistics.idleTimeIntervals, "Idle Time Intervals", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_TIME_INTERVAL, generalAndPerformanceStatistics, deviceStatictics->sasStatistics.timeIntervalDescriptor, "Time Interval Desriptor", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalAndPerformanceStatistics, deviceStatictics->sasStatistics.numberOfReadFUACommands, "Number Of Read FUA Commands", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalAndPerformanceStatistics, deviceStatictics->sasStatistics.numberOfWriteFUACommands, "Number Of Write FUA Commands", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalAndPerformanceStatistics, deviceStatictics->sasStatistics.numberOfReadFUANVCommands, "Number Of Read FUA NV Commands", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalAndPerformanceStatistics, deviceStatictics->sasStatistics.numberOfWriteFUANVCommands, "Number Of Write FUA NV Commands", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalAndPerformanceStatistics, deviceStatictics->sasStatistics.readFUACommandProcessingIntervals, "Read FUA Command Processing Intervals", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalAndPerformanceStatistics, deviceStatictics->sasStatistics.writeFUACommandProcessingIntervals, "Write FUA Command Processing Intervals", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalAndPerformanceStatistics, deviceStatictics->sasStatistics.readFUANVCommandProcessingIntervals, "Read FUA NV Command Processing Intervals", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, generalAndPerformanceStatistics, deviceStatictics->sasStatistics.writeFUANVCommandProcessingIntervals, "Write FUA NV Command Processing Intervals", M_NULLPTR, false);

        json_object_object_add(rootNode, "General Statistics And Performance", generalAndPerformanceStatistics);
    }

    if (deviceStatictics->sasStatistics.cacheMemoryStatisticsSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *cacheMemoryStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, cacheMemoryStatistics, deviceStatictics->sasStatistics.readCacheMemoryHits, "Read Cache Memory Hits", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, cacheMemoryStatistics, deviceStatictics->sasStatistics.readsToCacheMemory, "Reads To Cache Memory", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, cacheMemoryStatistics, deviceStatictics->sasStatistics.writeCacheMemoryHits, "Write Cache Memory Hits", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, cacheMemoryStatistics, deviceStatictics->sasStatistics.writesFromCacheMemory, "Writes From Cache Memory", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, cacheMemoryStatistics, deviceStatictics->sasStatistics.timeFromLastHardReset, "Last Hard Reset Intervals", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_SCSI_TIME_INTERVAL, cacheMemoryStatistics, deviceStatictics->sasStatistics.cacheTimeInterval, "Cache Memory Time Interval", M_NULLPTR, false);

        json_object_object_add(rootNode, "Cache Memory Statistics", cacheMemoryStatistics);
    }

    if (deviceStatictics->sasStatistics.timeStampSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *dateAndTimeStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_DATA_AND_TIME_TIMESTAMP, dateAndTimeStatistics, deviceStatictics->sasStatistics.dateAndTimeTimestamp, "Date And Time Timestamp", M_NULLPTR, false);

        json_object_object_add(rootNode, "Timestamp", dateAndTimeStatistics);
    }

    if (deviceStatictics->sasStatistics.zonedDeviceStatisticsSupported)
    {
        atleastOneStatisticsAvailable = true;
        json_object *zonedDeviceStatistics = json_object_new_object();

        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sasStatistics.maximumOpenZones, "Maximum Open Zones", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sasStatistics.maximumExplicitlyOpenZones, "Maximum Explicitly Open Zones", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sasStatistics.maximumImplicitlyOpenZones, "Maximum Implicitly Open Zones", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sasStatistics.minimumEmptyZones, "Minumum Empty Zones", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sasStatistics.maximumNonSequentialZones, "Maximum Non-sequential Zones", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sasStatistics.zonesEmptied, "Zones Emptied", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sasStatistics.suboptimalWriteCommands, "Suboptimal Write Commands", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sasStatistics.commandsExceedingOptimalLimit, "Commands Exceeding Optimal Limit", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sasStatistics.failedExplicitOpens, "Failed Explicit Opens", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sasStatistics.readRuleViolations, "Read Rule Violations", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sasStatistics.writeRuleViolations, "Write Rule Violations", M_NULLPTR, false);
        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, zonedDeviceStatistics, deviceStatictics->sasStatistics.maxImplicitlyOpenSeqOrBeforeReqZones, "Maximum Implicitly Open Sequential Or Before Required Zones", M_NULLPTR, false);

        json_object_object_add(rootNode, "Zoned Device Statistics", zonedDeviceStatistics);
    }

    if (deviceStatictics->sasStatistics.protocolSpecificStatisticsSupported && deviceStatictics->sasStatistics.protocolStatisticsType == STAT_PROT_SAS)
    {
        json_object *sasProtocolStatistics = json_object_new_object();

        for (uint16_t portIter = 0; portIter < SAS_STATISTICS_MAX_PORTS && portIter < deviceStatictics->sasStatistics.sasProtStats.portCount; ++portIter)
        {
            if (deviceStatictics->sasStatistics.sasProtStats.sasStatsPerPort[portIter].sasProtStatsValid)
            {
                for (uint8_t phyIter = 0; phyIter < SAS_STATISTICS_MAX_PHYS && phyIter < deviceStatictics->sasStatistics.sasProtStats.sasStatsPerPort[portIter].phyCount; ++phyIter)
                {
                    if (deviceStatictics->sasStatistics.sasProtStats.sasStatsPerPort[portIter].perPhy[phyIter].sasPhyStatsValid)
                    {
                        atleastOneStatisticsAvailable = true;
                        json_object *portPhyNode = json_object_new_object();

                        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, portPhyNode, deviceStatictics->sasStatistics.sasProtStats.sasStatsPerPort[portIter].perPhy[phyIter].invalidDWORDCount, "Invalid Dword Count", M_NULLPTR, false);
                        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, portPhyNode, deviceStatictics->sasStatistics.sasProtStats.sasStatsPerPort[portIter].perPhy[phyIter].runningDisparityErrorCount, "Running Disparit Error Count", M_NULLPTR, false);
                        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, portPhyNode, deviceStatictics->sasStatistics.sasProtStats.sasStatsPerPort[portIter].perPhy[phyIter].lossOfDWORDSynchronizationCount, "Loss of Dword Snchronization Count", M_NULLPTR, false);
                        create_Node_For_Statistic(STATISTICS_TYPE_COUNT, portPhyNode, deviceStatictics->sasStatistics.sasProtStats.sasStatsPerPort[portIter].perPhy[phyIter].phyResetProblemCount, "Phy Reset Problem Count", M_NULLPTR, false);

                        DECLARE_ZERO_INIT_ARRAY(char, portPhyNameName, 30);
                        snprintf(portPhyNameName, 30, "Port:%" PRIu16 "-Phy:%" PRIu16 "", deviceStatictics->sasStatistics.sasProtStats.sasStatsPerPort[portIter].portID, deviceStatictics->sasStatistics.sasProtStats.sasStatsPerPort[portIter].perPhy[phyIter].phyID);
                        json_object_object_add(sasProtocolStatistics, portPhyNameName, portPhyNode);
                    }
                }
            }
        }

        json_object_object_add(rootNode, "SAS Protocol Statistics", sasProtocolStatistics);
    }

    if (seagateDeviceStatisticsAvailable)
    {
        atleastOneStatisticsAvailable = true;
        json_object *segateDeviceStatistics = json_object_new_object();

        create_Node_For_Seagate_Statistic(STATISTICS_TYPE_COUNT, segateDeviceStatistics, seagateDeviceStatistics->sasStatistics.sanitizeCryptoEraseCount, "Sanitize Crypo Erase Count");
        create_Node_For_Seagate_Statistic(STATISTICS_TYPE_TIME_MINUTES, segateDeviceStatistics, seagateDeviceStatistics->sasStatistics.sanitizeCryptoEraseTimeStamp, "Sanitize Crypo Erase Requested Time");

        create_Node_For_Seagate_Statistic(STATISTICS_TYPE_COUNT, segateDeviceStatistics, seagateDeviceStatistics->sasStatistics.sanitizeOverwriteEraseCount, "Sanitize Overwrite Erase Count");
        create_Node_For_Seagate_Statistic(STATISTICS_TYPE_TIME_MINUTES, segateDeviceStatistics, seagateDeviceStatistics->sasStatistics.sanitizeOverwriteEraseTimeStamp, "Sanitize Overwrite Erase Requested Time");

        create_Node_For_Seagate_Statistic(STATISTICS_TYPE_COUNT, segateDeviceStatistics, seagateDeviceStatistics->sasStatistics.sanitizeBlockEraseCount, "Sanitize Block Erase Count");
        create_Node_For_Seagate_Statistic(STATISTICS_TYPE_TIME_MINUTES, segateDeviceStatistics, seagateDeviceStatistics->sasStatistics.sanitizeBlockEraseTimeStamp, "Sanitize Block Erase Requested Time");

        create_Node_For_Seagate_Statistic(STATISTICS_TYPE_COUNT, segateDeviceStatistics, seagateDeviceStatistics->sasStatistics.eraseSecurityFileFailureCount, "Erase Security File Failures Count");
        create_Node_For_Seagate_Statistic(STATISTICS_TYPE_TIME_MINUTES, segateDeviceStatistics, seagateDeviceStatistics->sasStatistics.eraseSecurityFileFailureTimeStamp, "Erase Security File Failures Requested Time");

        json_object_object_add(rootNode, "Seagate Device Statistics", segateDeviceStatistics);
    }

    //Convert JSON object to formatted string
    const char *jstr = json_object_to_json_string_ext(rootNode, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);

    if (atleastOneStatisticsAvailable)
    {
        ret = SUCCESS;

        //copy the json output into string
        if (asprintf(jsonFormat, "%s", jstr) < 0)
        {
            ret = MEMORY_FAILURE;
        }
    }

    //Free the JSON object
    json_object_put(rootNode);

    return ret;
}

eReturnValues create_JSON_File_For_Device_Statistics(tDevice *device, ptrDeviceStatistics deviceStatictics, ptrSeagateDeviceStatistics seagateDeviceStatistics, bool seagateDeviceStatisticsAvailable, char** jsonFormat)
{
    eReturnValues ret = NOT_SUPPORTED;

    if (deviceStatictics == M_NULLPTR)
    {
        return BAD_PARAMETER;
    }

    if (device->drive_info.drive_type == ATA_DRIVE)
    {
        ret = create_JSON_File_For_ATA_Device_Statistics(device, deviceStatictics, seagateDeviceStatistics, seagateDeviceStatisticsAvailable, jsonFormat);
    }
    else if (device->drive_info.drive_type == SCSI_DRIVE)
    {
        ret = create_JSON_File_For_SCSI_Device_Statistics(deviceStatictics, seagateDeviceStatistics, seagateDeviceStatisticsAvailable, jsonFormat);
    }

    return ret;
}