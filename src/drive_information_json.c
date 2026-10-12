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
// \file drive_information_json.c
// \brief This file defines types and functions related to the JSON-based output for Drive Information.

#include "drive_information_json.h"
#include "common_types.h"
#include "drive_info.h"
#include "io_utils.h"
#include "string_utils.h"
#include "time_utils.h"
#include "unit_conversion.h"

#define COMBINE_DRIVE_INFORMATION_JSON_VERSIONS_(x, y, z) #x "." #y "." #z
#define COMBINE_DRIVE_INFORMATION_JSON_VERSIONS(x, y, z)  COMBINE_DRIVE_INFORMATION_JSON_VERSIONS_(x, y, z)

#define DRIVE_INFORMATION_JSON_MAJOR_VERSION              1
#define DRIVE_INFORMATION_JSON_MINOR_VERSION              0
#define DRIVE_INFORMATION_JSON_PATCH_VERSION              0

#define DRIVE_INFORMATION_JSON_VERSION                                                                                 \
    COMBINE_DRIVE_INFORMATION_JSON_VERSIONS(DRIVE_INFORMATION_JSON_MAJOR_VERSION,                                      \
                                            DRIVE_INFORMATION_JSON_MINOR_VERSION,                                      \
                                            DRIVE_INFORMATION_JSON_PATCH_VERSION)

#define MAX_UINT64_TO_DEC_STRING_LENGTH              21
#define MAX_UINT32_TO_DEC_STRING_LENGTH              11
#define MAX_UINT16_TO_DEC_STRING_LENGTH              6
#define MAX_INT16_TO_DEC_STRING_LENGTH               7
#define MAX_UINT8_TO_DEC_STRING_LENGTH               4
#define MAX_INT64_TO_DEC_STRING_LENGTH               21
#define MAX_DOUBLE_TO_DEC_STRING_LENGHT              21
#define MAX_UINT64_TO_HEX_STRING_LENGTH              19
#define MAX_UINT32_TO_HEX_STRING_LENGTH              11
#define MAX_UINT16_TO_HEX_STRING_LENGTH              7
#define MAX_UINT8_TO_HEX_STRING_LENGTH               5
#define MAX_BOOL_TO_BOOL_STRING_LENGTH               21
#define MAX_TIME_STRING_LENGTH                       (4 * MAX_UINT8_TO_DEC_STRING_LENGTH) + MAX_UINT16_TO_DEC_STRING_LENGTH + 34
#define MAX_DATE_OF_MANUFACTURE_LENGTH               7 + MAX_UINT8_TO_DEC_STRING_LENGTH + MAX_UINT16_TO_DEC_STRING_LENGTH

#define MAX_STRING_NVME_VERSION_LENGTH               2 + MAX_UINT16_TO_DEC_STRING_LENGTH + (2 * MAX_UINT8_TO_DEC_STRING_LENGTH)
#define MAX_STRING_DRIVE_CAPACITY_LENGTH             28
#define MAX_STRING_CAPACITY_LENGTH                   31
#define MAX_STRING_UNALLOCATED_NVM_CAPACITY_LENGTH   37
#define MAX_STRING_NATIVE_DRIVE_CAPACITY_LENGTH      35
#define MAX_STRING_DEFAULT_CHS_LENGTH                18
#define MAX_STRING_PORT_NAME_LENGTH                  9
#define MAX_STRING_INTERFACE_SPEED_LENGTH            23 + MAX_DOUBLE_TO_DEC_STRING_LENGHT
#define MAX_STRING_BYTES_READ_LENGTH                 24
#define MAX_STRING_BYTES_WRITTEN_LENGTH              26
#define MAX_STRING_CACHE_SIZE_LENGTH                 18
#define MAX_STRING_HYBRID_NAND_CACHE_SIZE_LENGTH     30
#define MAX_STRING_NAMESPACE_SIZE_LENGTH             27
#define MAX_STRING_NAMESPACE_UTILIZATION_SIZE_LENGTH 34
#define MAX_STRING_LOW_CURRENT_SPINUP_LENGTH         24 + MAX_UINT16_TO_HEX_STRING_LENGTH

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

#define RETURN_ON_DRIVE_JSON_ERROR(expression)                                                                         \
    do                                                                                                                 \
    {                                                                                                                  \
        if ((expression) != 0)                                                                                         \
        {                                                                                                              \
            return MEMORY_FAILURE;                                                                                     \
        }                                                                                                              \
    } while (0)

static void create_Time_String(char*    timeString,
                               uint8_t  years,
                               uint16_t days,
                               uint8_t  hours,
                               uint8_t  minutes,
                               uint8_t  seconds)
{
    DECLARE_ZERO_INIT_ARRAY(char, temp, MAX_TIME_STRING_LENGTH);
    if (years > 0)
    {
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(temp, MAX_TIME_STRING_LENGTH, " %" PRIu8 " year%s", years, (years > 1) ? "s" : ""),
            "maximum time component text fits in MAX_TIME_STRING_LENGTH");
        M_IGNORE_SAFE_ERRNO_CALL(safe_strcat(timeString, MAX_TIME_STRING_LENGTH, temp),
                                 "MAX_TIME_STRING_LENGTH accommodates all five time components");
    }
    if (days > 0)
    {
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(temp, MAX_TIME_STRING_LENGTH, " %" PRIu16 " day%s", days, (days > 1) ? "s" : ""),
            "maximum time component text fits in MAX_TIME_STRING_LENGTH");
        M_IGNORE_SAFE_ERRNO_CALL(safe_strcat(timeString, MAX_TIME_STRING_LENGTH, temp),
                                 "MAX_TIME_STRING_LENGTH accommodates all five time components");
    }
    if (hours > 0)
    {
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(temp, MAX_TIME_STRING_LENGTH, " %" PRIu8 " hour%s", hours, (hours > 1) ? "s" : ""),
            "maximum time component text fits in MAX_TIME_STRING_LENGTH");
        M_IGNORE_SAFE_ERRNO_CALL(safe_strcat(timeString, MAX_TIME_STRING_LENGTH, temp),
                                 "MAX_TIME_STRING_LENGTH accommodates all five time components");
    }
    if (minutes > 0)
    {
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(temp, MAX_TIME_STRING_LENGTH, " %" PRIu8 " minute%s", minutes,
                                                   (minutes > 1) ? "s" : ""),
                               "maximum time component text fits in MAX_TIME_STRING_LENGTH");
        M_IGNORE_SAFE_ERRNO_CALL(safe_strcat(timeString, MAX_TIME_STRING_LENGTH, temp),
                                 "MAX_TIME_STRING_LENGTH accommodates all five time components");
    }
    if (seconds > 0)
    {
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(temp, MAX_TIME_STRING_LENGTH, " %" PRIu8 " second%s", seconds,
                                                   (seconds > 1) ? "s" : ""),
                               "maximum time component text fits in MAX_TIME_STRING_LENGTH");
        M_IGNORE_SAFE_ERRNO_CALL(safe_strcat(timeString, MAX_TIME_STRING_LENGTH, temp),
                                 "MAX_TIME_STRING_LENGTH accommodates all five time components");
    }
}

M_NODISCARD static eReturnValues create_JSON_Node_For_SAS_Sata_Device_Information(json_object* rootObject,
                                                                                  ptrDriveInformationSAS_SATA driveInfo)
{
    double mCapacity = 0.0;
    double capacity  = 0.0;
    DECLARE_ZERO_INIT_ARRAY(char, mCapUnits, UNIT_STRING_LENGTH);
    DECLARE_ZERO_INIT_ARRAY(char, capUnits, UNIT_STRING_LENGTH);
    char* mCapUnit = &mCapUnits[0];
    char* capUnit  = &capUnits[0];

    if (safe_strlen(driveInfo->vendorID))
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Vendor ID", json_object_new_string(driveInfo->vendorID)));
    }
    RETURN_ON_DRIVE_JSON_ERROR(
        add_JSON_Object(rootObject, "Model Number", json_object_new_string(driveInfo->modelNumber)));
    RETURN_ON_DRIVE_JSON_ERROR(
        add_JSON_Object(rootObject, "Serial Number", json_object_new_string(driveInfo->serialNumber)));

    if (safe_strlen(driveInfo->pcbaSerialNumber))
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "PCBA Serial Number", json_object_new_string(driveInfo->pcbaSerialNumber)));
    }
    RETURN_ON_DRIVE_JSON_ERROR(
        add_JSON_Object(rootObject, "Firmware Revision", json_object_new_string(driveInfo->firmwareRevision)));

    if (safe_strlen(driveInfo->satVendorID))
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "SAT Vendor ID", json_object_new_string(driveInfo->satVendorID)));
    }
    if (safe_strlen(driveInfo->satProductID))
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "SAT Product ID", json_object_new_string(driveInfo->satProductID)));
    }
    if (safe_strlen(driveInfo->satProductRevision))
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "SAT Product Rev", json_object_new_string(driveInfo->satProductRevision)));
    }

    if (driveInfo->worldWideNameSupported)
    {
        DECLARE_ZERO_INIT_ARRAY(char, worldWideName,
                                (MAX_UINT64_TO_HEX_STRING_LENGTH + MAX_UINT64_TO_HEX_STRING_LENGTH));
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(worldWideName,
                                                   (MAX_UINT64_TO_HEX_STRING_LENGTH + MAX_UINT64_TO_HEX_STRING_LENGTH),
                                                   "%016" PRIX64 "", driveInfo->worldWideName),
                               "JSON field buffer is sized for the fixed display format");

        if (driveInfo->worldWideNameExtensionValid)
        {
            M_IGNORE_SAFE_INT_CALL(
                snprintf_err_handle(worldWideName, (MAX_UINT64_TO_HEX_STRING_LENGTH + MAX_UINT64_TO_HEX_STRING_LENGTH),
                                    "%016" PRIX64 "%016" PRIX64, driveInfo->worldWideName,
                                    driveInfo->worldWideNameExtension),
                "JSON field buffer is sized for the fixed display format");
        }
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "World Wide Name", json_object_new_string(worldWideName)));
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "World Wide Name", json_object_new_string("Not Supported")));
    }

    if (driveInfo->dateOfManufactureValid)
    {
        DECLARE_ZERO_INIT_ARRAY(char, dateOfManufacture, MAX_DATE_OF_MANUFACTURE_LENGTH);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(dateOfManufacture, MAX_DATE_OF_MANUFACTURE_LENGTH,
                                                   "Week %" PRIu8 ", %" PRIu16, driveInfo->manufactureWeek,
                                                   driveInfo->manufactureYear),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Date Of Manufacture", json_object_new_string(dateOfManufacture)));
    }

    if (driveInfo->copyrightValid && safe_strlen(driveInfo->copyrightInfo))
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Copyright", json_object_new_string(driveInfo->copyrightInfo)));
    }

    // Drive capacity
    mCapacity = C_CAST(double, driveInfo->maxLBA * driveInfo->logicalSectorSize);
    if (driveInfo->maxLBA == 0 && driveInfo->ataLegacyCHSInfo.legacyCHSValid)
    {
        if (driveInfo->ataLegacyCHSInfo.currentCapacityInSectors > 0)
        {
            mCapacity = C_CAST(double, C_CAST(uint64_t, driveInfo->ataLegacyCHSInfo.currentCapacityInSectors) *
                                           C_CAST(uint64_t, driveInfo->logicalSectorSize));
        }
        else
        {
            mCapacity = C_CAST(double, (C_CAST(uint64_t, driveInfo->ataLegacyCHSInfo.numberOfLogicalCylinders) *
                                        C_CAST(uint64_t, driveInfo->ataLegacyCHSInfo.numberOfLogicalHeads) *
                                        C_CAST(uint64_t, driveInfo->ataLegacyCHSInfo.numberOfLogicalSectorsPerTrack)) *
                                           C_CAST(uint64_t, driveInfo->logicalSectorSize));
        }
    }
    capacity = mCapacity;
    metric_Unit_Convert(&mCapacity, &mCapUnit);
    capacity_Unit_Convert(&capacity, &capUnit);

    DECLARE_ZERO_INIT_ARRAY(char, driveCapacity, MAX_DOUBLE_TO_DEC_STRING_LENGHT + MAX_DOUBLE_TO_DEC_STRING_LENGHT);
    DECLARE_ZERO_INIT_ARRAY(char, driveCapStr, MAX_STRING_DRIVE_CAPACITY_LENGTH);
    M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(driveCapacity,
                                               (MAX_DOUBLE_TO_DEC_STRING_LENGHT + MAX_DOUBLE_TO_DEC_STRING_LENGHT),
                                               "%0.02f/%0.02f", mCapacity, capacity),
                           "JSON field buffer is sized for the fixed display format");
    M_IGNORE_SAFE_INT_CALL(
        snprintf_err_handle(driveCapStr, MAX_STRING_DRIVE_CAPACITY_LENGTH, "Drive Capacity (%s/%s)", mCapUnit, capUnit),
        "JSON field buffer is sized for the fixed display format");
    RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, driveCapStr, json_object_new_string(driveCapacity)));

    if (!(driveInfo->nativeMaxLBA == 0 || driveInfo->nativeMaxLBA == UINT64_MAX))
    {
        mCapacity =
            C_CAST(double, C_CAST(uint64_t, driveInfo->nativeMaxLBA) * C_CAST(uint64_t, driveInfo->logicalSectorSize));
        capacity = mCapacity;
        metric_Unit_Convert(&mCapacity, &mCapUnit);
        capacity_Unit_Convert(&capacity, &capUnit);

        DECLARE_ZERO_INIT_ARRAY(char, nativedriveCapacity,
                                (MAX_DOUBLE_TO_DEC_STRING_LENGHT + MAX_DOUBLE_TO_DEC_STRING_LENGHT));
        DECLARE_ZERO_INIT_ARRAY(char, nativeDriveStr, MAX_STRING_NATIVE_DRIVE_CAPACITY_LENGTH);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(nativedriveCapacity,
                                                   (MAX_DOUBLE_TO_DEC_STRING_LENGHT + MAX_DOUBLE_TO_DEC_STRING_LENGHT),
                                                   "%0.02f/%0.02f", mCapacity, capacity),
                               "JSON field buffer is sized for the fixed display format");
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(nativeDriveStr, MAX_STRING_NATIVE_DRIVE_CAPACITY_LENGTH,
                                                   "Native Drive Capacity (%s/%s)", mCapUnit, capUnit),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, nativeDriveStr, json_object_new_string(nativedriveCapacity)));
    }

    // Temperature Data
    {
        json_object* temperatureDataObj = json_object_new_object();
        if (temperatureDataObj == M_NULLPTR)
            return MEMORY_FAILURE;
        // current temperature
        if (driveInfo->temperatureData.temperatureDataValid)
        {
            DECLARE_ZERO_INIT_ARRAY(char, currTemp, MAX_INT16_TO_DEC_STRING_LENGTH);
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(currTemp, MAX_INT16_TO_DEC_STRING_LENGTH, "%" PRId16,
                                                       driveInfo->temperatureData.currentTemperature),
                                   "JSON field buffer is sized for the fixed display format");
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(temperatureDataObj, "Current Temperature (C)", json_object_new_string(currTemp)));
        }
        else
        {
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(temperatureDataObj, "Current Temperature (C)", json_object_new_string("Not Reported")));
        }
        // hightest temperature
        if (driveInfo->temperatureData.highestValid)
        {
            DECLARE_ZERO_INIT_ARRAY(char, highTemp, MAX_INT16_TO_DEC_STRING_LENGTH);
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(highTemp, MAX_INT16_TO_DEC_STRING_LENGTH, "%" PRId16,
                                                       driveInfo->temperatureData.highestTemperature),
                                   "JSON field buffer is sized for the fixed display format");
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(temperatureDataObj, "Highest Temperature (C)", json_object_new_string(highTemp)));
        }
        else
        {
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(temperatureDataObj, "Highest Temperature (C)", json_object_new_string("Not Reported")));
        }
        // lowest temperature
        if (driveInfo->temperatureData.lowestValid)
        {
            DECLARE_ZERO_INIT_ARRAY(char, lowTemp, MAX_INT16_TO_DEC_STRING_LENGTH);
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(lowTemp, MAX_INT16_TO_DEC_STRING_LENGTH, "%" PRId16,
                                                       driveInfo->temperatureData.lowestTemperature),
                                   "JSON field buffer is sized for the fixed display format");
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(temperatureDataObj, "Lowest Temperature (C)", json_object_new_string(lowTemp)));
        }
        else
        {
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(temperatureDataObj, "Lowest Temperature (C)", json_object_new_string("Not Reported")));
        }
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Temperature Data", temperatureDataObj));
    }

    // Humidity Data
    if (driveInfo->humidityData.humidityDataValid)
    {
        json_object* humidityDataObj = json_object_new_object();
        if (humidityDataObj == M_NULLPTR)
            return MEMORY_FAILURE;
        // Current Humidity
        if (driveInfo->humidityData.humidityDataValid)
        {
            if (driveInfo->humidityData.currentHumidity == UINT8_MAX)
            {
                RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(humidityDataObj, "Current Humidity (%)",
                                                           json_object_new_string("Invalid Reading")));
            }
            else
            {
                DECLARE_ZERO_INIT_ARRAY(char, currHumidity, MAX_UINT8_TO_DEC_STRING_LENGTH);
                M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(currHumidity, MAX_UINT8_TO_DEC_STRING_LENGTH, "%" PRIu8,
                                                           driveInfo->humidityData.currentHumidity),
                                       "JSON field buffer is sized for the fixed display format");
                RETURN_ON_DRIVE_JSON_ERROR(
                    add_JSON_Object(humidityDataObj, "Current Humidity (%)", json_object_new_string(currHumidity)));
            }
        }
        else
        {
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(humidityDataObj, "Current Humidity (%)", json_object_new_string("Not Reported")));
        }
        // Highest Humidity
        if (driveInfo->humidityData.highestValid)
        {
            if (driveInfo->humidityData.highestHumidity == UINT8_MAX)
            {
                RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(humidityDataObj, "Highest Humidity (%)",
                                                           json_object_new_string("Invalid Reading")));
            }
            else
            {
                DECLARE_ZERO_INIT_ARRAY(char, highHumidity, MAX_UINT8_TO_DEC_STRING_LENGTH);
                M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(highHumidity, MAX_UINT8_TO_DEC_STRING_LENGTH, "%" PRIu8,
                                                           driveInfo->humidityData.highestHumidity),
                                       "JSON field buffer is sized for the fixed display format");
                RETURN_ON_DRIVE_JSON_ERROR(
                    add_JSON_Object(humidityDataObj, "Highest Humidity (%)", json_object_new_string(highHumidity)));
            }
        }
        else
        {
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(humidityDataObj, "Highest Humidity (%)", json_object_new_string("Not Reported")));
        }
        // Lowest Humidity
        if (driveInfo->humidityData.lowestValid)
        {
            if (driveInfo->humidityData.lowestHumidity == UINT8_MAX)
            {
                RETURN_ON_DRIVE_JSON_ERROR(
                    add_JSON_Object(humidityDataObj, "Lowest Humidity (%)", json_object_new_string("Invalid Reading")));
            }
            else
            {
                DECLARE_ZERO_INIT_ARRAY(char, lowHumidity, MAX_UINT8_TO_DEC_STRING_LENGTH);
                M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(lowHumidity, MAX_UINT8_TO_DEC_STRING_LENGTH, "%" PRIu8,
                                                           driveInfo->humidityData.lowestHumidity),
                                       "JSON field buffer is sized for the fixed display format");
                RETURN_ON_DRIVE_JSON_ERROR(
                    add_JSON_Object(humidityDataObj, "Lowest Humidity (%)", json_object_new_string(lowHumidity)));
            }
        }
        else
        {
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(humidityDataObj, "Lowest Humidity (%)", json_object_new_string("Not Reported")));
        }

        // Add the humidity data object to the root JSON object
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Humidity Data", humidityDataObj));
    }

    // Power On Time
    if (driveInfo->powerOnMinutesValid)
    {
        uint16_t days    = UINT16_C(0);
        uint8_t  years   = UINT8_C(0);
        uint8_t  hours   = UINT8_C(0);
        uint8_t  minutes = UINT8_C(0);
        uint8_t  seconds = UINT8_C(0);
        convert_Seconds_To_Displayable_Time(driveInfo->powerOnMinutes * UINT64_C(60), &years, &days, &hours, &minutes,
                                            &seconds);
        // Create a string to hold the formatted time
        DECLARE_ZERO_INIT_ARRAY(char, timeString, MAX_TIME_STRING_LENGTH);

        // Append each time component to the string if it's greater than 0
        create_Time_String(timeString, years, days, hours, minutes, seconds);
        // Add the formatted string to the JSON object
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Power On Time", json_object_new_string(timeString)));
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Power On Time", json_object_new_string("Not Reported")));
    }

    if (driveInfo->powerOnMinutesValid)
    {
        // convert to a double to display as xx.xx
        double powerOnHours = C_CAST(double, driveInfo->powerOnMinutes) / 60.00;
        DECLARE_ZERO_INIT_ARRAY(char, poh, MAX_DOUBLE_TO_DEC_STRING_LENGHT);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(poh, MAX_DOUBLE_TO_DEC_STRING_LENGHT, "%0.02f", powerOnHours),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Power On Hours", json_object_new_string(poh)));
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Power On Hours", json_object_new_string("Not Reported")));
    }

    // CHS
    if (driveInfo->ataLegacyCHSInfo.legacyCHSValid && driveInfo->maxLBA == 0)
    {
        // Default CHS
        DECLARE_ZERO_INIT_ARRAY(char, chs, MAX_STRING_DEFAULT_CHS_LENGTH);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(chs, MAX_STRING_DEFAULT_CHS_LENGTH,
                                                   "%" PRIu16 " | %" PRIu8 " | %" PRIu8,
                                                   driveInfo->ataLegacyCHSInfo.numberOfLogicalCylinders,
                                                   driveInfo->ataLegacyCHSInfo.numberOfLogicalHeads,
                                                   driveInfo->ataLegacyCHSInfo.numberOfLogicalSectorsPerTrack),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Default CHS", json_object_new_string(chs)));

        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(chs, MAX_STRING_DEFAULT_CHS_LENGTH,
                                                   "%" PRIu16 " | %" PRIu8 " | %" PRIu8,
                                                   driveInfo->ataLegacyCHSInfo.numberOfCurrentLogicalCylinders,
                                                   driveInfo->ataLegacyCHSInfo.numberOfCurrentLogicalHeads,
                                                   driveInfo->ataLegacyCHSInfo.numberOfCurrentLogicalSectorsPerTrack),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Current CHS", json_object_new_string(chs)));

        uint32_t simMaxLBA = UINT32_C(0);
        if (driveInfo->ataLegacyCHSInfo.currentInfoconfigurationValid &&
            driveInfo->ataLegacyCHSInfo.numberOfCurrentLogicalCylinders > 0 &&
            driveInfo->ataLegacyCHSInfo.numberOfCurrentLogicalHeads > 0 &&
            driveInfo->ataLegacyCHSInfo.numberOfCurrentLogicalSectorsPerTrack > 0)
        {
            simMaxLBA = C_CAST(uint32_t, driveInfo->ataLegacyCHSInfo.numberOfCurrentLogicalCylinders) *
                        C_CAST(uint32_t, driveInfo->ataLegacyCHSInfo.numberOfCurrentLogicalHeads) *
                        C_CAST(uint32_t, driveInfo->ataLegacyCHSInfo.numberOfCurrentLogicalSectorsPerTrack);
        }
        else
        {
            simMaxLBA = C_CAST(uint32_t, driveInfo->ataLegacyCHSInfo.numberOfLogicalCylinders) *
                        C_CAST(uint32_t, driveInfo->ataLegacyCHSInfo.numberOfLogicalHeads) *
                        C_CAST(uint32_t, driveInfo->ataLegacyCHSInfo.numberOfLogicalSectorsPerTrack);
        }
        DECLARE_ZERO_INIT_ARRAY(char, value, MAX_UINT32_TO_DEC_STRING_LENGTH);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(value, MAX_UINT32_TO_DEC_STRING_LENGTH, "%" PRIu32, simMaxLBA),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Simulated MaxLBA", json_object_new_string(value)));
    }
    else
    {
        // MaxLBA
        DECLARE_ZERO_INIT_ARRAY(char, maxLBA, MAX_UINT64_TO_DEC_STRING_LENGTH);
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(maxLBA, MAX_UINT64_TO_DEC_STRING_LENGTH, "%" PRIu64, driveInfo->maxLBA),
            "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "MaxLBA", json_object_new_string(maxLBA)));

        // Native Max LBA
        if (driveInfo->nativeMaxLBA == 0 || driveInfo->nativeMaxLBA == UINT64_MAX)
        {
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(rootObject, "Native MaxLBA", json_object_new_string("Not Reported")));
        }
        else
        {
            DECLARE_ZERO_INIT_ARRAY(char, nativeMaxLBA, MAX_UINT64_TO_DEC_STRING_LENGTH);
            M_IGNORE_SAFE_INT_CALL(
                snprintf_err_handle(nativeMaxLBA, MAX_UINT64_TO_DEC_STRING_LENGTH, "%" PRIu64, driveInfo->nativeMaxLBA),
                "JSON field buffer is sized for the fixed display format");
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(rootObject, "Native MaxLBA", json_object_new_uint64(driveInfo->nativeMaxLBA)));
        }
    }

    if (driveInfo->isFormatCorrupt)
    {
        // Logical Sector Size
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Logical Sector Size (B)", json_object_new_string("Format Corrupt")));
        // Physical Sector Size
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Physical Sector Size (B)", json_object_new_string("Format Corrupt")));
        // Sector Alignment
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Sector Alignment", json_object_new_string("Format Corrupt")));
    }
    else
    {
        DECLARE_ZERO_INIT_ARRAY(char, sectorSize, MAX_UINT32_TO_DEC_STRING_LENGTH);
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(sectorSize, MAX_UINT32_TO_DEC_STRING_LENGTH, "%" PRIu32, driveInfo->logicalSectorSize),
            "JSON field buffer is sized for the fixed display format");
        // Logical Sector Size
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Logical Sector Size (B)", json_object_new_string(sectorSize)));
        // Physical Sector Size
        DECLARE_ZERO_INIT_ARRAY(char, phySectorSize, MAX_UINT32_TO_DEC_STRING_LENGTH);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(phySectorSize, MAX_UINT32_TO_DEC_STRING_LENGTH, "%" PRIu32,
                                                   driveInfo->physicalSectorSize),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Physical Sector Size (B)", json_object_new_string(phySectorSize)));
        // Sector Alignment
        DECLARE_ZERO_INIT_ARRAY(char, sectorAlign, MAX_UINT16_TO_DEC_STRING_LENGTH);
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(sectorAlign, MAX_UINT16_TO_DEC_STRING_LENGTH, "%" PRIu16, driveInfo->sectorAlignment),
            "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Sector Alignment", json_object_new_string(sectorAlign)));
    }
    // Rotation Rate
    if (driveInfo->rotationRate == 0)
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Rotation Rate (RPM)", json_object_new_string("Not Reported")));
    }
    else if (driveInfo->rotationRate == 0x0001)
    {
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Rotation Rate (RPM)", json_object_new_string("SSD")));
    }
    else
    {
        DECLARE_ZERO_INIT_ARRAY(char, rotationRate, MAX_UINT16_TO_DEC_STRING_LENGTH);
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(rotationRate, MAX_UINT16_TO_DEC_STRING_LENGTH, "%" PRIu16, driveInfo->rotationRate),
            "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Rotation Rate (RPM)", json_object_new_string(rotationRate)));
    }

    if (driveInfo->isWriteProtected)
    {
        // print_str("\tMedium is write protected!\n");
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Medium is write protected", json_object_new_string("true")));
    }

    // Form Factor
    switch (driveInfo->formFactor)
    {
    case 1:
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Form Factor", json_object_new_string("5.25\"")));
        break;
    case 2:
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Form Factor", json_object_new_string("3.5\"")));
        break;
    case 3:
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Form Factor", json_object_new_string("2.5\"")));
        break;
    case 4:
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Form Factor", json_object_new_string("1.8\"")));
        break;
    case 5:
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Form Factor", json_object_new_string("Less than 1.8\"")));
        break;
    case 6:
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Form Factor", json_object_new_string("mSATA")));
        break;
    case 7:
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Form Factor", json_object_new_string("M.2")));
        break;
    case 8:
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Form Factor", json_object_new_string("MicroSSD")));
        break;
    case 9:
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Form Factor", json_object_new_string("CFast")));
        break;
    case 0:
    default:
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Form Factor", json_object_new_string("Not Reported")));
        break;
    }

    // Last DST information
    if (driveInfo->dstInfo.informationValid && driveInfo->powerOnMinutesValid)
    {
        if (driveInfo->powerOnMinutes - (driveInfo->dstInfo.powerOnHours * 60) != driveInfo->powerOnMinutes)
        {
            json_object* dstDataObj = json_object_new_object();
            if (dstDataObj == M_NULLPTR)
                return MEMORY_FAILURE;

            double timeSinceLastDST =
                (C_CAST(double, driveInfo->powerOnMinutes) / 60.0) - C_CAST(double, driveInfo->dstInfo.powerOnHours);
            if (timeSinceLastDST >= 0)
            {
                DECLARE_ZERO_INIT_ARRAY(char, timeLastDst, MAX_DOUBLE_TO_DEC_STRING_LENGHT);
                M_IGNORE_SAFE_INT_CALL(
                    snprintf_err_handle(timeLastDst, MAX_DOUBLE_TO_DEC_STRING_LENGHT, "%0.02f", timeSinceLastDST),
                    "JSON field buffer is sized for the fixed display format");
                RETURN_ON_DRIVE_JSON_ERROR(
                    add_JSON_Object(dstDataObj, "Time since last DST (hours)", json_object_new_string(timeLastDst)));
            }
            else
            {
                RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(dstDataObj, "Time since last DST (hours)",
                                                           json_object_new_string("Indeterminate")));
            }

            DECLARE_ZERO_INIT_ARRAY(char, statusResult, MAX_UINT8_TO_HEX_STRING_LENGTH);
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(statusResult, MAX_UINT8_TO_HEX_STRING_LENGTH, "0x%" PRIX8,
                                                       driveInfo->dstInfo.resultOrStatus),
                                   "JSON field buffer is sized for the fixed display format");
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(dstDataObj, "DST Status/Result", json_object_new_string(statusResult)));

            DECLARE_ZERO_INIT_ARRAY(char, testRun, MAX_UINT8_TO_HEX_STRING_LENGTH);
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(testRun, MAX_UINT8_TO_HEX_STRING_LENGTH, "0x%" PRIX8,
                                                       driveInfo->dstInfo.testNumber),
                                   "JSON field buffer is sized for the fixed display format");
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(dstDataObj, "DST Test run", json_object_new_string(testRun)));

            if (driveInfo->dstInfo.resultOrStatus != 0 && driveInfo->dstInfo.resultOrStatus != 0xF &&
                driveInfo->dstInfo.errorLBA != UINT64_MAX)
            {
                // Show the Error LBA
                DECLARE_ZERO_INIT_ARRAY(char, errLBA, MAX_UINT64_TO_HEX_STRING_LENGTH);
                M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(errLBA, MAX_UINT64_TO_HEX_STRING_LENGTH, "0x%" PRIX64,
                                                           driveInfo->dstInfo.errorLBA),
                                       "JSON field buffer is sized for the fixed display format");
                RETURN_ON_DRIVE_JSON_ERROR(
                    add_JSON_Object(dstDataObj, "Error occurred at LBA", json_object_new_string(errLBA)));
            }
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Last DST information", dstDataObj));
        }
        else
        {
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(rootObject, "Last DST information", json_object_new_string("DST has never been run")));
        }
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Last DST information", json_object_new_string("Not Reported")));
    }
    // Long DST time
    if (driveInfo->longDSTTimeMinutes > 0)
    {
        // print as hours:minutes
        uint16_t days    = UINT16_C(0);
        uint8_t  years   = UINT8_C(0);
        uint8_t  hours   = UINT8_C(0);
        uint8_t  minutes = UINT8_C(0);
        uint8_t  seconds = UINT8_C(0);
        convert_Seconds_To_Displayable_Time(driveInfo->longDSTTimeMinutes * 60, &years, &days, &hours, &minutes,
                                            &seconds);
        DECLARE_ZERO_INIT_ARRAY(char, timeString, MAX_TIME_STRING_LENGTH);
        // Append each time component to the string if it's greater than 0
        create_Time_String(timeString, years, days, hours, minutes, seconds);
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Long Drive Self Test Time", json_object_new_string(timeString)));
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Long Drive Self Test Time", json_object_new_string("Not Supported")));
    }

    // Interface Speed
    if (driveInfo->interfaceSpeedInfo.speedIsValid)
    {
        json_object* speedDataObj = json_object_new_object();
        if (speedDataObj == M_NULLPTR)
            return MEMORY_FAILURE;
        if (driveInfo->interfaceSpeedInfo.speedType == INTERFACE_SPEED_SERIAL)
        {
            if (driveInfo->interfaceSpeedInfo.serialSpeed.numberOfPorts > 0)
            {
                if (driveInfo->interfaceSpeedInfo.serialSpeed.numberOfPorts == 1)
                {
                    switch (driveInfo->interfaceSpeedInfo.serialSpeed.portSpeedsMax[0])
                    {
                    case 5:
                        RETURN_ON_DRIVE_JSON_ERROR(
                            add_JSON_Object(speedDataObj, "Max Speed (Gb/s)", json_object_new_string("22.5")));
                        break;
                    case 4:
                        RETURN_ON_DRIVE_JSON_ERROR(
                            add_JSON_Object(speedDataObj, "Max Speed (Gb/s)", json_object_new_string("12.0")));
                        break;
                    case 3:
                        RETURN_ON_DRIVE_JSON_ERROR(
                            add_JSON_Object(speedDataObj, "Max Speed (Gb/s)", json_object_new_string("6.0")));
                        break;
                    case 2:
                        RETURN_ON_DRIVE_JSON_ERROR(
                            add_JSON_Object(speedDataObj, "Max Speed (Gb/s)", json_object_new_string("3.0")));
                        break;
                    case 1:
                        RETURN_ON_DRIVE_JSON_ERROR(
                            add_JSON_Object(speedDataObj, "Max Speed (Gb/s)", json_object_new_string("1.5")));
                        break;
                    case 0:
                        RETURN_ON_DRIVE_JSON_ERROR(
                            add_JSON_Object(speedDataObj, "Max Speed (Gb/s)", json_object_new_string("Not Reported")));
                        break;
                    default:
                        RETURN_ON_DRIVE_JSON_ERROR(
                            add_JSON_Object(speedDataObj, "Max Speed (Gb/s)", json_object_new_string("Unknown")));
                        break;
                    }

                    switch (driveInfo->interfaceSpeedInfo.serialSpeed.portSpeedsNegotiated[0])
                    {
                    case 5:
                        RETURN_ON_DRIVE_JSON_ERROR(
                            add_JSON_Object(speedDataObj, "Negotiated Speed (Gb/s)", json_object_new_string("22.5")));
                        break;
                    case 4:
                        RETURN_ON_DRIVE_JSON_ERROR(
                            add_JSON_Object(speedDataObj, "Negotiated Speed (Gb/s)", json_object_new_string("12.0")));
                        break;
                    case 3:
                        RETURN_ON_DRIVE_JSON_ERROR(
                            add_JSON_Object(speedDataObj, "Negotiated Speed (Gb/s)", json_object_new_string("6.0")));
                        break;
                    case 2:
                        RETURN_ON_DRIVE_JSON_ERROR(
                            add_JSON_Object(speedDataObj, "Negotiated Speed (Gb/s)", json_object_new_string("3.0")));
                        break;
                    case 1:
                        RETURN_ON_DRIVE_JSON_ERROR(
                            add_JSON_Object(speedDataObj, "Negotiated Speed (Gb/s)", json_object_new_string("1.5")));
                        break;
                    case 0:
                        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(speedDataObj, "Negotiated Speed (Gb/s)",
                                                                   json_object_new_string("Not Reported")));
                        break;
                    default:
                        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(speedDataObj, "Negotiated Speed (Gb/s)",
                                                                   json_object_new_string("Unknown")));
                        break;
                    }
                }
                else
                {
                    // Add current port number
                    if (driveInfo->interfaceSpeedInfo.serialSpeed.activePortNumber != UINT8_MAX)
                    {
                        DECLARE_ZERO_INIT_ARRAY(char, portString, MAX_STRING_PORT_NAME_LENGTH);
                        M_IGNORE_SAFE_INT_CALL(
                            snprintf_err_handle(portString, MAX_STRING_PORT_NAME_LENGTH, "Port %" PRIu8 "",
                                                driveInfo->interfaceSpeedInfo.serialSpeed.activePortNumber),
                            "JSON field buffer is sized for the fixed display format");
                        RETURN_ON_DRIVE_JSON_ERROR(
                            add_JSON_Object(speedDataObj, "Current Port", json_object_new_string(portString)));
                    }
                    else
                    {
                        // TODO - check if this will ever be true
                        RETURN_ON_DRIVE_JSON_ERROR(
                            add_JSON_Object(speedDataObj, "Current Port", json_object_new_string("Not Reported")));
                    }

                    // now add individaul port detail in the list
                    json_object* portArray = json_object_new_array();
                    if (portArray == M_NULLPTR)
                    {
                        json_object_put(speedDataObj);
                        return MEMORY_FAILURE;
                    }

                    for (uint8_t portIter = UINT8_C(0);
                         portIter < driveInfo->interfaceSpeedInfo.serialSpeed.numberOfPorts && portIter < MAX_PORTS;
                         portIter++)
                    {
                        json_object* portObj = json_object_new_object();
                        if (portObj == M_NULLPTR)
                        {
                            json_object_put(portArray);
                            json_object_put(speedDataObj);
                            return MEMORY_FAILURE;
                        }

                        // Max Speed
                        switch (driveInfo->interfaceSpeedInfo.serialSpeed.portSpeedsMax[portIter])
                        {
                        case 5:
                            RETURN_ON_DRIVE_JSON_ERROR(
                                add_JSON_Object(portObj, "Max Speed (GB/s)", json_object_new_string("22.5")));
                            break;
                        case 4:
                            RETURN_ON_DRIVE_JSON_ERROR(
                                add_JSON_Object(portObj, "Max Speed (GB/s)", json_object_new_string("12.0")));
                            break;
                        case 3:
                            RETURN_ON_DRIVE_JSON_ERROR(
                                add_JSON_Object(portObj, "Max Speed (GB/s)", json_object_new_string("6.0")));
                            break;
                        case 2:
                            RETURN_ON_DRIVE_JSON_ERROR(
                                add_JSON_Object(portObj, "Max Speed (GB/s)", json_object_new_string("3.0")));
                            break;
                        case 1:
                            RETURN_ON_DRIVE_JSON_ERROR(
                                add_JSON_Object(portObj, "Max Speed (GB/s)", json_object_new_string("1.5")));
                            break;
                        case 0:
                            RETURN_ON_DRIVE_JSON_ERROR(
                                add_JSON_Object(portObj, "Max Speed (GB/s)", json_object_new_string("Not Reported")));
                            break;
                        default:
                            RETURN_ON_DRIVE_JSON_ERROR(
                                add_JSON_Object(portObj, "Max Speed (GB/s)", json_object_new_string("Unknown")));
                            break;
                        }

                        // Negotiated speed
                        switch (driveInfo->interfaceSpeedInfo.serialSpeed.portSpeedsNegotiated[portIter])
                        {
                        case 5:
                            RETURN_ON_DRIVE_JSON_ERROR(
                                add_JSON_Object(portObj, "Negotiated Speed (Gb/s)", json_object_new_string("22.5")));
                            break;
                        case 4:
                            RETURN_ON_DRIVE_JSON_ERROR(
                                add_JSON_Object(portObj, "Negotiated Speed (Gb/s)", json_object_new_string("12.0")));
                            break;
                        case 3:
                            RETURN_ON_DRIVE_JSON_ERROR(
                                add_JSON_Object(portObj, "Negotiated Speed (Gb/s)", json_object_new_string("6.0")));
                            break;
                        case 2:
                            RETURN_ON_DRIVE_JSON_ERROR(
                                add_JSON_Object(portObj, "Negotiated Speed (Gb/s)", json_object_new_string("3.0")));
                            break;
                        case 1:
                            RETURN_ON_DRIVE_JSON_ERROR(
                                add_JSON_Object(portObj, "Negotiated Speed (Gb/s)", json_object_new_string("1.5")));
                            break;
                        case 0:
                            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(portObj, "Negotiated Speed (Gb/s)",
                                                                       json_object_new_string("Not Reported")));
                            break;
                        default:
                            RETURN_ON_DRIVE_JSON_ERROR(
                                add_JSON_Object(portObj, "Negotiated Speed (Gb/s)", json_object_new_string("Unknown")));
                        }

                        // create new node, name if Analyzed Field ? and then add name-value pair in this node
                        json_object* node = json_object_new_object();
                        if (node == M_NULLPTR)
                        {
                            json_object_put(portObj);
                            json_object_put(portArray);
                            json_object_put(speedDataObj);
                            return MEMORY_FAILURE;
                        }
                        DECLARE_ZERO_INIT_ARRAY(char, portString, MAX_STRING_PORT_NAME_LENGTH);
                        M_IGNORE_SAFE_INT_CALL(
                            snprintf_err_handle(portString, MAX_STRING_PORT_NAME_LENGTH, "Port %" PRIu8, portIter),
                            "JSON field buffer is sized for the fixed display format");
                        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(node, portString, portObj));

                        // add this in array
                        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Array_Element(portArray, node));

                        // json_object_object_add(speedDataObj, portString, portObj);
                    }

                    RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(speedDataObj, "Ports", portArray));
                }

                RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Interface speed", speedDataObj));
            }
            else
            {
                RETURN_ON_DRIVE_JSON_ERROR(
                    add_JSON_Object(rootObject, "Interface speed", json_object_new_string("Not Reported")));
            }
        }
        else if (driveInfo->interfaceSpeedInfo.speedType == INTERFACE_SPEED_PARALLEL)
        {
            DECLARE_ZERO_INIT_ARRAY(char, temp, MAX_STRING_INTERFACE_SPEED_LENGTH);
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(temp, MAX_STRING_INTERFACE_SPEED_LENGTH, "%0.02f",
                                                       driveInfo->interfaceSpeedInfo.parallelSpeed.maxSpeed),
                                   "JSON field buffer is sized for the fixed display format");
            if (driveInfo->interfaceSpeedInfo.parallelSpeed.maxModeNameValid)
            {
                M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(temp, MAX_STRING_INTERFACE_SPEED_LENGTH, "%0.02f (%s)",
                                                           driveInfo->interfaceSpeedInfo.parallelSpeed.maxSpeed,
                                                           driveInfo->interfaceSpeedInfo.parallelSpeed.maxModeName),
                                       "JSON field buffer is sized for the fixed display format");
            }
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(speedDataObj, "Max Speed (MB/s)", json_object_new_string(temp)));

            if (driveInfo->interfaceSpeedInfo.parallelSpeed.negotiatedValid)
            {
                M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(temp, sizeof(temp), "%0.02f",
                                                           driveInfo->interfaceSpeedInfo.parallelSpeed.negotiatedSpeed),
                                       "JSON field buffer is sized for the fixed display format");
                if (driveInfo->interfaceSpeedInfo.parallelSpeed.negModeNameValid)
                {
                    M_IGNORE_SAFE_INT_CALL(
                        snprintf_err_handle(temp, sizeof(temp), "%0.02f (%s)",
                                            driveInfo->interfaceSpeedInfo.parallelSpeed.negotiatedSpeed,
                                            driveInfo->interfaceSpeedInfo.parallelSpeed.negModeName),
                        "JSON field buffer is sized for the fixed display format");
                }
                RETURN_ON_DRIVE_JSON_ERROR(
                    add_JSON_Object(speedDataObj, "Negotiated Speed (MB/s)", json_object_new_string(temp)));
            }
            else
            {
                RETURN_ON_DRIVE_JSON_ERROR(
                    add_JSON_Object(speedDataObj, "Negotiated Speed (MB/s)", json_object_new_string("Not Reported")));
            }
            if (driveInfo->interfaceSpeedInfo.parallelSpeed.cableInfoType == CABLING_INFO_ATA &&
                driveInfo->interfaceSpeedInfo.parallelSpeed.ataCableInfo.cablingInfoValid)
            {
                if (driveInfo->interfaceSpeedInfo.parallelSpeed.ataCableInfo.ata80PinCableDetected)
                {
                    RETURN_ON_DRIVE_JSON_ERROR(
                        add_JSON_Object(speedDataObj, "Cabling Detected", json_object_new_string("80-pin Cable")));
                }
                else
                {
                    RETURN_ON_DRIVE_JSON_ERROR(
                        add_JSON_Object(speedDataObj, "Cabling Detected", json_object_new_string("40-pin Cable")));
                }

                RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(
                    speedDataObj, "Device Number",
                    json_object_new_int(
                        driveInfo->interfaceSpeedInfo.parallelSpeed.ataCableInfo.device1 ? UINT8_C(1) : UINT8_C(0))));

                switch (driveInfo->interfaceSpeedInfo.parallelSpeed.ataCableInfo.deviceNumberDetermined)
                {
                case 1:
                    RETURN_ON_DRIVE_JSON_ERROR(
                        add_JSON_Object(speedDataObj, "Device Set by", json_object_new_string("Jumper")));
                    break;
                case 2:
                    RETURN_ON_DRIVE_JSON_ERROR(
                        add_JSON_Object(speedDataObj, "Device Set by", json_object_new_string("Cable Select")));
                    break;
                default:
                    RETURN_ON_DRIVE_JSON_ERROR(
                        add_JSON_Object(speedDataObj, "Device Set by", json_object_new_string("Unknown")));
                    break;
                }
            }

            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Interface speed", speedDataObj));
        }
        else if (driveInfo->interfaceSpeedInfo.speedType == INTERFACE_SPEED_ANCIENT)
        {
            if (driveInfo->interfaceSpeedInfo.ancientHistorySpeed.dataTransferGt10MbS)
            {
                RETURN_ON_DRIVE_JSON_ERROR(
                    add_JSON_Object(rootObject, "Interface speed", json_object_new_string(">10Mb/s")));
            }
            else if (driveInfo->interfaceSpeedInfo.ancientHistorySpeed.dataTransferGt5MbSLte10MbS)
            {
                RETURN_ON_DRIVE_JSON_ERROR(
                    add_JSON_Object(rootObject, "Interface speed", json_object_new_string(">5Mb/s & <10Mb/s")));
            }
            else if (driveInfo->interfaceSpeedInfo.ancientHistorySpeed.dataTransferLte5MbS)
            {
                RETURN_ON_DRIVE_JSON_ERROR(
                    add_JSON_Object(rootObject, "Interface speed", json_object_new_string("<5Mb/s")));
            }
            else
            {
                RETURN_ON_DRIVE_JSON_ERROR(
                    add_JSON_Object(rootObject, "Interface speed", json_object_new_string("Not Reported")));
            }
        }
        else
        {
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(rootObject, "Interface speed", json_object_new_string("Not Reported")));
        }
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Interface speed", json_object_new_string("Not Reported")));
    }

    // Workload Rate (Annualized)
    if (driveInfo->totalBytesRead > 0 || driveInfo->totalBytesWritten > 0)
    {
        if (driveInfo->powerOnMinutesValid)
        {
#ifndef MINUTES_IN_1_YEAR
#    define MINUTES_IN_1_YEAR 525600.0
#endif // !MINUTES_IN_1_YEAR
            double totalTerabytesRead    = C_CAST(double, driveInfo->totalBytesRead) / 1000000000000.0;
            double totalTerabytesWritten = C_CAST(double, driveInfo->totalBytesWritten) / 1000000000000.0;
            double calculatedUsage       = C_CAST(double, totalTerabytesRead + totalTerabytesWritten) *
                                     C_CAST(double, MINUTES_IN_1_YEAR / C_CAST(double, driveInfo->powerOnMinutes));
            DECLARE_ZERO_INIT_ARRAY(char, usage, MAX_DOUBLE_TO_DEC_STRING_LENGHT);
            M_IGNORE_SAFE_INT_CALL(
                snprintf_err_handle(usage, MAX_DOUBLE_TO_DEC_STRING_LENGHT, "%0.02f", calculatedUsage),
                "JSON field buffer is sized for the fixed display format");
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(rootObject, "Annualized Workload Rate (TB/yr)", json_object_new_string(usage)));
        }
        else
        {
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(rootObject, "Annualized Workload Rate (TB/yr)", json_object_new_string("0.00")));
        }
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Annualized Workload Rate (TB/yr)", json_object_new_string("Not Reported")));
    }

    // Total Bytes Read
    if (driveInfo->totalBytesRead > 0)
    {
        double totalBytesRead = C_CAST(double, driveInfo->totalBytesRead);
        char   unitString[4]  = {'\0'};
        char*  unit           = &unitString[0];
        metric_Unit_Convert(&totalBytesRead, &unit);
        DECLARE_ZERO_INIT_ARRAY(char, bytesRead, MAX_DOUBLE_TO_DEC_STRING_LENGHT);
        DECLARE_ZERO_INIT_ARRAY(char, bytesReadStr, MAX_STRING_BYTES_READ_LENGTH);
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(bytesReadStr, MAX_STRING_BYTES_READ_LENGTH, "Total Bytes Read (%s)", unit),
            "JSON field buffer is sized for the fixed display format");
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(bytesRead, MAX_DOUBLE_TO_DEC_STRING_LENGHT, "%0.02f", totalBytesRead),
            "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, bytesReadStr, json_object_new_string(bytesRead)));
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Total Bytes Read (B)", json_object_new_string("Not Reported")));
    }

    // Total Bytes Written
    if (driveInfo->totalBytesWritten > 0)
    {
        double totalBytesWritten = C_CAST(double, driveInfo->totalBytesWritten);
        char   unitString[4]     = {'\0'};
        char*  unit              = &unitString[0];
        metric_Unit_Convert(&totalBytesWritten, &unit);
        DECLARE_ZERO_INIT_ARRAY(char, bytesWritten, MAX_DOUBLE_TO_DEC_STRING_LENGHT);
        DECLARE_ZERO_INIT_ARRAY(char, bytesWrittenStr, MAX_STRING_BYTES_WRITTEN_LENGTH);
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(bytesWrittenStr, MAX_STRING_BYTES_WRITTEN_LENGTH, "Total Bytes Written (%s)", unit),
            "JSON field buffer is sized for the fixed display format");
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(bytesWritten, MAX_DOUBLE_TO_DEC_STRING_LENGHT, "%0.02f", totalBytesWritten),
            "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, bytesWrittenStr, json_object_new_string(bytesWritten)));
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Total Bytes Written (B)", json_object_new_string("Not Reported")));
    }

    // Drive reported Utilization
    if (driveInfo->deviceReportedUtilizationRate > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, utilization, MAX_DOUBLE_TO_DEC_STRING_LENGHT);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(utilization, MAX_DOUBLE_TO_DEC_STRING_LENGHT, "%0.04f",
                                                   driveInfo->deviceReportedUtilizationRate),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Drive Reported Utilization (%)", json_object_new_string(utilization)));
    }

    // Encryption Support
    switch (driveInfo->encryptionSupport)
    {
    case ENCRYPTION_SELF_ENCRYPTING:
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Encryption Support", json_object_new_string("Self Encrypting")));
        break;
    case ENCRYPTION_FULL_DISK:
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Encryption Support", json_object_new_string("Full Disk Encryption")));
        break;
    case ENCRYPTION_NONE:
    default:
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Encryption Support", json_object_new_string("Not Supported")));
        break;
    }

    // Cache Size -- convert to MB
    if (driveInfo->cacheSize > 0)
    {
        double cacheSize = C_CAST(double, driveInfo->cacheSize);
        DECLARE_ZERO_INIT_ARRAY(char, cacheUnit, UNIT_STRING_LENGTH);
        char* cachUnitPtr = &cacheUnit[0];
        capacity_Unit_Convert(&cacheSize, &cachUnitPtr);
        DECLARE_ZERO_INIT_ARRAY(char, cacheSizeVal, MAX_DOUBLE_TO_DEC_STRING_LENGHT);
        DECLARE_ZERO_INIT_ARRAY(char, cacheSizeStr, MAX_STRING_CACHE_SIZE_LENGTH);
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(cacheSizeStr, MAX_STRING_CACHE_SIZE_LENGTH, "Cache Size (%s)", cacheUnit),
            "JSON field buffer is sized for the fixed display format");
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(cacheSizeVal, MAX_DOUBLE_TO_DEC_STRING_LENGHT, "%0.02f", cacheSize),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, cacheSizeStr, json_object_new_string(cacheSizeVal)));
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Cache Size (MiB)", json_object_new_string("Not Reported")));
    }

    // Hybrid NAND Cache Size -- convert to GB
    if (driveInfo->hybridNANDSize > 0)
    {
        double cacheSize = C_CAST(double, driveInfo->hybridNANDSize);
        DECLARE_ZERO_INIT_ARRAY(char, cacheUnit, UNIT_STRING_LENGTH);
        char* cachUnitPtr = &cacheUnit[0];
        capacity_Unit_Convert(&cacheSize, &cachUnitPtr);
        DECLARE_ZERO_INIT_ARRAY(char, cacheSizeVal, MAX_DOUBLE_TO_DEC_STRING_LENGHT);
        DECLARE_ZERO_INIT_ARRAY(char, cacheSizeStr, MAX_STRING_HYBRID_NAND_CACHE_SIZE_LENGTH);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(cacheSizeStr, MAX_STRING_HYBRID_NAND_CACHE_SIZE_LENGTH,
                                                   "Hybrid NAND Cache Size (%s)", cacheUnit),
                               "JSON field buffer is sized for the fixed display format");
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(cacheSizeVal, MAX_DOUBLE_TO_DEC_STRING_LENGHT, "%0.02f", cacheSize),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, cacheSizeStr, json_object_new_string(cacheSizeVal)));
    }

    // Percent Endurance Used
    if (driveInfo->rotationRate == 0x0001)
    {
        if (driveInfo->percentEnduranceUsed >= 0)
        {
            DECLARE_ZERO_INIT_ARRAY(char, enduranceUsed, MAX_DOUBLE_TO_DEC_STRING_LENGHT);
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(enduranceUsed, MAX_DOUBLE_TO_DEC_STRING_LENGHT, "%0.05f",
                                                       driveInfo->percentEnduranceUsed),
                                   "JSON field buffer is sized for the fixed display format");
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Percentage Used Endurance Indicator (%)",
                                                       json_object_new_string(enduranceUsed)));
        }
        else
        {
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Percentage Used Endurance Indicator (%)",
                                                       json_object_new_string("Not Reported")));
        }
    }

    // Write Amplification
    if (driveInfo->rotationRate == 0x0001 && driveInfo->totalWritesToFlash > 0)
    {
        if (driveInfo->totalLBAsWritten > 0)
        {
            DECLARE_ZERO_INIT_ARRAY(char, writeAmplification, MAX_DOUBLE_TO_DEC_STRING_LENGHT);
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(writeAmplification, MAX_DOUBLE_TO_DEC_STRING_LENGHT, "%0.02f",
                                                       C_CAST(double, driveInfo->totalWritesToFlash) /
                                                           C_CAST(double, driveInfo->totalLBAsWritten)),
                                   "JSON field buffer is sized for the fixed display format");
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(rootObject, "Write Amplification (%)", json_object_new_string(writeAmplification)));
        }
        else
        {
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(rootObject, "Write Amplification (%)", json_object_new_string("0")));
        }
    }

    // Read look ahead
    if (driveInfo->readLookAheadSupported)
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Read Look-Ahead",
                            json_object_new_string(driveInfo->readLookAheadEnabled ? "Enabled" : "Disabled")));
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Read Look-Ahead", json_object_new_string("Not Supported")));
    }

    // NVCache (!NV_DIS bit from caching MP)
    if (driveInfo->nvCacheSupported)
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Non-Volatile Cache",
                            json_object_new_string(driveInfo->nvCacheEnabled ? "Enabled" : "Disabled")));
    }

    // Write Cache
    if (driveInfo->writeCacheSupported)
    {
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(
            rootObject, "Write Cache", json_object_new_string(driveInfo->writeCacheEnabled ? "Enabled" : "Disabled")));
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Write Cache", json_object_new_string("Not Supported")));
    }

    if (driveInfo->lowCurrentSpinupValid)
    {
        DECLARE_ZERO_INIT_ARRAY(char, lowCurrentSpinupValue, MAX_STRING_LOW_CURRENT_SPINUP_LENGTH);
        if (driveInfo->lowCurrentSpinupViaSCT) // to handle differences in reporting between 2.5" products and others
        {
            switch (driveInfo->lowCurrentSpinupEnabled)
            {
            case SEAGATE_LOW_CURRENT_SPINUP_STATE_LOW:
                if (0 != safe_strcpy(lowCurrentSpinupValue, MAX_STRING_LOW_CURRENT_SPINUP_LENGTH, "Enabled"))
                    M_UNLIKELY
                    {
                        perror("Error copying low current spinup value for JSON output (likely truncation)");
                    }
                break;
            case SEAGATE_LOW_CURRENT_SPINUP_STATE_DEFAULT:
                if (0 != safe_strcpy(lowCurrentSpinupValue, MAX_STRING_LOW_CURRENT_SPINUP_LENGTH, "Disabled"))
                    M_UNLIKELY
                    {
                        perror("Error copying low current spinup value for JSON output (likely truncation)");
                    }
                break;
            case SEAGATE_LOW_CURRENT_SPINUP_STATE_ULTRA_LOW:
                if (0 != safe_strcpy(lowCurrentSpinupValue, MAX_STRING_LOW_CURRENT_SPINUP_LENGTH, "Ultra Low Enabled"))
                    M_UNLIKELY
                    {
                        perror("Error copying low current spinup value for JSON output (likely truncation)");
                    }
                break;
            default:
                M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(lowCurrentSpinupValue, MAX_STRING_LOW_CURRENT_SPINUP_LENGTH,
                                                           "Unknown/Invalid state: %" PRIX16,
                                                           C_CAST(uint16_t, driveInfo->lowCurrentSpinupEnabled)),
                                       "maximum 16-bit state text fits in MAX_STRING_LOW_CURRENT_SPINUP_LENGTH");
                break;
            }
        }
        else
        {
            if (0 != safe_strcpy(lowCurrentSpinupValue, MAX_STRING_LOW_CURRENT_SPINUP_LENGTH,
                                 driveInfo->lowCurrentSpinupEnabled > 0 ? "Enabled" : "Disabled"))
                M_UNLIKELY
                {
                    perror("Error copying low current spinup value for JSON output (likely truncation)");
                }
        }

        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Low Current Spinup", json_object_new_string(lowCurrentSpinupValue)));
    }

    // SMART Status
    switch (driveInfo->smartStatus)
    {
    case 0: // good
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "SMART Status", json_object_new_string("Good")));
        break;
    case 1: // bad
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "SMART Status", json_object_new_string("Tripped")));
        break;
    default: // unknown
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "SMART Status", json_object_new_string("Unknown or Not Supported")));
        break;
    }

    // ATA Security Infomation
    if (driveInfo->ataSecurityInformation.securitySupported)
    {
        json_object* securityInfo = json_object_new_array();
        if (securityInfo == M_NULLPTR)
            return MEMORY_FAILURE;
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Array_Element(securityInfo, json_object_new_string("Supported")));
        if (driveInfo->ataSecurityInformation.securityEnabled)
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Array_Element(securityInfo, json_object_new_string("Enabled")));
        if (driveInfo->ataSecurityInformation.securityLocked)
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Array_Element(securityInfo, json_object_new_string("Locked")));
        if (driveInfo->ataSecurityInformation.securityFrozen)
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Array_Element(securityInfo, json_object_new_string("Frozen")));
        if (driveInfo->ataSecurityInformation.securityCountExpired)
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Array_Element(securityInfo, json_object_new_string("Password Count Expired")));
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "ATA Security Information", securityInfo));
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "ATA Security Information", json_object_new_string("Not Supported")));
    }

    // Zoned Device Type
    if (driveInfo->zonedDevice > 0)
    {
        switch (driveInfo->zonedDevice)
        {
        case 0x1: // host aware
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(rootObject, "Zoned Device Type", json_object_new_string("Host Aware")));
            break;
        case 0x2: // host managed
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(rootObject, "Zoned Device Type", json_object_new_string("Device Managed")));
            break;
        case 0x3: // reserved
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(rootObject, "Zoned Device Type", json_object_new_string("Reserved")));
            break;
        default:
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(rootObject, "Zoned Device Type", json_object_new_string("Not a Zoned Device")));
            break;
        }
    }

    if (driveInfo->fwdlSupport.downloadSupported)
    {
        json_object* fwSupport = json_object_new_array();
        if (fwSupport == M_NULLPTR)
            return MEMORY_FAILURE;
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Array_Element(fwSupport, json_object_new_string("Full")));
        if (driveInfo->fwdlSupport.segmentedSupported)
        {
            if (driveInfo->fwdlSupport.seagateDeferredPowerCycleRequired)
                RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Array_Element(
                    fwSupport, json_object_new_string("Segmented as Deferred - Power Cycle Activation Only")));
            else
                RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Array_Element(fwSupport, json_object_new_string("Segmented")));
        }
        if (driveInfo->fwdlSupport.deferredSupported)
        {
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Array_Element(fwSupport, json_object_new_string("Deferred")));
        }
        if (driveInfo->fwdlSupport.dmaModeSupported)
        {
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Array_Element(fwSupport, json_object_new_string("DMA")));
        }
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Firmware Download Support", fwSupport));
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Firmware Download Support", json_object_new_string("Not Supported")));
    }

    if (driveInfo->lunCount > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, luncount, MAX_UINT8_TO_DEC_STRING_LENGTH);
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(luncount, MAX_UINT8_TO_DEC_STRING_LENGTH, "%" PRIu8, driveInfo->lunCount),
            "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Number of Logical Units", json_object_new_string(luncount)));
    }

    if (driveInfo->concurrentPositioningRanges > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, pos, MAX_UINT8_TO_DEC_STRING_LENGTH);
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(pos, MAX_UINT8_TO_DEC_STRING_LENGTH, "%" PRIu8, driveInfo->concurrentPositioningRanges),
            "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Number of Concurrent Ranges", json_object_new_string(pos)));
    }

    // Specifications Supported
    if (driveInfo->numberOfSpecificationsSupported > 0)
    {
        json_object* specificationSupport = json_object_new_array();
        if (specificationSupport == M_NULLPTR)
            return MEMORY_FAILURE;
        for (uint8_t specificationsIter = UINT8_C(0);
             specificationsIter < driveInfo->numberOfSpecificationsSupported && specificationsIter < MAX_SPECS;
             specificationsIter++)
        {
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Array_Element(
                specificationSupport, json_object_new_string(driveInfo->specificationsSupported[specificationsIter])));
        }
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Specifications Supported", specificationSupport));
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "Specifications Supported", json_object_new_string("None reported by device")));
    }

    // Features Supported
    if (driveInfo->numberOfFeaturesSupported > 0)
    {
        json_object* featuresSupport = json_object_new_array();
        if (featuresSupport == M_NULLPTR)
            return MEMORY_FAILURE;
        for (uint8_t featuresIter = UINT8_C(0);
             featuresIter < driveInfo->numberOfFeaturesSupported && featuresIter < MAX_FEATURES; featuresIter++)
        {
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Array_Element(
                featuresSupport, json_object_new_string(driveInfo->featuresSupported[featuresIter])));
        }
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Features Supported", featuresSupport));
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(
            rootObject, "Features Supported",
            json_object_new_string("None reported or an error occurred while trying to determine\n\t\tthe features.")));
    }

    // Adapter information
    {
        json_object* adapterInfo = json_object_new_object();
        if (adapterInfo == M_NULLPTR)
            return MEMORY_FAILURE;
        switch (driveInfo->adapterInformation.infoType)
        {
        case ADAPTER_INFO_USB:
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(adapterInfo, "Adapter Type", json_object_new_string("USB")));
            break;
        case ADAPTER_INFO_PCI:
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(adapterInfo, "Adapter Type", json_object_new_string("PCI")));
            break;
        case ADAPTER_INFO_IEEE1394:
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(adapterInfo, "Adapter Type", json_object_new_string("IEEE1394")));
            break;
        case ADAPTER_INFO_UNKNOWN:
        default:
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(adapterInfo, "Adapter Type", json_object_new_string("Unknown")));
            break;
        }
        if (driveInfo->adapterInformation.vendorIDValid)
        {
            DECLARE_ZERO_INIT_ARRAY(char, vendorID, MAX_UINT32_TO_HEX_STRING_LENGTH);
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(vendorID, MAX_UINT32_TO_HEX_STRING_LENGTH, "%04" PRIX32 "h",
                                                       driveInfo->adapterInformation.vendorID),
                                   "JSON field buffer is sized for the fixed display format");
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(adapterInfo, "Vendor ID", json_object_new_string(vendorID)));
        }
        else
        {
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(adapterInfo, "Vendor ID", json_object_new_string("Not available")));
        }
        if (driveInfo->adapterInformation.productIDValid)
        {
            DECLARE_ZERO_INIT_ARRAY(char, productID, MAX_UINT32_TO_HEX_STRING_LENGTH);
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(productID, MAX_UINT32_TO_HEX_STRING_LENGTH, "%04" PRIX32 "h",
                                                       driveInfo->adapterInformation.productID),
                                   "JSON field buffer is sized for the fixed display format");
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(adapterInfo, "Product ID", json_object_new_string(productID)));
        }
        else
        {
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(adapterInfo, "Product ID", json_object_new_string("Not available")));
        }
        if (driveInfo->adapterInformation.revisionValid)
        {
            DECLARE_ZERO_INIT_ARRAY(char, revision, MAX_UINT32_TO_HEX_STRING_LENGTH);
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(revision, MAX_UINT32_TO_HEX_STRING_LENGTH, "%04" PRIX32 "h",
                                                       driveInfo->adapterInformation.revision),
                                   "JSON field buffer is sized for the fixed display format");
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(adapterInfo, "Revision", json_object_new_string(revision)));
        }
        else
        {
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(adapterInfo, "Revision", json_object_new_string("Not available")));
        }
        if (driveInfo->adapterInformation
                .specifierIDValid) // IEEE1394 only, so it will only print when we get this set to true for now - TJE
        {
            DECLARE_ZERO_INIT_ARRAY(char, specifierID, MAX_UINT32_TO_HEX_STRING_LENGTH);
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(specifierID, MAX_UINT32_TO_HEX_STRING_LENGTH, "%04" PRIX32 "h",
                                                       driveInfo->adapterInformation.specifierID),
                                   "JSON field buffer is sized for the fixed display format");
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(adapterInfo, "Specifier ID", json_object_new_string(specifierID)));
        }
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Adapter Information", adapterInfo));
    }

    if ((driveInfo->trustedCommandsBeingBlocked) || (driveInfo->lunCount > 1))
    {
        json_object* additionalInfo = json_object_new_array();
        if (additionalInfo == M_NULLPTR)
            return MEMORY_FAILURE;
        if (driveInfo->trustedCommandsBeingBlocked)
        {
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Array_Element(
                additionalInfo,
                json_object_new_string("WARNING: OS/driver/HBA is blocking TCG commands over "
                                       "passthrough. Please enable it before running any TCG commands")));
        }
        if (driveInfo->lunCount > 1)
        {
            // printf("This device has multiple actuators. Some commands/features may affect more than one
            // actuator.\n");
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Array_Element(
                additionalInfo,
                json_object_new_string(
                    "This device has multiple actuators. Some commands/features may affect more than one actuator.")));
        }
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Additional Info", additionalInfo));
    }

    return SUCCESS;
}

M_NODISCARD static eReturnValues create_JSON_Node_For_NVMe_Device_Information(json_object*            rootObject,
                                                                              ptrDriveInformationNVMe driveInfo)
{
    json_object* nvmeInfoObject = json_object_new_object();
    if (nvmeInfoObject == M_NULLPTR)
        return MEMORY_FAILURE;
    RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "NVMe Controller Information", nvmeInfoObject));

    RETURN_ON_DRIVE_JSON_ERROR(
        add_JSON_Object(nvmeInfoObject, "Model Number", json_object_new_string(driveInfo->controllerData.modelNumber)));
    RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(nvmeInfoObject, "Serial Number",
                                               json_object_new_string(driveInfo->controllerData.serialNumber)));
    RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(nvmeInfoObject, "Firmware Revision",
                                               json_object_new_string(driveInfo->controllerData.firmwareRevision)));

    if (driveInfo->controllerData.ieeeOUI > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, ieeeOUI, MAX_UINT32_TO_HEX_STRING_LENGTH);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(ieeeOUI, MAX_UINT32_TO_HEX_STRING_LENGTH, "%06" PRIX32,
                                                   driveInfo->controllerData.ieeeOUI),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(nvmeInfoObject, "IEEE OUI", json_object_new_string(ieeeOUI)));
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(nvmeInfoObject, "IEEE OUI", json_object_new_string("Not Supported")));
    }

    DECLARE_ZERO_INIT_ARRAY(char, pciVendorID, MAX_UINT16_TO_HEX_STRING_LENGTH);
    M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(pciVendorID, MAX_UINT16_TO_HEX_STRING_LENGTH, "%04" PRIX16,
                                               driveInfo->controllerData.pciVendorID),
                           "JSON field buffer is sized for the fixed display format");
    RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(nvmeInfoObject, "PCI Vendor ID", json_object_new_string(pciVendorID)));

    DECLARE_ZERO_INIT_ARRAY(char, pciSubVendorID, MAX_UINT16_TO_HEX_STRING_LENGTH);
    M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(pciSubVendorID, MAX_UINT16_TO_HEX_STRING_LENGTH, "%04" PRIX16,
                                               driveInfo->controllerData.pciSubsystemVendorID),
                           "JSON field buffer is sized for the fixed display format");
    RETURN_ON_DRIVE_JSON_ERROR(
        add_JSON_Object(nvmeInfoObject, "PCI Subsystem Vendor ID", json_object_new_string(pciSubVendorID)));

    if (driveInfo->controllerData.controllerID > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, controllerID, MAX_UINT16_TO_HEX_STRING_LENGTH);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(controllerID, MAX_UINT16_TO_HEX_STRING_LENGTH, "%04" PRIX16,
                                                   driveInfo->controllerData.controllerID),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(nvmeInfoObject, "Controller ID", json_object_new_string(controllerID)));
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(nvmeInfoObject, "Controller ID", json_object_new_string("Not Supported")));
    }

    if (driveInfo->controllerData.majorVersion > 0 || driveInfo->controllerData.minorVersion > 0 ||
        driveInfo->controllerData.tertiaryVersion > 0)
    {
        DECLARE_ZERO_INIT_ARRAY(char, nvmeVersion, MAX_STRING_NVME_VERSION_LENGTH);
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(nvmeVersion, MAX_STRING_NVME_VERSION_LENGTH, "%" PRIu16 ".%" PRIu8 ".%" PRIu8,
                                driveInfo->controllerData.majorVersion, driveInfo->controllerData.minorVersion,
                                driveInfo->controllerData.tertiaryVersion),
            "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(nvmeInfoObject, "NVMe Version", json_object_new_string(nvmeVersion)));
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(nvmeInfoObject, "NVMe Version",
                                                   json_object_new_string("Not reported (NVMe 1.1 or older)")));
    }

    if (driveInfo->controllerData.hostIdentifierSupported)
    {
        // TODO: Print out the host identifier
    }

    DECLARE_ZERO_INIT_ARRAY(uint8_t, zero128Bit, 16);
    if (memcmp(zero128Bit, driveInfo->controllerData.fguid, 16) != 0)
    {
        json_object* fguid = json_object_new_array();
        if (fguid == M_NULLPTR)
            return MEMORY_FAILURE;
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(nvmeInfoObject, "FGUID", fguid));
        for (uint8_t i = UINT8_C(0); i < 16; ++i)
        {
            DECLARE_ZERO_INIT_ARRAY(char, temp, MAX_UINT8_TO_HEX_STRING_LENGTH);
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(temp, MAX_UINT8_TO_HEX_STRING_LENGTH, "%02" PRIX8,
                                                       driveInfo->controllerData.fguid[i]),
                                   "JSON field buffer is sized for the fixed display format");
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Array_Element(fguid, json_object_new_string(temp)));
        }
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(nvmeInfoObject, "FGUID", json_object_new_string("Not Supported")));
    }

    if (driveInfo->controllerData.totalNVMCapacityD > 0)
    {
        // convert this to an "easy" unit instead of tons and tons of bytes
        DECLARE_ZERO_INIT_ARRAY(char, mTotalCapUnits, UNIT_STRING_LENGTH);
        DECLARE_ZERO_INIT_ARRAY(char, totalCapUnits, UNIT_STRING_LENGTH);
        char * mTotalCapUnit = &mTotalCapUnits[0], *totalCapUnit = &totalCapUnits[0];
        double mTotalCapacity = driveInfo->controllerData.totalNVMCapacityD;
        double totalCapacity  = mTotalCapacity;
        metric_Unit_Convert(&mTotalCapacity, &mTotalCapUnit);
        capacity_Unit_Convert(&totalCapacity, &totalCapUnit);
        DECLARE_ZERO_INIT_ARRAY(char, driveCapacity, MAX_DOUBLE_TO_DEC_STRING_LENGHT + MAX_DOUBLE_TO_DEC_STRING_LENGHT);
        DECLARE_ZERO_INIT_ARRAY(char, driveCapStr, MAX_STRING_CAPACITY_LENGTH);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(driveCapStr, MAX_STRING_CAPACITY_LENGTH,
                                                   "Total NVM Capacity (%s/%s)", mTotalCapUnit, totalCapUnit),
                               "JSON field buffer is sized for the fixed display format");
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(driveCapacity,
                                                   (MAX_DOUBLE_TO_DEC_STRING_LENGHT + MAX_DOUBLE_TO_DEC_STRING_LENGHT),
                                                   "%0.02f/%0.02f", mTotalCapacity, totalCapacity),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(nvmeInfoObject, driveCapStr, json_object_new_string(driveCapacity)));

        if (driveInfo->controllerData.unallocatedNVMCapacityD > 0)
        {
            DECLARE_ZERO_INIT_ARRAY(char, mUnCapUnits, UNIT_STRING_LENGTH);
            DECLARE_ZERO_INIT_ARRAY(char, unCapUnits, UNIT_STRING_LENGTH);
            char * mUnCapUnit = &mUnCapUnits[0], *unCapUnit = &unCapUnits[0];
            double mUnCapacity = driveInfo->controllerData.unallocatedNVMCapacityD;
            double unCapacity  = mUnCapacity;
            metric_Unit_Convert(&mUnCapacity, &mUnCapUnit);
            capacity_Unit_Convert(&unCapacity, &unCapUnit);

            DECLARE_ZERO_INIT_ARRAY(char, unNVMCapacity,
                                    MAX_DOUBLE_TO_DEC_STRING_LENGHT + MAX_DOUBLE_TO_DEC_STRING_LENGHT);
            DECLARE_ZERO_INIT_ARRAY(char, unNVMCapStr, MAX_STRING_UNALLOCATED_NVM_CAPACITY_LENGTH);
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(unNVMCapStr, MAX_STRING_UNALLOCATED_NVM_CAPACITY_LENGTH,
                                                       "Unallocated NVM Capacity (%s/%s)", mUnCapUnit, unCapUnit),
                                   "JSON field buffer is sized for the fixed display format");
            M_IGNORE_SAFE_INT_CALL(
                snprintf_err_handle(unNVMCapacity, (MAX_DOUBLE_TO_DEC_STRING_LENGHT + MAX_DOUBLE_TO_DEC_STRING_LENGHT),
                                    "%0.02f/%0.02f", mUnCapacity, unCapacity),
                "JSON field buffer is sized for the fixed display format");
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(nvmeInfoObject, unNVMCapStr, json_object_new_string(unNVMCapacity)));
        }
    }

    if (driveInfo->controllerData.volatileWriteCacheSupported)
    {
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(
            nvmeInfoObject, "Write Cache",
            json_object_new_string(driveInfo->controllerData.volatileWriteCacheEnabled ? "Enabled" : "Disabled")));
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(nvmeInfoObject, "Write Cache", json_object_new_string("Not Supported")));
    }

    DECLARE_ZERO_INIT_ARRAY(char, maxNumNamespace, MAX_UINT32_TO_DEC_STRING_LENGTH);
    M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(maxNumNamespace, MAX_UINT32_TO_DEC_STRING_LENGTH, "%" PRIu32,
                                               driveInfo->controllerData.maxNumberOfNamespaces),
                           "JSON field buffer is sized for the fixed display format");
    RETURN_ON_DRIVE_JSON_ERROR(
        add_JSON_Object(nvmeInfoObject, "Maximum Number Of Namespaces", json_object_new_string(maxNumNamespace)));

    DECLARE_ZERO_INIT_ARRAY(char, numOfPowerState, MAX_UINT8_TO_DEC_STRING_LENGTH);
    M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(numOfPowerState, MAX_UINT8_TO_DEC_STRING_LENGTH, "%" PRIu8,
                                               (driveInfo->controllerData.numberOfPowerStatesSupported + 1)),
                           "JSON field buffer is sized for the fixed display format");
    RETURN_ON_DRIVE_JSON_ERROR(
        add_JSON_Object(nvmeInfoObject, "Number of supported power states", json_object_new_string(numOfPowerState)));

    // Putting SMART & DST data here so that it isn't confused with the namespace data below - TJE
    if (driveInfo->smartData.valid)
    {
        if (driveInfo->smartData.mediumIsReadOnly)
        {
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(nvmeInfoObject, "Read-Only Medium", json_object_new_string("True")));
        }
        else
        {
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(nvmeInfoObject, "Read-Only Medium", json_object_new_string("False")));
        }

        switch (driveInfo->smartData.smartStatus)
        {
        case 0:
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(nvmeInfoObject, "SMART Status", json_object_new_string("Good")));
            break;
        case 1:
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(nvmeInfoObject, "SMART Status", json_object_new_string("Bad")));
            break;
        case 2:
        default:
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(nvmeInfoObject, "SMART Status", json_object_new_string("Unknown")));
            break;
        }

        DECLARE_ZERO_INIT_ARRAY(char, temp, MAX_UINT16_TO_DEC_STRING_LENGTH);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(temp, MAX_UINT16_TO_DEC_STRING_LENGTH, "%" PRIu16,
                                                   driveInfo->smartData.compositeTemperatureKelvin),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(nvmeInfoObject, "Composite Temperature (K)", json_object_new_string(temp)));

        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(temp, MAX_UINT16_TO_DEC_STRING_LENGTH, "%" PRIu8, driveInfo->smartData.percentageUsed),
            "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(nvmeInfoObject, "Percent Used (%)", json_object_new_string(temp)));

        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(temp, MAX_UINT16_TO_DEC_STRING_LENGTH, "%" PRIu8,
                                                   driveInfo->smartData.availableSpacePercent),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(nvmeInfoObject, "Available Spare (%)", json_object_new_string(temp)));

        uint16_t days    = UINT16_C(0);
        uint8_t  years   = UINT8_C(0);
        uint8_t  hours   = UINT8_C(0);
        uint8_t  minutes = UINT8_C(0);
        uint8_t  seconds = UINT8_C(0);
        convert_Seconds_To_Displayable_Time_Double(driveInfo->smartData.powerOnHoursD * 3600.0, &years, &days, &hours,
                                                   &minutes, &seconds);
        DECLARE_ZERO_INIT_ARRAY(char, timeString, MAX_TIME_STRING_LENGTH);
        // Append each time component to the string if it's greater than 0
        create_Time_String(timeString, years, days, hours, minutes, seconds);
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(nvmeInfoObject, "Power On Time", json_object_new_string(timeString)));

        DECLARE_ZERO_INIT_ARRAY(char, poh, MAX_DOUBLE_TO_DEC_STRING_LENGHT);
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(poh, MAX_DOUBLE_TO_DEC_STRING_LENGHT, "%0.00f", driveInfo->smartData.powerOnHoursD),
            "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(nvmeInfoObject, "Power On Hours (hours)", json_object_new_string(poh)));

        // Last DST information
        if (driveInfo->dstInfo.informationValid)
        {
            if (driveInfo->smartData.powerOnHoursD - C_CAST(double, (driveInfo->dstInfo.powerOnHours)) <
                driveInfo->smartData.powerOnHoursD)
            {
                json_object* dstDataObj = json_object_new_object();
                if (dstDataObj == M_NULLPTR)
                    return MEMORY_FAILURE;
                RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(nvmeInfoObject, "Last DST information", dstDataObj));

                double timeSinceLastDST = C_CAST(double, driveInfo->smartData.powerOnHoursD) -
                                          C_CAST(double, driveInfo->dstInfo.powerOnHours);
                if (timeSinceLastDST >= 0)
                {
                    DECLARE_ZERO_INIT_ARRAY(char, timeLastDst, MAX_DOUBLE_TO_DEC_STRING_LENGHT);
                    M_IGNORE_SAFE_INT_CALL(
                        snprintf_err_handle(timeLastDst, MAX_DOUBLE_TO_DEC_STRING_LENGHT, "%0.02f", timeSinceLastDST),
                        "JSON field buffer is sized for the fixed display format");
                    RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(dstDataObj, "Time since last DST (hours)",
                                                               json_object_new_string(timeLastDst)));
                }
                else
                {
                    RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(dstDataObj, "Time since last DST (hours)",
                                                               json_object_new_string("Indeterminate")));
                }
                DECLARE_ZERO_INIT_ARRAY(char, statusResult, MAX_UINT8_TO_HEX_STRING_LENGTH);
                M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(statusResult, MAX_UINT8_TO_HEX_STRING_LENGTH, "0x%" PRIX8,
                                                           driveInfo->dstInfo.resultOrStatus),
                                       "JSON field buffer is sized for the fixed display format");
                RETURN_ON_DRIVE_JSON_ERROR(
                    add_JSON_Object(dstDataObj, "DST Status/Result", json_object_new_string(statusResult)));

                DECLARE_ZERO_INIT_ARRAY(char, testRun, MAX_UINT8_TO_HEX_STRING_LENGTH);
                M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(testRun, MAX_UINT8_TO_HEX_STRING_LENGTH, "0x%" PRIX8,
                                                           driveInfo->dstInfo.testNumber),
                                       "JSON field buffer is sized for the fixed display format");
                RETURN_ON_DRIVE_JSON_ERROR(
                    add_JSON_Object(dstDataObj, "DST Test run", json_object_new_string(testRun)));

                if (driveInfo->dstInfo.resultOrStatus != 0 && driveInfo->dstInfo.resultOrStatus != 0xF &&
                    driveInfo->dstInfo.errorLBA != UINT64_MAX)
                {
                    // Show the Error LBA
                    DECLARE_ZERO_INIT_ARRAY(char, errLBA, MAX_UINT64_TO_DEC_STRING_LENGTH);
                    M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(errLBA, MAX_UINT64_TO_DEC_STRING_LENGTH, "0x%" PRIu64,
                                                               driveInfo->dstInfo.errorLBA),
                                           "JSON field buffer is sized for the fixed display format");
                    RETURN_ON_DRIVE_JSON_ERROR(
                        add_JSON_Object(dstDataObj, "Error occurred at LBA", json_object_new_string(errLBA)));
                }
            }
            else
            {
                RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(nvmeInfoObject, "Last DST information",
                                                           json_object_new_string("DST has never been run")));
            }
        }
        else
        {
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(nvmeInfoObject, "Last DST information", json_object_new_string("Not supported")));
        }

        // Long DST time
        if (driveInfo->controllerData.longDSTTimeMinutes > UINT64_C(0))
        {
            // print as hours:minutes
            years   = UINT8_C(0);
            days    = UINT16_C(0);
            hours   = UINT8_C(0);
            minutes = UINT8_C(0);
            seconds = UINT8_C(0);
            convert_Seconds_To_Displayable_Time(driveInfo->controllerData.longDSTTimeMinutes * UINT64_C(60), &years,
                                                &days, &hours, &minutes, &seconds);
            DECLARE_ZERO_INIT_ARRAY(char, dstTimeString, MAX_TIME_STRING_LENGTH);
            // Append each time component to the string if it's greater than 0
            create_Time_String(dstTimeString, years, days, hours, minutes, seconds);
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(nvmeInfoObject, "Long Drive Self Test Time", json_object_new_string(dstTimeString)));
        }
        else
        {
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(nvmeInfoObject, "Long Drive Self Test Time", json_object_new_string("Not Supported")));
        }

        // Workload Rate (Annualized)
#ifndef MINUTES_IN_1_YEAR
#    define MINUTES_IN_1_YEAR 525600.0
#endif // !MINUTES_IN_1_YEAR
        double totalTerabytesRead    = (driveInfo->smartData.dataUnitsReadD * 512.0 * 1000.0) / 1000000000000.0;
        double totalTerabytesWritten = (driveInfo->smartData.dataUnitsWrittenD * 512.0 * 1000.0) / 1000000000000.0;
        double calculatedUsage       = C_CAST(double, totalTerabytesRead + totalTerabytesWritten) *
                                 C_CAST(double, MINUTES_IN_1_YEAR / (driveInfo->smartData.powerOnHoursD * 60.0));
        DECLARE_ZERO_INIT_ARRAY(char, usage, MAX_DOUBLE_TO_DEC_STRING_LENGHT);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(usage, MAX_DOUBLE_TO_DEC_STRING_LENGHT, "%0.02f", calculatedUsage),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(nvmeInfoObject, "Annualized Workload Rate (TB/yr)", json_object_new_string(usage)));

        // Total Bytes Read
        double totalBytesRead = driveInfo->smartData.dataUnitsReadD * 512.0 * 1000.0;
        DECLARE_ZERO_INIT_ARRAY(char, unitReadString, UNIT_STRING_LENGTH);
        char* unitRead = &unitReadString[0];
        metric_Unit_Convert(&totalBytesRead, &unitRead);

        DECLARE_ZERO_INIT_ARRAY(char, bytesRead, MAX_DOUBLE_TO_DEC_STRING_LENGHT);
        DECLARE_ZERO_INIT_ARRAY(char, bytesReadStr, MAX_STRING_BYTES_READ_LENGTH);
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(bytesReadStr, MAX_STRING_BYTES_READ_LENGTH, "Total Bytes Read (%s)", unitRead),
            "JSON field buffer is sized for the fixed display format");
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(bytesRead, MAX_DOUBLE_TO_DEC_STRING_LENGHT, "%0.02f", totalBytesRead),
            "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(nvmeInfoObject, bytesReadStr, json_object_new_string(bytesRead)));

        // Total Bytes Written
        double totalBytesWritten = driveInfo->smartData.dataUnitsWrittenD * 512.0 * 1000.0;
        DECLARE_ZERO_INIT_ARRAY(char, unitWrittenString, UNIT_STRING_LENGTH);
        char* unitWritten = &unitWrittenString[0];
        metric_Unit_Convert(&totalBytesWritten, &unitWritten);
        DECLARE_ZERO_INIT_ARRAY(char, bytesWritten, MAX_DOUBLE_TO_DEC_STRING_LENGHT);
        DECLARE_ZERO_INIT_ARRAY(char, bytesWrittenStr, MAX_STRING_BYTES_WRITTEN_LENGTH);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(bytesWrittenStr, MAX_STRING_BYTES_WRITTEN_LENGTH,
                                                   "Total Bytes Written (%s)", unitWritten),
                               "JSON field buffer is sized for the fixed display format");
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(bytesWritten, MAX_DOUBLE_TO_DEC_STRING_LENGHT, "%0.02f", totalBytesWritten),
            "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(nvmeInfoObject, bytesWrittenStr, json_object_new_string(bytesWritten)));
    }

    // Encryption Support
    switch (driveInfo->controllerData.encryptionSupport)
    {
    case ENCRYPTION_SELF_ENCRYPTING:
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(nvmeInfoObject, "Encryption Support", json_object_new_string("Self Encrypting")));
        break;
    case ENCRYPTION_FULL_DISK:
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(nvmeInfoObject, "Encryption Support", json_object_new_string("Full Disk Encryption")));
        break;
    case ENCRYPTION_NONE:
    default:
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(nvmeInfoObject, "Encryption Support", json_object_new_string("Not Supported")));
        break;
    }

    // number of firmware slots
    DECLARE_ZERO_INIT_ARRAY(char, slots, MAX_UINT8_TO_DEC_STRING_LENGTH);
    M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(slots, MAX_UINT8_TO_DEC_STRING_LENGTH, "%" PRIu8,
                                               driveInfo->controllerData.numberOfFirmwareSlots),
                           "JSON field buffer is sized for the fixed display format");
    RETURN_ON_DRIVE_JSON_ERROR(
        add_JSON_Object(nvmeInfoObject, "Number of Firmware Slots", json_object_new_string(slots)));

    // Print out Controller features! (admin commands, etc)
    json_object* controllerFeature = json_object_new_array();
    if (controllerFeature == M_NULLPTR)
        return MEMORY_FAILURE;
    RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(nvmeInfoObject, "Controller Features", controllerFeature));
    for (uint16_t featureIter = UINT16_C(0); featureIter < driveInfo->controllerData.numberOfControllerFeatures;
         ++featureIter)
    {
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Array_Element(
            controllerFeature,
            json_object_new_string(driveInfo->controllerData.controllerFeaturesSupported[featureIter])));
    }
    if (driveInfo->namespaceData.valid)
    {
        json_object* namespaceObj = json_object_new_object();
        if (namespaceObj == M_NULLPTR)
            return MEMORY_FAILURE;
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "NVMe Namespace Information", namespaceObj));

        // Namespace size
        DECLARE_ZERO_INIT_ARRAY(char, mSizeUnits, UNIT_STRING_LENGTH);
        DECLARE_ZERO_INIT_ARRAY(char, sizeUnits, UNIT_STRING_LENGTH);
        char*  mSizeUnit = &mSizeUnits[0];
        char*  sizeUnit  = &sizeUnits[0];
        double nvmMSize =
            C_CAST(double, driveInfo->namespaceData.namespaceSize * driveInfo->namespaceData.formattedLBASizeBytes);
        double nvmSize = nvmMSize;
        metric_Unit_Convert(&nvmMSize, &mSizeUnit);
        capacity_Unit_Convert(&nvmSize, &sizeUnit);
        DECLARE_ZERO_INIT_ARRAY(char, namespaceSize,
                                (MAX_DOUBLE_TO_DEC_STRING_LENGHT + MAX_DOUBLE_TO_DEC_STRING_LENGHT));
        DECLARE_ZERO_INIT_ARRAY(char, namespaceStr, MAX_STRING_NAMESPACE_SIZE_LENGTH);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(namespaceStr, MAX_STRING_NAMESPACE_SIZE_LENGTH,
                                                   "Namespace Size (%s/%s)", mSizeUnit, sizeUnit),
                               "JSON field buffer is sized for the fixed display format");
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(namespaceSize, ((MAX_DOUBLE_TO_DEC_STRING_LENGHT + MAX_DOUBLE_TO_DEC_STRING_LENGHT)),
                                "%0.02f/%0.02f", nvmMSize, nvmSize),
            "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(namespaceObj, namespaceStr, json_object_new_string(namespaceSize)));

        // Namespace size in LBA
        DECLARE_ZERO_INIT_ARRAY(char, namespaceSizeLBA, MAX_UINT64_TO_DEC_STRING_LENGTH);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(namespaceSizeLBA, MAX_UINT64_TO_DEC_STRING_LENGTH, "%" PRIu64,
                                                   driveInfo->namespaceData.namespaceSize),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(namespaceObj, "Namespace Size (LBAs)", json_object_new_string(namespaceSizeLBA)));

        // namespace capacity
        DECLARE_ZERO_INIT_ARRAY(char, mCapUnits, UNIT_STRING_LENGTH);
        DECLARE_ZERO_INIT_ARRAY(char, capUnits, UNIT_STRING_LENGTH);
        char*  mCapUnit = &mCapUnits[0];
        char*  capUnit  = &capUnits[0];
        double nvmMCap =
            C_CAST(double, driveInfo->namespaceData.namespaceCapacity * driveInfo->namespaceData.formattedLBASizeBytes);
        double nvmCap = nvmMCap;
        metric_Unit_Convert(&nvmMCap, &mCapUnit);
        capacity_Unit_Convert(&nvmCap, &capUnit);

        DECLARE_ZERO_INIT_ARRAY(char, namespaceCap,
                                (MAX_DOUBLE_TO_DEC_STRING_LENGHT + MAX_DOUBLE_TO_DEC_STRING_LENGHT));
        DECLARE_ZERO_INIT_ARRAY(char, namespaceCapStr, MAX_STRING_CAPACITY_LENGTH);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(namespaceCapStr, MAX_STRING_CAPACITY_LENGTH,
                                                   "Namespace Capacity (%s/%s)", mCapUnit, capUnit),
                               "JSON field buffer is sized for the fixed display format");
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(namespaceCap,
                                                   (MAX_DOUBLE_TO_DEC_STRING_LENGHT + MAX_DOUBLE_TO_DEC_STRING_LENGHT),
                                                   "%0.02f/%0.02f", nvmMCap, nvmCap),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(namespaceObj, namespaceCapStr, json_object_new_string(namespaceCap)));

        // namespace capacity in LBA
        DECLARE_ZERO_INIT_ARRAY(char, namespaceCapLBA, MAX_UINT64_TO_DEC_STRING_LENGTH);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(namespaceCapLBA, MAX_UINT64_TO_DEC_STRING_LENGTH, "%" PRIu64,
                                                   driveInfo->namespaceData.namespaceCapacity),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(namespaceObj, "Namespace Capacity (LBAs)", json_object_new_string(namespaceCapLBA)));

        // namespace utilization
        DECLARE_ZERO_INIT_ARRAY(char, mUtilizationUnits, UNIT_STRING_LENGTH);
        DECLARE_ZERO_INIT_ARRAY(char, utilizationUnits, UNIT_STRING_LENGTH);
        char*  mUtilizationUnit = &mUtilizationUnits[0];
        char*  utilizationUnit  = &utilizationUnits[0];
        double nvmMUtilization  = C_CAST(double, driveInfo->namespaceData.namespaceUtilization *
                                                     driveInfo->namespaceData.formattedLBASizeBytes);
        double nvmUtilization   = nvmMUtilization;
        metric_Unit_Convert(&nvmMUtilization, &mUtilizationUnit);
        capacity_Unit_Convert(&nvmUtilization, &utilizationUnit);

        DECLARE_ZERO_INIT_ARRAY(char, utilization, (MAX_DOUBLE_TO_DEC_STRING_LENGHT + MAX_DOUBLE_TO_DEC_STRING_LENGHT));
        DECLARE_ZERO_INIT_ARRAY(char, utilizationStr, MAX_STRING_NAMESPACE_UTILIZATION_SIZE_LENGTH);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(utilizationStr, MAX_STRING_NAMESPACE_UTILIZATION_SIZE_LENGTH,
                                                   "Namespace Utilization (%s/%s)", mUtilizationUnit, utilizationUnit),
                               "JSON field buffer is sized for the fixed display format");
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(utilization,
                                                   (MAX_DOUBLE_TO_DEC_STRING_LENGHT + MAX_DOUBLE_TO_DEC_STRING_LENGHT),
                                                   "%0.02f/%0.02f", nvmMUtilization, nvmUtilization),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(namespaceObj, utilizationStr, json_object_new_string(utilization)));

        // namespace utilization in LBA
        DECLARE_ZERO_INIT_ARRAY(char, utilizationLBA, MAX_UINT64_TO_DEC_STRING_LENGTH);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(utilizationLBA, MAX_UINT64_TO_DEC_STRING_LENGTH, "%" PRIu64,
                                                   driveInfo->namespaceData.namespaceUtilization),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(namespaceObj, "Namespace Utilization (LBAs)", json_object_new_string(utilizationLBA)));

        // Formatted LBA Size
        DECLARE_ZERO_INIT_ARRAY(char, logicalBlockSize, MAX_UINT32_TO_DEC_STRING_LENGTH);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(logicalBlockSize, MAX_UINT32_TO_DEC_STRING_LENGTH, "%" PRIu32,
                                                   driveInfo->namespaceData.formattedLBASizeBytes),
                               "JSON field buffer is sized for the fixed display format");
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(namespaceObj, "Logical Block Size (B)", json_object_new_string(logicalBlockSize)));

        // relative performance
        switch (driveInfo->namespaceData.relativeFormatPerformance)
        {
        case 0:
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(namespaceObj, "Logical Block Size Relative Performance",
                                                       json_object_new_string("Best Performance")));
            break;
        case 1:
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(namespaceObj, "Logical Block Size Relative Performance",
                                                       json_object_new_string("Better Performance")));
            break;
        case 2:
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(namespaceObj, "Logical Block Size Relative Performance",
                                                       json_object_new_string("Good Performance")));
            break;
        case 3:
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(namespaceObj, "Logical Block Size Relative Performance",
                                                       json_object_new_string("Degraded Performance")));
            break;
        default: // this case shouldn't ever happen...just reducing a warning - TJE
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(namespaceObj, "Logical Block Size Relative Performance",
                                                       json_object_new_string("Unknown Performance")));
            break;
        }

        if (driveInfo->namespaceData.nvmCapacityD > 0)
        {
            M_INITIALIZE_STRUCTURE(mCapUnits, UNIT_STRING_LENGTH);
            M_INITIALIZE_STRUCTURE(capUnits, UNIT_STRING_LENGTH);
            double mCapacity = driveInfo->namespaceData.nvmCapacityD;
            double capacity  = mCapacity;
            metric_Unit_Convert(&mCapacity, &mCapUnit);
            capacity_Unit_Convert(&capacity, &capUnit);
            DECLARE_ZERO_INIT_ARRAY(char, cap, MAX_DOUBLE_TO_DEC_STRING_LENGHT + MAX_DOUBLE_TO_DEC_STRING_LENGHT);
            DECLARE_ZERO_INIT_ARRAY(char, capStr, MAX_STRING_CAPACITY_LENGTH);
            M_IGNORE_SAFE_INT_CALL(
                snprintf_err_handle(capStr, MAX_STRING_CAPACITY_LENGTH, "NVM Capacity (%s/%s)", mCapUnit, capUnit),
                "JSON field buffer is sized for the fixed display format");
            M_IGNORE_SAFE_INT_CALL(
                snprintf_err_handle(cap, (MAX_DOUBLE_TO_DEC_STRING_LENGHT + MAX_DOUBLE_TO_DEC_STRING_LENGHT),
                                    "%0.02f/%0.02f", mCapacity, capacity),
                "JSON field buffer is sized for the fixed display format");
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(namespaceObj, capStr, json_object_new_string(cap)));
        }

        if (memcmp(zero128Bit, driveInfo->namespaceData.namespaceGloballyUniqueIdentifier, 16) != 0)
        {
            char nguid[33] = {0};
            for (uint8_t i = UINT8_C(0); i < 16; ++i)
            {
                DECLARE_ZERO_INIT_ARRAY(char, temp, MAX_UINT8_TO_HEX_STRING_LENGTH);
                M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(temp, MAX_UINT8_TO_HEX_STRING_LENGTH, "%02" PRIX8,
                                                           driveInfo->controllerData.fguid[i]),
                                       "JSON field buffer is sized for the fixed display format");
                M_IGNORE_SAFE_ERRNO_CALL(safe_strcat(nguid, sizeof(nguid), temp),
                                         "sixteen two-character hexadecimal values fit in nguid");
            }
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(namespaceObj, "NGUID", json_object_new_string(nguid)));
        }
        else
        {
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(namespaceObj, "NGUID", json_object_new_string("Not Supported")));
        }

        if (driveInfo->namespaceData.ieeeExtendedUniqueIdentifier != 0)
        {
            DECLARE_ZERO_INIT_ARRAY(char, temp, MAX_UINT64_TO_HEX_STRING_LENGTH);
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(temp, MAX_UINT64_TO_HEX_STRING_LENGTH, "%016" PRIX64,
                                                       driveInfo->namespaceData.ieeeExtendedUniqueIdentifier),
                                   "JSON field buffer is sized for the fixed display format");
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(namespaceObj, "EUI64", json_object_new_string(temp)));
        }
        else
        {
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(namespaceObj, "EUI64", json_object_new_string("Not Supported")));
        }

        // Namespace features.
        json_object* features = json_object_new_array();
        if (features == M_NULLPTR)
            return MEMORY_FAILURE;
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(namespaceObj, "Namespace Features", features));
        for (uint16_t featureIter = UINT16_C(0); featureIter < driveInfo->namespaceData.numberOfNamespaceFeatures;
             ++featureIter)
        {
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Array_Element(
                features, json_object_new_string(driveInfo->namespaceData.namespaceFeaturesSupported[featureIter])));
        }
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "NVMe Namespace Information",
                                                   json_object_new_string("ERROR: Could not get namespace data!")));
    }

    return SUCCESS;
}

M_NODISCARD static eReturnValues create_JSON_For_Device_Information(json_object*        rootObject,
                                                                    ptrDriveInformation driveInfo)
{
    eReturnValues ret = NOT_SUPPORTED;
    switch (driveInfo->infoType)
    {
    case DRIVE_INFO_SAS_SATA:
        ret = create_JSON_Node_For_SAS_Sata_Device_Information(rootObject, &driveInfo->sasSata);
        break;
    case DRIVE_INFO_NVME:
        ret = create_JSON_Node_For_NVMe_Device_Information(rootObject, &driveInfo->nvme);
        break;
    default:
        break;
    }
    return ret;
}

M_NODISCARD static eReturnValues create_JSON_Node_For_Parent_And_Child_Information(
    json_object*        rootObject,
    ptrDriveInformation translatorDriveInfo,
    ptrDriveInformation driveInfo)
{
    DISABLE_NONNULL_COMPARE
    eReturnValues ret = NOT_SUPPORTED;

    if (translatorDriveInfo != M_NULLPTR && translatorDriveInfo->infoType == DRIVE_INFO_SAS_SATA)
    {
        json_object* translatorDriveInfoObject = json_object_new_object();
        if (translatorDriveInfoObject == M_NULLPTR)
        {
            return MEMORY_FAILURE;
        }
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "SCSI Translator Information", translatorDriveInfoObject));
        ret = create_JSON_For_Device_Information(translatorDriveInfoObject, translatorDriveInfo);
    }
    else
    {
        RETURN_ON_DRIVE_JSON_ERROR(
            add_JSON_Object(rootObject, "SCSI Translator Information", json_object_new_string("Not Available")));
    }

    if (driveInfo != M_NULLPTR)
    {
        json_object* nodeInfoObject = json_object_new_object();
        if (nodeInfoObject == M_NULLPTR)
        {
            return MEMORY_FAILURE;
        }

        if (driveInfo->infoType == DRIVE_INFO_SAS_SATA)
        {
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "ATA Reported Information", nodeInfoObject));
            ret = create_JSON_For_Device_Information(nodeInfoObject, driveInfo);
        }
        else if (driveInfo != M_NULLPTR && driveInfo->infoType == DRIVE_INFO_NVME)
        {
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "NVMe Reported Information", nodeInfoObject));
            ret = create_JSON_For_Device_Information(nodeInfoObject, driveInfo);
        }
        else if (driveInfo != M_NULLPTR)
        {
            RETURN_ON_DRIVE_JSON_ERROR(add_JSON_Object(rootObject, "Unknown device Information type", nodeInfoObject));
            ret = create_JSON_For_Device_Information(nodeInfoObject, driveInfo);
        }
        else
        {
            RETURN_ON_DRIVE_JSON_ERROR(
                add_JSON_Object(rootObject, "Drive Information", json_object_new_string("Not Available")));
        }
    }
    RESTORE_NONNULL_COMPARE

    return ret;
}

OPENSEA_JSONFORMAT_API eReturnValues create_JSON_Output_For_Drive_Information(const tDevice* M_NONNULL device,
                                                                              bool showChildInformation,
                                                                              const char* M_NONNULL utilityName,
                                                                              const char* M_NONNULL buildVersion,
                                                                              char**                jsonFormat)
{
    eReturnValues ret = NOT_SUPPORTED;
    if (device == M_NULLPTR || jsonFormat == M_NULLPTR)
    {
        return BAD_PARAMETER;
    }

    *jsonFormat = M_NULLPTR;

    json_object* rootNode = json_object_new_object();
    if (rootNode == M_NULLPTR)
        return MEMORY_FAILURE;

    if (create_Node_For_Utility_Version(rootNode, utilityName, buildVersion, "Device Info",
                                        DRIVE_INFORMATION_JSON_VERSION) != SUCCESS ||
        create_Node_For_Drive_Information(rootNode, device) != SUCCESS)
    {
        json_object_put(rootNode);
        return MEMORY_FAILURE;
    }

    ptrDriveInformation    ataDriveInfo  = M_NULLPTR;
    ptrDriveInformation    scsiDriveInfo = M_NULLPTR;
    ptrDriveInformation    nvmeDriveInfo = M_NULLPTR;
    ptrDriveInformation    usbDriveInfo  = M_NULLPTR;
    eDriveTypeForPrintInfo driveType     = PRINT_INFO_FOR_UNKNOWN_DRIVE;
    ret = get_Drive_Information(device, showChildInformation, &ataDriveInfo, &scsiDriveInfo, &nvmeDriveInfo,
                                &usbDriveInfo, &driveType);

    if (ret == SUCCESS)
    {
        json_object* driveInfoNode = json_object_new_object();
        if (driveInfoNode == M_NULLPTR)
        {
            ret = MEMORY_FAILURE;
        }
        else if (add_JSON_Object(rootNode, "Device Detailed Information", driveInfoNode) != SUCCESS)
        {
            ret = MEMORY_FAILURE;
        }
        else
        {

            switch (driveType)
            {
            case PRINT_INFO_FOR_ATA_DRIVE:
                ret = create_JSON_For_Device_Information(driveInfoNode, ataDriveInfo);
                break;
            case PRINT_INFO_FOR_NVME_DRIVE:
                ret = create_JSON_For_Device_Information(driveInfoNode, nvmeDriveInfo);
                break;
            case PRINT_INFO_FOR_SCSI_DRIVE:
                ret = create_JSON_For_Device_Information(driveInfoNode, scsiDriveInfo);
                break;
            case PRINT_INFO_FOR_USB_DRIVE:
                ret = create_JSON_For_Device_Information(driveInfoNode, usbDriveInfo);
                break;
            case PRINT_INFO_FOR_ATA_SCSI_DRIVE:
                ret = create_JSON_Node_For_Parent_And_Child_Information(driveInfoNode, scsiDriveInfo, ataDriveInfo);
                break;
            case PRINT_INFO_FOR_NVME_SCSI_DRIVE:
                ret = create_JSON_Node_For_Parent_And_Child_Information(driveInfoNode, scsiDriveInfo, nvmeDriveInfo);
                break;
            case PRINT_INFO_FOR_UNKNOWN_DRIVE:
            default:
                break;
            }
        }
    }

    safe_free_drive_info(&ataDriveInfo);
    safe_free_drive_info(&scsiDriveInfo);
    safe_free_drive_info(&nvmeDriveInfo);
    safe_free_drive_info(&usbDriveInfo);

    if (ret == SUCCESS)
    {
        // Convert JSON object to formatted string
        const char* jstr =
            json_object_to_json_string_ext(rootNode, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);

        // copy the json output into string
        if (asprintf(jsonFormat, "%s", jstr) < 0)
        {
            ret = MEMORY_FAILURE;
        }
    }

    // Free the JSON object
    json_object_put(rootNode);

    return ret;
}
