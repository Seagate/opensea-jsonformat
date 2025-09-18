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
#include <drive_info.h>
#include <time_utils.h>
#include <string_utils.h>
#include <io_utils.h>
#include <unit_conversion.h>
#include <drive_information_json.h>

#define DRIVE_INFORMATION_JSON_MAJOR_VERSION              1
#define DRIVE_INFORMATION_JSON_MINOR_VERSION              0
#define DRIVE_INFORMATION_JSON_PATCH_VERSION              0

void create_Time_String(char *timeString, uint8_t years, uint16_t days, uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    char temp[64] = {0};
    if (years > 0)
    {
        snprintf_err_handle(temp, sizeof(temp), " %" PRIu8 " year%s", years, (years > 1) ? "s" : "");
        strcat(timeString, temp);
    }
    if (days > 0)
    {
        snprintf_err_handle(temp, sizeof(temp), " %" PRIu16 " day%s", days, (days > 1) ? "s" : "");
        strcat(timeString, temp);
    }
    if (hours > 0)
    {
        snprintf_err_handle(temp, sizeof(temp), " %" PRIu8 " hour%s", hours, (hours > 1) ? "s" : "");
        strcat(timeString, temp);
    }
    if (minutes > 0)
    {
        snprintf_err_handle(temp, sizeof(temp), " %" PRIu8 " minute%s", minutes, (minutes > 1) ? "s" : "");
        strcat(timeString, temp);
    }
    if (seconds > 0)
    {
        snprintf_err_handle(temp, sizeof(temp), " %" PRIu8 " second%s", seconds, (seconds > 1) ? "s" : "");
        strcat(timeString, temp);
    }
}

eReturnValues create_JSON_Node_For_Parent_And_Child_Information(json_object* rootObject, ptrDriveInformation translatorDriveInfo,
                                                                ptrDriveInformation driveInfo)
{
    DISABLE_NONNULL_COMPARE
    eReturnValues ret            = NOT_SUPPORTED;

    if (translatorDriveInfo != M_NULLPTR && translatorDriveInfo->infoType == DRIVE_INFO_SAS_SATA)
    {
        json_object* translatorDriveInfoObject = json_object_new_object();
        if (translatorDriveInfoObject == M_NULLPTR)
        {
            return MEMORY_FAILURE;
        }
        ret = create_JSON_For_Device_Information(translatorDriveInfoObject, translatorDriveInfo);
        json_object_object_add(rootObject, "SCSI Translator Reported Information", translatorDriveInfoObject);
    }
    else
    {
        json_object_object_add(rootObject, "SCSI Translator Information", json_object_new_string("Not Available"));
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
            ret = create_JSON_For_Device_Information(nodeInfoObject, driveInfo);
            json_object_object_add(rootObject, "ATA Reported Information", nodeInfoObject);
        }
        else if (driveInfo != M_NULLPTR && driveInfo->infoType == DRIVE_INFO_NVME)
        {
            ret = create_JSON_For_Device_Information(nodeInfoObject, driveInfo);
            json_object_object_add(rootObject, "NVMe Reported Information", nodeInfoObject);
        }
        else if (driveInfo != M_NULLPTR)
        {
            ret = create_JSON_For_Device_Information(nodeInfoObject, driveInfo);
            json_object_object_add(rootObject, "Unknown device Information type", nodeInfoObject);
        }
        else
        {
            json_object_object_add(rootObject, "Drive Information", json_object_new_string("Not Available"));
        }
    }
    return ret;
    RESTORE_NONNULL_COMPARE
}

eReturnValues create_JSON_Node_For_SAS_Sata_Device_Information(json_object* rootObject, ptrDriveInformationSAS_SATA driveInfo)
{
    eReturnValues ret       = SUCCESS;
    double mCapacity = 0.0;
    double capacity  = 0.0;
    DECLARE_ZERO_INIT_ARRAY(char, mCapUnits, UNIT_STRING_LENGTH);
    DECLARE_ZERO_INIT_ARRAY(char, capUnits, UNIT_STRING_LENGTH);
    char* mCapUnit = &mCapUnits[0];
    char* capUnit  = &capUnits[0];

    if (safe_strlen(driveInfo->vendorID))
    {
        json_object_object_add(rootObject, "Vendor ID", json_object_new_string(driveInfo->vendorID));
    }
    json_object_object_add(rootObject, "Model Number", json_object_new_string(driveInfo->modelNumber));
    json_object_object_add(rootObject, "Serial Number", json_object_new_string(driveInfo->serialNumber));

    if (safe_strlen(driveInfo->pcbaSerialNumber))
    {
        json_object_object_add(rootObject, "PCBA Serial Number", json_object_new_string(driveInfo->pcbaSerialNumber));
    }
    json_object_object_add(rootObject, "Firmware Revision", json_object_new_string(driveInfo->firmwareRevision));

    if (safe_strlen(driveInfo->satVendorID))
    {
        json_object_object_add(rootObject, "SAT Vendor ID", json_object_new_string(driveInfo->satVendorID));
    }
    if (safe_strlen(driveInfo->satProductID))
    {
        json_object_object_add(rootObject, "SAT Product ID", json_object_new_string(driveInfo->satProductID));
    }
    if (safe_strlen(driveInfo->satProductRevision))
    {
        json_object_object_add(rootObject, "SAT Product Rev", json_object_new_string(driveInfo->satProductRevision));
    }

    if (driveInfo->worldWideNameSupported)
    {
        char worldWideName[40];
        snprintf_err_handle(worldWideName, sizeof(worldWideName), "%016" PRIX64, driveInfo->worldWideName);

        if (driveInfo->worldWideNameExtensionValid)
        {
            snprintf_err_handle(worldWideName, sizeof(worldWideName), "%016" PRIX64 "%016" PRIX64, driveInfo->worldWideName,
                     driveInfo->worldWideNameExtension);
        }
        json_object_object_add(rootObject, "World Wide Name", json_object_new_string(worldWideName));
    }
    else
    {
        json_object_object_add(rootObject, "World Wide Name", json_object_new_string("Not Supported"));
    }

    if (driveInfo->dateOfManufactureValid) {
        char dateOfManufacture[20];
        snprintf_err_handle(dateOfManufacture, sizeof(dateOfManufacture), "Week %" PRIu8 ", %" PRIu16, driveInfo->manufactureWeek, driveInfo->manufactureYear);
        json_object_object_add(rootObject, "Date Of Manufacture", json_object_new_string(dateOfManufacture));
    }

    if (driveInfo->copyrightValid && safe_strlen(driveInfo->copyrightInfo))
    {
        json_object_object_add(rootObject, "Copyright", json_object_new_string(driveInfo->copyrightInfo));
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

    char driveCapacity[64];
    char driveCapStr[32];
    snprintf_err_handle(driveCapacity, sizeof(driveCapacity), "%0.02f/%0.02f", mCapacity, capacity);
    snprintf_err_handle(driveCapStr, sizeof(driveCapStr), "Drive Capacity (%s/%s)", mCapUnit, capUnit);
    json_object_object_add(rootObject, driveCapStr, json_object_new_string(driveCapacity));

    if (!(driveInfo->nativeMaxLBA == 0 || driveInfo->nativeMaxLBA == UINT64_MAX))
    {
        mCapacity =
            C_CAST(double, C_CAST(uint64_t, driveInfo->nativeMaxLBA) * C_CAST(uint64_t, driveInfo->logicalSectorSize));
        capacity = mCapacity;
        metric_Unit_Convert(&mCapacity, &mCapUnit);
        capacity_Unit_Convert(&capacity, &capUnit);
        char nativedriveCapacity[64];
        char nativeDriveStr[40];
        snprintf_err_handle(nativedriveCapacity, sizeof(nativedriveCapacity), "%0.02f/%0.02f", mCapacity, capacity);
        snprintf_err_handle(nativeDriveStr, sizeof(nativeDriveStr), "Native Drive Capacity (%s/%s)", mCapUnit, capUnit);
        json_object_object_add(rootObject, nativeDriveStr,
                               json_object_new_string(nativedriveCapacity));
    }

    // Temperature Data
    json_object* temperatureDataObj = json_object_new_object();
    if (temperatureDataObj == M_NULLPTR)
        return MEMORY_FAILURE;

    if (driveInfo->temperatureData.temperatureDataValid)
    {
        json_object_object_add(temperatureDataObj, "Current Temperature (C)",
                               json_object_new_int(driveInfo->temperatureData.currentTemperature));
    }
    else
    {
        json_object_object_add(temperatureDataObj, "Current Temperature (C)", json_object_new_string("Not Reported"));
    }

    if (driveInfo->temperatureData.highestValid)
    {
        json_object_object_add(temperatureDataObj, "Highest Temperature (C)",
                               json_object_new_int(driveInfo->temperatureData.highestTemperature));
    }
    else
    {
        json_object_object_add(temperatureDataObj, "Highest Temperature (C)", json_object_new_string("Not Reported"));
    }

    if (driveInfo->temperatureData.lowestValid)
    {
        json_object_object_add(temperatureDataObj, "Lowest Temperature (C)",
                               json_object_new_int(driveInfo->temperatureData.lowestTemperature));
    }
    else
    {
        json_object_object_add(temperatureDataObj, "Lowest Temperature (C)", json_object_new_string("Not Reported"));
    }
    json_object_object_add(rootObject, "Temperature Data", temperatureDataObj);

    if (driveInfo->humidityData.humidityDataValid)
    {
        json_object* humidityDataObj = json_object_new_object();
        if (humidityDataObj == M_NULLPTR)
            return MEMORY_FAILURE;
        // Humidity Data

        if (driveInfo->humidityData.humidityDataValid) 
        {
            // Current Humidity
            if (driveInfo->humidityData.currentHumidity == UINT8_MAX)
            {
                json_object_object_add(humidityDataObj, "Current Humidity (%)",
                                       json_object_new_string("Invalid Reading"));
            }
            else
            {
                json_object_object_add(humidityDataObj, "Current Humidity (%)",
                                       json_object_new_int(driveInfo->humidityData.currentHumidity));
            }
        } 
        else 
        {
            json_object_object_add(humidityDataObj, "Current Humidity (%)", json_object_new_string("Not Reported"));
        }

        // Highest Humidity
        if (driveInfo->humidityData.highestValid)
        {
            if (driveInfo->humidityData.highestHumidity == UINT8_MAX)
            {
                json_object_object_add(humidityDataObj, "Highest Humidity (%)",
                                       json_object_new_string("Invalid Reading"));
            }
            else
            {
                json_object_object_add(humidityDataObj, "Highest Humidity (%)",
                                       json_object_new_int(driveInfo->humidityData.highestHumidity));
            }
        }
        else
        {
            json_object_object_add(humidityDataObj, "Highest Humidity (%)", json_object_new_string("Not Reported"));
        }

        // Lowest Humidity
        if (driveInfo->humidityData.lowestValid)
        {
            if (driveInfo->humidityData.lowestHumidity == UINT8_MAX)
            {
                json_object_object_add(humidityDataObj, "Lowest Humidity (%)",
                                       json_object_new_string("Invalid Reading"));
            }
            else
            {
                json_object_object_add(humidityDataObj, "Lowest Humidity (%)",
                                       json_object_new_int(driveInfo->humidityData.lowestHumidity));
            }
        }
        else
        {
            json_object_object_add(humidityDataObj, "Lowest Humidity (%)", json_object_new_string("Not Reported"));
        }

        // Add the humidity data object to the root JSON object
        json_object_object_add(rootObject, "Humidity Data", humidityDataObj);
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
        char timeString[256] = {0};

        // Append each time component to the string if it's greater than 0
        create_Time_String(timeString, years, days, hours, minutes, seconds);
        // Add the formatted string to the JSON object
        json_object_object_add(rootObject, "Power On Time", json_object_new_string(timeString));
    }
    else
    {
        json_object_object_add(rootObject, "Power On Time", json_object_new_string("Not Reported"));
    }

    if (driveInfo->powerOnMinutesValid)
    {
        // convert to a double to display as xx.xx
        double powerOnHours = C_CAST(double, driveInfo->powerOnMinutes) / 60.00;
        char   poh[64];
        snprintf_err_handle(poh, sizeof(poh), "%0.02f", powerOnHours);
        json_object_object_add(rootObject, "Power On Hours", json_object_new_string(poh));
    }
    else
    {
        json_object_object_add(rootObject, "Power On Hours", json_object_new_string("Not Reported"));
    }

    // CHS
    if (driveInfo->ataLegacyCHSInfo.legacyCHSValid && driveInfo->maxLBA == 0)
    {
        // Default CHS
        char defaultCHS[20];
        snprintf_err_handle(defaultCHS, sizeof(defaultCHS), "%" PRIu16 " | %" PRIu8 " | %" PRIu8,
                 driveInfo->ataLegacyCHSInfo.numberOfLogicalCylinders, driveInfo->ataLegacyCHSInfo.numberOfLogicalHeads,
                 driveInfo->ataLegacyCHSInfo.numberOfLogicalSectorsPerTrack);
        json_object_object_add(rootObject, "Default CHS", json_object_new_string(defaultCHS));

        char currentCHS[20];
        snprintf_err_handle(currentCHS, sizeof(currentCHS), "%" PRIu16 " | %" PRIu8 " | %" PRIu8,
                 driveInfo->ataLegacyCHSInfo.numberOfCurrentLogicalCylinders,
                 driveInfo->ataLegacyCHSInfo.numberOfCurrentLogicalHeads,
                 driveInfo->ataLegacyCHSInfo.numberOfCurrentLogicalSectorsPerTrack);
        json_object_object_add(rootObject, "Current CHS", json_object_new_string(currentCHS));

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
        json_object_object_add(rootObject, "Simulated MaxLBA", json_object_new_int(simMaxLBA));
    }
    else
    {
        // MaxLBA
        json_object_object_add(rootObject, "MaxLBA", json_object_new_uint64(driveInfo->maxLBA));

        // Native Max LBA
        if (driveInfo->nativeMaxLBA == 0 || driveInfo->nativeMaxLBA == UINT64_MAX)
        {
            json_object_object_add(rootObject, "Native MaxLBA", json_object_new_string("Not Reported"));

        }
        else
        {
            json_object_object_add(rootObject, "Native MaxLBA", json_object_new_uint64(driveInfo->nativeMaxLBA));
        }
    }

    if (driveInfo->isFormatCorrupt)
    {
        // Logical Sector Size
        json_object_object_add(rootObject, "Logical Sector Size (B)", json_object_new_string("Format Corrupt"));
        // Physical Sector Size
        json_object_object_add(rootObject, "Physical Sector Size (B)", json_object_new_string("Format Corrupt"));
        // Sector Alignment
        json_object_object_add(rootObject, "Sector Alignment", json_object_new_string("Format Corrupt"));
    }
    else
    {
        // Logical Sector Size
        json_object_object_add(rootObject, "Logical Sector Size (B)", json_object_new_int(driveInfo->logicalSectorSize));
        // Physical Sector Size
        json_object_object_add(rootObject, "Physical Sector Size (B)", json_object_new_int(driveInfo->physicalSectorSize));
        // Sector Alignment
        json_object_object_add(rootObject, "Sector Alignment", json_object_new_int(driveInfo->sectorAlignment));
    }
    // Rotation Rate
    if (driveInfo->rotationRate == 0)
    {
        json_object_object_add(rootObject, "Rotation Rate (RPM)", json_object_new_string("Not Reported"));
    }
    else if (driveInfo->rotationRate == 0x0001)
    {
        json_object_object_add(rootObject, "Rotation Rate (RPM)", json_object_new_string("SSD"));
    }
    else
    {
        json_object_object_add(rootObject, "Rotation Rate (RPM)", json_object_new_int(driveInfo->rotationRate));
    }

    if (driveInfo->isWriteProtected)
    {
        //printf("\tMedium is write protected!\n");
        json_object_object_add(rootObject, "Medium is write protected", json_object_new_string("true"));
    }

    // Form Factor
    switch (driveInfo->formFactor)
    {
    case 1:
        json_object_object_add(rootObject, "Form Factor", json_object_new_string("5.25\""));
        break;
    case 2:
        json_object_object_add(rootObject, "Form Factor", json_object_new_string("3.5\""));
        break;
    case 3:
        json_object_object_add(rootObject, "Form Factor", json_object_new_string("2.5\""));
        break;
    case 4:
        json_object_object_add(rootObject, "Form Factor", json_object_new_string("1.8\""));
        break;
    case 5:
        json_object_object_add(rootObject, "Form Factor", json_object_new_string("Less than 1.8\""));
        break;
    case 6:
        json_object_object_add(rootObject, "Form Factor", json_object_new_string("mSATA"));
        break;
    case 7:
        json_object_object_add(rootObject, "Form Factor", json_object_new_string("M.2"));
        break;
    case 8:
        json_object_object_add(rootObject, "Form Factor", json_object_new_string("MicroSSD"));
        break;
    case 9:
        json_object_object_add(rootObject, "Form Factor", json_object_new_string("CFast"));
        break;
    case 0:
    default:
        json_object_object_add(rootObject, "Form Factor", json_object_new_string("Not Reported"));
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
                char timeLastDst[10];
                snprintf_err_handle(timeLastDst, sizeof(timeLastDst), "%0.02f", timeSinceLastDST);
                json_object_object_add(dstDataObj, "Time since last DST (hours)", json_object_new_string(timeLastDst));
            }
            else
            {
                json_object_object_add(dstDataObj, "Time since last DST (hours)", json_object_new_string("Indeterminate"));
            }
            char statusResult[10];
            snprintf_err_handle(statusResult, sizeof(statusResult), "0x%" PRIX8, driveInfo->dstInfo.resultOrStatus);
            json_object_object_add(dstDataObj, "DST Status/Result", json_object_new_string(statusResult));

            char testRun[10];
            snprintf_err_handle(testRun, sizeof(testRun), "0x%" PRIX8, driveInfo->dstInfo.testNumber);
            json_object_object_add(dstDataObj, "DST Test run", json_object_new_string(testRun));

            if (driveInfo->dstInfo.resultOrStatus != 0 && driveInfo->dstInfo.resultOrStatus != 0xF &&
                driveInfo->dstInfo.errorLBA != UINT64_MAX)
            {
                // Show the Error LBA
                json_object_object_add(dstDataObj, "Error occurred at LBA",
                                       json_object_new_uint64(driveInfo->dstInfo.errorLBA));
            }
            json_object_object_add(rootObject, "Last DST information", dstDataObj);
        }
        else
        {
            json_object_object_add(rootObject, "Last DST information", json_object_new_string("DST has never been run"));
        }
    }
    else
    {
        json_object_object_add(rootObject, "Last DST information", json_object_new_string("Not Reported"));
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
        char timeString[256] = {0};
        // Append each time component to the string if it's greater than 0
        create_Time_String(timeString, years, days, hours, minutes, seconds);
        json_object_object_add(rootObject, "Long Drive Self Test Time", json_object_new_string(timeString));   
    }
    else
    {
        json_object_object_add(rootObject, "Long Drive Self Test Time", json_object_new_string("Not Supported")); 
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
                        json_object_object_add(speedDataObj, "Max Speed (Gb/s)", json_object_new_string("22.5"));
                        break;
                    case 4:
                        json_object_object_add(speedDataObj, "Max Speed (Gb/s)", json_object_new_string("12.0"));
                        break;
                    case 3:
                        json_object_object_add(speedDataObj, "Max Speed (Gb/s)", json_object_new_string("6.0"));
                        break;
                    case 2:
                        json_object_object_add(speedDataObj, "Max Speed (Gb/s)", json_object_new_string("3.0"));
                        break;
                    case 1:
                        json_object_object_add(speedDataObj, "Max Speed (Gb/s)", json_object_new_string("1.5"));
                        break;
                    case 0:
                        json_object_object_add(speedDataObj, "Max Speed (Gb/s)", json_object_new_string("Not Reported"));
                        break;
                    default:
                        json_object_object_add(speedDataObj, "Max Speed (Gb/s)", json_object_new_string("Unknown"));
                        break;
                    }

                    switch (driveInfo->interfaceSpeedInfo.serialSpeed.portSpeedsNegotiated[0])
                    {
                    case 5:
                        json_object_object_add(speedDataObj, "Negotiated Speed (Gb/s)", json_object_new_string("22.5"));
                        break;
                    case 4:
                        json_object_object_add(speedDataObj, "Negotiated Speed (Gb/s)", json_object_new_string("12.0"));
                        break;
                    case 3:
                        json_object_object_add(speedDataObj, "Negotiated Speed (Gb/s)", json_object_new_string("6.0"));
                        break;
                    case 2:
                        json_object_object_add(speedDataObj, "Negotiated Speed (Gb/s)", json_object_new_string("3.0"));
                        break;
                    case 1:
                        json_object_object_add(speedDataObj, "Negotiated Speed (Gb/s)", json_object_new_string("1.5"));
                        break;
                    case 0:
                        json_object_object_add(speedDataObj, "Negotiated Speed (Gb/s)", json_object_new_string("Not Reported"));
                        break;
                    default:
                        json_object_object_add(speedDataObj, "Negotiated Speed (Gb/s)", json_object_new_string("Unknown"));
                        break;
                    }
                }
                else
                {
                    for (uint8_t portIter = UINT8_C(0);
                         portIter < driveInfo->interfaceSpeedInfo.serialSpeed.numberOfPorts && portIter < MAX_PORTS;
                         portIter++)
                    {
                        json_object* portObj = json_object_new_object();
                        if (portObj == M_NULLPTR)
                            return MEMORY_FAILURE;
                        char portString[25];
                        snprintf_err_handle(portString, sizeof(portString), "Port %" PRIu8, portIter);
                        if (driveInfo->interfaceSpeedInfo.serialSpeed.activePortNumber == portIter &&
                            driveInfo->interfaceSpeedInfo.serialSpeed.activePortNumber != UINT8_MAX)
                        {
                            snprintf_err_handle(portString, sizeof(portString), "Port %" PRIu8 " (Current Port)", portIter);
                        }

                        // Max Speed
                        switch (driveInfo->interfaceSpeedInfo.serialSpeed.portSpeedsMax[portIter])
                        {
                        case 5:
                            json_object_object_add(portObj, "Max Speed (GB/s)", json_object_new_string("22.5"));
                            break;
                        case 4:
                            json_object_object_add(portObj, "Max Speed (GB/s)", json_object_new_string("12.0"));
                            break;
                        case 3:
                            json_object_object_add(portObj, "Max Speed (GB/s)", json_object_new_string("6.0"));
                            break;
                        case 2:
                            json_object_object_add(portObj, "Max Speed (GB/s)", json_object_new_string("3.0"));
                            break;
                        case 1:
                            json_object_object_add(portObj, "Max Speed (GB/s)", json_object_new_string("1.5"));
                            break;
                        case 0:
                            json_object_object_add(portObj, "Max Speed (GB/s)", json_object_new_string("Not Reported"));
                            break;
                        default:
                            json_object_object_add(portObj, "Max Speed (GB/s)", json_object_new_string("Unknown"));
                            break;
                        }

                        // Negotiated speed
                        switch (driveInfo->interfaceSpeedInfo.serialSpeed.portSpeedsNegotiated[portIter])
                        {
                        case 5:
                            json_object_object_add(portObj, "Negotiated Speed (Gb/s)",
                                                   json_object_new_string("22.5"));
                            break;
                        case 4:
                            json_object_object_add(portObj, "Negotiated Speed (Gb/s)",
                                                   json_object_new_string("12.0"));
                            break;
                        case 3:
                            json_object_object_add(portObj, "Negotiated Speed (Gb/s)",
                                                   json_object_new_string("6.0"));
                            break;
                        case 2:
                            json_object_object_add(portObj, "Negotiated Speed (Gb/s)",
                                                   json_object_new_string("3.0"));
                            break;
                        case 1:
                            json_object_object_add(portObj, "Negotiated Speed (Gb/s)",
                                                   json_object_new_string("1.5"));
                            break;
                        case 0:
                            json_object_object_add(portObj, "Negotiated Speed (Gb/s)",
                                                   json_object_new_string("Not Reported"));
                            break;
                        default:
                            json_object_object_add(portObj, "Negotiated Speed (Gb/s)",
                                                   json_object_new_string("Unknown"));
                        }

                        json_object_object_add(speedDataObj, portString, portObj);
                    }
                }

                json_object_object_add(rootObject, "Interface speed", speedDataObj);
            }
            else
            {
                json_object_object_add(rootObject, "Interface speed", json_object_new_string("Not Reported"));
            }
        }
        else if (driveInfo->interfaceSpeedInfo.speedType == INTERFACE_SPEED_PARALLEL)
        {
            char temp[100];
            snprintf_err_handle(temp, sizeof(temp), "%0.02f", driveInfo->interfaceSpeedInfo.parallelSpeed.maxSpeed);
            if (driveInfo->interfaceSpeedInfo.parallelSpeed.maxModeNameValid)
            {
                snprintf_err_handle(temp, sizeof(temp), "%0.02f (%s)", driveInfo->interfaceSpeedInfo.parallelSpeed.maxSpeed,
                         driveInfo->interfaceSpeedInfo.parallelSpeed.maxModeName);
            }
            json_object_object_add(speedDataObj, "Max Speed (MB/s)", json_object_new_string(temp));

            if (driveInfo->interfaceSpeedInfo.parallelSpeed.negotiatedValid)
            {
                snprintf_err_handle(temp, sizeof(temp), "%0.02f", driveInfo->interfaceSpeedInfo.parallelSpeed.negotiatedSpeed);
                if (driveInfo->interfaceSpeedInfo.parallelSpeed.negModeNameValid)
                {
                    snprintf_err_handle(temp, sizeof(temp), "%0.02f (%s)",
                             driveInfo->interfaceSpeedInfo.parallelSpeed.negotiatedSpeed,
                             driveInfo->interfaceSpeedInfo.parallelSpeed.negModeName);
                }
                json_object_object_add(speedDataObj, "Negotiated Speed (MB/s)", json_object_new_string(temp));
            }
            else
            {
                json_object_object_add(speedDataObj, "Negotiated Speed (MB/s)", json_object_new_string("Not Reported"));
            }
            if (driveInfo->interfaceSpeedInfo.parallelSpeed.cableInfoType == CABLING_INFO_ATA &&
                driveInfo->interfaceSpeedInfo.parallelSpeed.ataCableInfo.cablingInfoValid)
            {
                if (driveInfo->interfaceSpeedInfo.parallelSpeed.ataCableInfo.ata80PinCableDetected)
                {
                    json_object_object_add(speedDataObj, "Cabling Detected", json_object_new_string("80-pin Cable"));
                }
                else
                {
                    json_object_object_add(speedDataObj, "Cabling Detected", json_object_new_string("40-pin Cable"));
                }

                json_object_object_add(
                    speedDataObj, "Device Number",
                    json_object_new_int(driveInfo->interfaceSpeedInfo.parallelSpeed.ataCableInfo.device1 ? UINT8_C(1)
                                                                                                         : UINT8_C(0)));

                switch (driveInfo->interfaceSpeedInfo.parallelSpeed.ataCableInfo.deviceNumberDetermined)
                {
                case 1:
                    json_object_object_add(speedDataObj, "Device Set by", json_object_new_string("Jumper"));
                    break;
                case 2:
                    json_object_object_add(speedDataObj, "Device Set by", json_object_new_string("Cable Select"));
                    break;
                default:
                    json_object_object_add(speedDataObj, "Device Set by", json_object_new_string("Unknown"));
                    break;
                }
            }

            json_object_object_add(rootObject, "Interface speed", speedDataObj);
        }
        else if (driveInfo->interfaceSpeedInfo.speedType == INTERFACE_SPEED_ANCIENT)
        {
            if (driveInfo->interfaceSpeedInfo.ancientHistorySpeed.dataTransferGt10MbS)
            {
                json_object_object_add(rootObject, "Interface speed", json_object_new_string(">10Mb/s"));
            }
            else if (driveInfo->interfaceSpeedInfo.ancientHistorySpeed.dataTransferGt5MbSLte10MbS)
            {
                json_object_object_add(rootObject, "Interface speed", json_object_new_string(">5Mb/s & <10Mb/s"));
            }
            else if (driveInfo->interfaceSpeedInfo.ancientHistorySpeed.dataTransferLte5MbS)
            {
                json_object_object_add(rootObject, "Interface speed", json_object_new_string("<5Mb/s"));
            }
            else
            {
                json_object_object_add(rootObject, "Interface speed", json_object_new_string("Not Reported"));
            }
        }
        else
        {
            json_object_object_add(rootObject, "Interface speed", json_object_new_string("Not Reported"));
        }
    }
    else
    {
        json_object_object_add(rootObject, "Interface speed", json_object_new_string("Not Reported"));
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
            char usage[64];
            snprintf_err_handle(usage, sizeof(usage), "%0.02f", calculatedUsage);
            json_object_object_add(rootObject, "Annualized Workload Rate (TB/yr)",
                                   json_object_new_string(usage));
        }
        else
        {
            json_object_object_add(rootObject, "Annualized Workload Rate (TB/yr)", json_object_new_string("0.00"));
        }
    }
    else
    {
        json_object_object_add(rootObject, "Annualized Workload Rate (TB/yr)", json_object_new_string("Not Reported"));
    }
    // Total Bytes Read
    if (driveInfo->totalBytesRead > 0)
    {
        double totalBytesRead = C_CAST(double, driveInfo->totalBytesRead);
        char   unitString[4]  = {'\0'};
        char*  unit           = &unitString[0];
        metric_Unit_Convert(&totalBytesRead, &unit);
        char bytesRead[64];
        char bytesReadStr[40];
        snprintf_err_handle(bytesReadStr, sizeof(bytesReadStr), "Total Bytes Read (%s)", unit);
        snprintf_err_handle(bytesRead, sizeof(bytesRead), "%0.02f", totalBytesRead);
        json_object_object_add(rootObject, bytesReadStr, json_object_new_string(bytesRead));
    }
    else
    {
        json_object_object_add(rootObject, "Total Bytes Read (B)", json_object_new_string("Not Reported"));
    }
    // Total Bytes Written
    if (driveInfo->totalBytesWritten > 0)
    {
        double totalBytesWritten = C_CAST(double, driveInfo->totalBytesWritten);
        char   unitString[4]     = {'\0'};
        char*  unit              = &unitString[0];
        metric_Unit_Convert(&totalBytesWritten, &unit);
        char bytesWritten[64];
        char bytesWrittenStr[40];
        snprintf_err_handle(bytesWrittenStr, sizeof(bytesWrittenStr), "Total Bytes Written (%s)", unit);
        snprintf_err_handle(bytesWritten, sizeof(bytesWritten), "%0.02f", totalBytesWritten);
        json_object_object_add(rootObject, bytesWrittenStr, json_object_new_string(bytesWritten));
    }
    else
    {
        json_object_object_add(rootObject, "Total Bytes Written (B)", json_object_new_string("Not Reported"));
    }
    // Drive reported Utilization
    if (driveInfo->deviceReportedUtilizationRate > 0)
    {
        char utilization[68];
        snprintf_err_handle(utilization, sizeof(utilization), "%0.04f", driveInfo->deviceReportedUtilizationRate);
        json_object_object_add(rootObject, "Drive Reported Utilization (%)", json_object_new_string(utilization));
    }
    // Encryption Support
    switch (driveInfo->encryptionSupport)
    {
    case ENCRYPTION_SELF_ENCRYPTING:
        json_object_object_add(rootObject, "Encryption Support", json_object_new_string("Self Encrypting"));
        break;
    case ENCRYPTION_FULL_DISK:
        json_object_object_add(rootObject, "Encryption Support", json_object_new_string("Full Disk Encryption"));
        break;
    case ENCRYPTION_NONE:
    default:
        json_object_object_add(rootObject, "Encryption Support", json_object_new_string("Not Supported"));
        break;
    }
    if (driveInfo->trustedCommandsBeingBlocked)
    {
        /* printf(
            "\t\tWARNING: OS/driver/HBA is blocking TCG commands over passthrough. Please enable it before running "
               "any TCG commands\n");*/
        json_object_object_add(rootObject, "WARNING", json_object_new_string("OS/driver/HBA is blocking TCG commands over passthrough. \nPlease enable it before running "
               "any TCG commands"));
    }
    // Cache Size -- convert to MB
    if (driveInfo->cacheSize > 0)
    {
        double cacheSize = C_CAST(double, driveInfo->cacheSize);
        DECLARE_ZERO_INIT_ARRAY(char, cacheUnit, UNIT_STRING_LENGTH);
        char* cachUnitPtr = &cacheUnit[0];
        capacity_Unit_Convert(&cacheSize, &cachUnitPtr);
        char cacheSizeVal[64];
        char cacheSizeStr[40];
        snprintf_err_handle(cacheSizeStr, sizeof(cacheSizeStr), "Cache Size (%s)", cacheUnit);
        snprintf_err_handle(cacheSizeVal, sizeof(cacheSizeVal), "%0.02f", cacheSize);
        json_object_object_add(rootObject, cacheSizeStr, json_object_new_string(cacheSizeVal));
    }
    else
    {
        json_object_object_add(rootObject, "Cache Size (MiB)", json_object_new_string("Not Reported"));
    }
    // Hybrid NAND Cache Size -- convert to GB
    if (driveInfo->hybridNANDSize > 0)
    {
        double cacheSize = C_CAST(double, driveInfo->hybridNANDSize);
        DECLARE_ZERO_INIT_ARRAY(char, cacheUnit, UNIT_STRING_LENGTH);
        char* cachUnitPtr = &cacheUnit[0];
        capacity_Unit_Convert(&cacheSize, &cachUnitPtr);
        char cacheSizeVal[64];
        char cacheSizeStr[40];
        snprintf_err_handle(cacheSizeStr, sizeof(cacheSizeStr), "Hybrid NAND Cache Size (%s)", cacheUnit);
        snprintf_err_handle(cacheSizeVal, sizeof(cacheSizeVal), "%0.02f", cacheSize);
        json_object_object_add(rootObject, cacheSizeStr, json_object_new_string(cacheSizeVal));
    }
    // Percent Endurance Used
    if (driveInfo->rotationRate == 0x0001)
    {
        if (driveInfo->percentEnduranceUsed >= 0)
        {
            char enduranceUsed[70];
            snprintf_err_handle(enduranceUsed, sizeof(enduranceUsed), "%0.05f", driveInfo->percentEnduranceUsed);
            json_object_object_add(rootObject, "Percentage Used Endurance Indicator (%)",
                                   json_object_new_string(enduranceUsed));
        }
        else
        {
            json_object_object_add(rootObject, "Percentage Used Endurance Indicator (%)", json_object_new_string("Not Reported"));
        }
    }
    // Write Amplification
    if (driveInfo->rotationRate == 0x0001 && driveInfo->totalWritesToFlash > 0)
    {
        if (driveInfo->totalLBAsWritten > 0)
        {
            char writeAmplification[64];
            snprintf_err_handle(writeAmplification, sizeof(writeAmplification), "%0.02f",
                     C_CAST(double, driveInfo->totalWritesToFlash) / C_CAST(double, driveInfo->totalLBAsWritten));
            json_object_object_add(rootObject, "Write Amplification (%)", json_object_new_string(writeAmplification));
        }
        else
        {
            json_object_object_add(rootObject, "Write Amplification (%)", json_object_new_string("0"));
        }
    }
    // Read look ahead
    if (driveInfo->readLookAheadSupported)
    {
        json_object_object_add(rootObject, "Read Look-Ahead",
                               json_object_new_string(driveInfo->readLookAheadEnabled ? "Enabled" : "Disabled"));
    }
    else
    {
        json_object_object_add(rootObject, "Read Look-Ahead", json_object_new_string("Not Supported"));
    }
    // NVCache (!NV_DIS bit from caching MP)
    if (driveInfo->nvCacheSupported)
    {
        json_object_object_add(rootObject, "Non-Volatile Cache",
                               json_object_new_string(driveInfo->nvCacheEnabled ? "Enabled" : "Disabled"));
    }
    // Write Cache
    if (driveInfo->writeCacheSupported)
    {
        json_object_object_add(rootObject, "Write Cache",
                               json_object_new_string(driveInfo->writeCacheEnabled ? "Enabled" : "Disabled"));
    }
    else
    {
        json_object_object_add(rootObject, "Write Cache", json_object_new_string( "Not Supported"));
    }
    if (driveInfo->lowCurrentSpinupValid)
    {
        if (driveInfo->lowCurrentSpinupViaSCT) // to handle differences in reporting between 2.5" products and others
        {
            switch (driveInfo->lowCurrentSpinupEnabled)
            {
            case SEAGATE_LOW_CURRENT_SPINUP_STATE_LOW:
                json_object_object_add(rootObject, "Low Current Spinup", json_object_new_string("Enabled"));
                break;
            case SEAGATE_LOW_CURRENT_SPINUP_STATE_DEFAULT:
                json_object_object_add(rootObject, "Low Current Spinup", json_object_new_string("Disabled"));
                break;
            case SEAGATE_LOW_CURRENT_SPINUP_STATE_ULTRA_LOW:
                json_object_object_add(rootObject, "Low Current Spinup", json_object_new_string("Ultra Low Enabled"));
                break;
            default: {
                char unknownState[32];
                snprintf_err_handle(unknownState, sizeof(unknownState), "Unknown/Invalid state: %" PRIX16,
                         C_CAST(uint16_t, driveInfo->lowCurrentSpinupEnabled));
                json_object_object_add(rootObject, "Low Current Spinup", json_object_new_string(unknownState));
                break;
                }
            }
        }
        else
        {
            json_object_object_add(
                rootObject, "Low Current Spinup",
                json_object_new_string(driveInfo->lowCurrentSpinupEnabled > 0 ? "Enabled" : "Disabled"));
        }
    }
    // SMART Status
    switch (driveInfo->smartStatus)
    {
    case 0: // good
        json_object_object_add(rootObject, "SMART Status", json_object_new_string("Good"));
        break;
    case 1: // bad
        json_object_object_add(rootObject, "SMART Status", json_object_new_string("Tripped"));
        break;
    default: // unknown
        json_object_object_add(rootObject, "SMART Status", json_object_new_string("Unknown or Not Supported"));
        break;
    }
    // ATA Security Infomation
    if (driveInfo->ataSecurityInformation.securitySupported)
    {
        json_object* securityInfo = json_object_new_array();
        json_object_array_add(securityInfo, json_object_new_string("Supported"));
        if (driveInfo->ataSecurityInformation.securityEnabled)
            json_object_array_add(securityInfo, json_object_new_string("Enabled"));
        if (driveInfo->ataSecurityInformation.securityLocked)
            json_object_array_add(securityInfo, json_object_new_string("Locked"));
        if (driveInfo->ataSecurityInformation.securityFrozen)
            json_object_array_add(securityInfo, json_object_new_string("Frozen"));
        if (driveInfo->ataSecurityInformation.securityCountExpired)
            json_object_array_add(securityInfo, json_object_new_string("Password Count Expired"));
        json_object_object_add(rootObject, "ATA Security Information", securityInfo);
    }
    else
    {
        json_object_object_add(rootObject, "ATA Security Information", json_object_new_string("Not Supported"));
    }
    // Zoned Device Type
    if (driveInfo->zonedDevice > 0)
    {
        switch (driveInfo->zonedDevice)
        {
        case 0x1: // host aware
            json_object_object_add(rootObject, "Zoned Device Type", json_object_new_string("Host Aware"));
            break;
        case 0x2: // host managed
            json_object_object_add(rootObject, "Zoned Device Type", json_object_new_string("Device Managed"));
            break;
        case 0x3: // reserved
            json_object_object_add(rootObject, "Zoned Device Type", json_object_new_string("Reserved"));
            break;
        default:
            json_object_object_add(rootObject, "Zoned Device Type", json_object_new_string("Not a Zoned Device"));
            break;
        }
    }

    if (driveInfo->fwdlSupport.downloadSupported)
    {
        json_object* fwSupport = json_object_new_array();
        json_object_array_add(fwSupport, json_object_new_string("Full"));
        if (driveInfo->fwdlSupport.segmentedSupported)
        {
            if (driveInfo->fwdlSupport.seagateDeferredPowerCycleRequired)
                json_object_array_add(fwSupport, json_object_new_string("Segmented as Deferred - Power Cycle Activation Only"));
            else 
                json_object_array_add(fwSupport, json_object_new_string("Segmented"));
        }
        if (driveInfo->fwdlSupport.deferredSupported)
        {
            json_object_array_add(fwSupport, json_object_new_string("Deferred"));
        }
        if (driveInfo->fwdlSupport.dmaModeSupported)
        {
            json_object_array_add(fwSupport, json_object_new_string("DMA"));
        }
        json_object_object_add(rootObject, "Firmware Download Support", fwSupport);
    }
    else
    {
        json_object_object_add(rootObject, "Firmware Download Support", json_object_new_string("Not Supported"));
    }

    if (driveInfo->lunCount > 0)
    {
        json_object_object_add(rootObject, "Number of Logical Units", json_object_new_int(driveInfo->lunCount));
    }
    if (driveInfo->concurrentPositioningRanges > 0)
    {
        json_object_object_add(rootObject, "Number of Concurrent Ranges",
                               json_object_new_int(driveInfo->concurrentPositioningRanges));
    }
    // Specifications Supported
    if (driveInfo->numberOfSpecificationsSupported > 0)
    {
        json_object* specificationSupport          = json_object_new_array();
        uint8_t specificationsIter = UINT8_C(0);
        for (specificationsIter = 0;
             specificationsIter < driveInfo->numberOfSpecificationsSupported && specificationsIter < MAX_SPECS;
             specificationsIter++)
        {
            json_object_array_add(specificationSupport,
                                  json_object_new_string(driveInfo->specificationsSupported[specificationsIter]));
        }
        json_object_object_add(rootObject, "Specifications Supported", specificationSupport);
    }
    else
    {
        json_object_object_add(rootObject, "Specifications Supported",
                               json_object_new_string("None reported by device"));
    }
    // Features Supported
    if (driveInfo->numberOfFeaturesSupported > 0)
    {
        uint8_t featuresIter = UINT8_C(0);
        json_object* featuresSupport = json_object_new_array();
        for (featuresIter = 0; featuresIter < driveInfo->numberOfFeaturesSupported && featuresIter < MAX_FEATURES;
             featuresIter++)
        {
            json_object_array_add(featuresSupport, json_object_new_string(driveInfo->featuresSupported[featuresIter]));
        }
        json_object_object_add(rootObject, "Features Supported", featuresSupport);
    }
    else
    {
        json_object_object_add(rootObject, "Features Supported", json_object_new_string("None reported or an error occurred while trying to determine\n\t\tthe features."));
    }
    // Adapter information
    json_object* adapterInfo = json_object_new_object();
    if (adapterInfo == M_NULLPTR)
        return MEMORY_FAILURE;
    switch (driveInfo->adapterInformation.infoType)
    {
    case ADAPTER_INFO_USB:
        json_object_object_add(adapterInfo, "Adapter Type", json_object_new_string("USB"));
        break;
    case ADAPTER_INFO_PCI:
        json_object_object_add(adapterInfo, "Adapter Type", json_object_new_string("PCI"));
        break;
    case ADAPTER_INFO_IEEE1394:
        json_object_object_add(adapterInfo, "Adapter Type", json_object_new_string("IEEE1394"));
        break;
    case ADAPTER_INFO_UNKNOWN:
    default:
        json_object_object_add(adapterInfo, "Adapter Type", json_object_new_string("Unknown"));
        break;
    }
    if (driveInfo->adapterInformation.vendorIDValid)
    {
        char vendorID[64];
        snprintf_err_handle(vendorID, sizeof(vendorID), "%04" PRIX32 "h", driveInfo->adapterInformation.vendorID);
        json_object_object_add(adapterInfo, "Vendor ID", json_object_new_string(vendorID));
    }
    else
    {
        json_object_object_add(adapterInfo, "Vendor ID", json_object_new_string("Not available"));
    }
    if (driveInfo->adapterInformation.productIDValid)
    {
        char productID[64];
        snprintf_err_handle(productID, sizeof(productID), "%04" PRIX32 "h", driveInfo->adapterInformation.productID);
        json_object_object_add(adapterInfo, "Product ID", json_object_new_string(productID));
    }
    else
    {
        json_object_object_add(adapterInfo, "Product ID", json_object_new_string("Not available"));
    }
    if (driveInfo->adapterInformation.revisionValid)
    {
        char revision[64];
        snprintf_err_handle(revision, sizeof(revision), "%04" PRIX32 "h", driveInfo->adapterInformation.revision);
        json_object_object_add(adapterInfo, "Revision", json_object_new_string(revision));
    }
    else
    {
        json_object_object_add(adapterInfo, "Revision", json_object_new_string("Not available"));
    }
    if (driveInfo->adapterInformation
            .specifierIDValid) // IEEE1394 only, so it will only print when we get this set to true for now - TJE
    {
        char specifierID[64];
        snprintf_err_handle(specifierID, sizeof(specifierID), "%04" PRIX32 "h", driveInfo->adapterInformation.specifierID);
        json_object_object_add(adapterInfo, "Specifier ID", json_object_new_string(specifierID));
    }
    json_object_object_add(rootObject, "Adapter Information", adapterInfo);

    if (driveInfo->lunCount > 1)
    {
        //printf("This device has multiple actuators. Some commands/features may affect more than one actuator.\n");
        json_object_object_add(
            rootObject, "Info",
            json_object_new_string
            ("This device has multiple actuators. Some commands/features may affect more than one actuator."));
    }
    return ret;
}

eReturnValues create_JSON_Node_For_NVMe_Device_Information(json_object* rootObject, ptrDriveInformationNVMe driveInfo)
{
    eReturnValues ret            = SUCCESS;
    json_object* nvmeInfoObject = json_object_new_object();
    if (nvmeInfoObject == M_NULLPTR)
        return MEMORY_FAILURE;

    json_object_object_add(nvmeInfoObject, "Model Number",
                           json_object_new_string(driveInfo->controllerData.modelNumber));
    json_object_object_add(nvmeInfoObject, "Serial Number",
                           json_object_new_string(driveInfo->controllerData.serialNumber));
    json_object_object_add(nvmeInfoObject, "Firmware Revision",
                           json_object_new_string(driveInfo->controllerData.firmwareRevision));

    if (driveInfo->controllerData.ieeeOUI > 0)
    {
        char ieeeOUI[10];
        snprintf_err_handle(ieeeOUI, sizeof(ieeeOUI), "%06" PRIX32, driveInfo->controllerData.ieeeOUI);
        json_object_object_add(nvmeInfoObject, "IEEE OUI", json_object_new_string(ieeeOUI));
    }
    else
    {
        json_object_object_add(nvmeInfoObject, "IEEE OUI", json_object_new_string("Not Supported"));
    }

    char pciVendorID[8];
    snprintf_err_handle(pciVendorID, sizeof(pciVendorID), "%04" PRIX16, driveInfo->controllerData.pciVendorID);
    json_object_object_add(nvmeInfoObject, "PCI Vendor ID", json_object_new_string(pciVendorID));

    char pciSubVendorID[8];
    snprintf_err_handle(pciSubVendorID, sizeof(pciSubVendorID), "%04" PRIX16, driveInfo->controllerData.pciSubsystemVendorID);
    json_object_object_add(nvmeInfoObject, "PCI Subsystem Vendor ID", json_object_new_string(pciSubVendorID));

    if (driveInfo->controllerData.controllerID > 0)
    {
        char controllerID[8];
        snprintf_err_handle(controllerID, sizeof(controllerID), "%04" PRIX16, driveInfo->controllerData.controllerID);
        json_object_object_add(nvmeInfoObject, "Controller ID", json_object_new_string(controllerID));
    }
    else
    {
        json_object_object_add(nvmeInfoObject, "Controller ID", json_object_new_string("Not Supported"));
    }

    if (driveInfo->controllerData.majorVersion > 0 || driveInfo->controllerData.minorVersion > 0 ||
        driveInfo->controllerData.tertiaryVersion > 0)
    {
        char nvmeVersion[15];
        snprintf_err_handle(nvmeVersion, sizeof(nvmeVersion), "%" PRIu16 ".%" PRIu8 ".%" PRIu8,
                 driveInfo->controllerData.majorVersion, driveInfo->controllerData.minorVersion,
                 driveInfo->controllerData.tertiaryVersion);
        json_object_object_add(nvmeInfoObject, "NVMe Version", json_object_new_string(nvmeVersion));
    }
    else
    {
        json_object_object_add(nvmeInfoObject, "NVMe Version", json_object_new_string("Not reported (NVMe 1.1 or older)"));
    }
    if (driveInfo->controllerData.hostIdentifierSupported)
    {
        // TODO: Print out the host identifier
    }

    DECLARE_ZERO_INIT_ARRAY(uint8_t, zero128Bit, 16);
    if (memcmp(zero128Bit, driveInfo->controllerData.fguid, 16) != 0)
    {
        json_object* fguid = json_object_new_array();
        for (uint8_t i = UINT8_C(0); i < 16; ++i)
        {
            char temp[4];
            snprintf_err_handle(temp, sizeof(temp), "%02" PRIX8, driveInfo->controllerData.fguid[i]);
            json_object_array_add(fguid, json_object_new_string(temp));
        }
        json_object_object_add(nvmeInfoObject, "FGUID", fguid);
    }
    else
    {
        json_object_object_add(nvmeInfoObject, "FGUID", json_object_new_string("Not Supported"));
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
        char driveCapacity[64];
        char driveCapStr[40];
        snprintf_err_handle(driveCapStr, sizeof(driveCapStr), "Total NVM Capacity (%s/%s)", mTotalCapUnit,
                            totalCapUnit);
        snprintf_err_handle(driveCapacity, sizeof(driveCapacity), "%0.02f/%0.02f", mTotalCapacity, totalCapacity);
        json_object_object_add(nvmeInfoObject, driveCapStr, json_object_new_string(driveCapacity));

        if (driveInfo->controllerData.unallocatedNVMCapacityD > 0)
        {
            DECLARE_ZERO_INIT_ARRAY(char, mUnCapUnits, UNIT_STRING_LENGTH);
            DECLARE_ZERO_INIT_ARRAY(char, unCapUnits, UNIT_STRING_LENGTH);
            char * mUnCapUnit = &mUnCapUnits[0], *unCapUnit = &unCapUnits[0];
            double mUnCapacity = driveInfo->controllerData.unallocatedNVMCapacityD;
            double unCapacity  = mUnCapacity;
            metric_Unit_Convert(&mUnCapacity, &mUnCapUnit);
            capacity_Unit_Convert(&unCapacity, &unCapUnit);

            char unNVMCapacity[64];
            char unNVMCapStr[40];
            snprintf_err_handle(unNVMCapStr, sizeof(unNVMCapStr), "Unallocated NVM Capacity (%s/%s)", mUnCapUnit,
                                unCapUnit);
            snprintf_err_handle(unNVMCapacity, sizeof(unNVMCapacity), "%0.02f/%0.02f", mUnCapacity, unCapacity);
            json_object_object_add(nvmeInfoObject, unNVMCapStr, json_object_new_string(unNVMCapacity));
        }
    }

    if (driveInfo->controllerData.volatileWriteCacheSupported)
    {
        json_object_object_add(
            nvmeInfoObject, "Write Cache",
            json_object_new_string(driveInfo->controllerData.volatileWriteCacheEnabled ? "Enabled" : "Disabled"));
    }
    else
    {
        json_object_object_add(nvmeInfoObject, "Write Cache", json_object_new_string("Not Supported"));
    }
    json_object_object_add(nvmeInfoObject, "Maximum Number Of Namespaces", 
            json_object_new_int(driveInfo->controllerData.maxNumberOfNamespaces));

    json_object_object_add(nvmeInfoObject, "Number of supported power states",
                           json_object_new_int(driveInfo->controllerData.numberOfPowerStatesSupported + 1));
    // Putting SMART & DST data here so that it isn't confused with the namespace data below - TJE
    if (driveInfo->smartData.valid)
    {
        json_object_object_add(nvmeInfoObject, "Read-Only Medium",
                               json_object_new_boolean(driveInfo->smartData.mediumIsReadOnly));

        switch (driveInfo->smartData.smartStatus)
        {
        case 0:
            json_object_object_add(nvmeInfoObject, "SMART Status", json_object_new_string("Good"));
            break;
        case 1:
            json_object_object_add(nvmeInfoObject, "SMART Status", json_object_new_string("Bad"));
            break;
        case 2:
        default:
            json_object_object_add(nvmeInfoObject, "SMART Status", json_object_new_string("Unknown"));
            break;
        }
        // kelvin_To_Celsius(&driveInfo->smartData.compositeTemperatureKelvin);

        json_object_object_add(nvmeInfoObject, "Composite Temperature (K)",
                               json_object_new_int(driveInfo->smartData.compositeTemperatureKelvin));
        json_object_object_add(nvmeInfoObject, "Percent Used (%)",
                               json_object_new_int(driveInfo->smartData.percentageUsed));
        json_object_object_add(nvmeInfoObject, "Available Spare (%)",
                               json_object_new_int(driveInfo->smartData.availableSpacePercent));
        uint16_t days    = UINT16_C(0);
        uint8_t  years   = UINT8_C(0);
        uint8_t  hours   = UINT8_C(0);
        uint8_t  minutes = UINT8_C(0);
        uint8_t  seconds = UINT8_C(0);
        convert_Seconds_To_Displayable_Time_Double(driveInfo->smartData.powerOnHoursD * 3600.0, &years, &days, &hours,
                                                   &minutes, &seconds);
        char timeString[256] = {0};
        // Append each time component to the string if it's greater than 0
        create_Time_String(timeString, years, days, hours, minutes, seconds);
        json_object_object_add(nvmeInfoObject, "Power On Time", json_object_new_string(timeString));

        char poh[64];
        snprintf_err_handle(poh, sizeof(poh), "%0.00f", driveInfo->smartData.powerOnHoursD);
        json_object_object_add(nvmeInfoObject, "Power On Hours (hours)", json_object_new_string(poh));

        // Last DST information
        if (driveInfo->dstInfo.informationValid)
        {
            if (driveInfo->smartData.powerOnHoursD - C_CAST(double, (driveInfo->dstInfo.powerOnHours)) <
                driveInfo->smartData.powerOnHoursD)
            {
                json_object* dstDataObj       = json_object_new_object();
                if (dstDataObj == M_NULLPTR)
                    return MEMORY_FAILURE;

                double timeSinceLastDST = C_CAST(double, driveInfo->smartData.powerOnHoursD) -
                                          C_CAST(double, driveInfo->dstInfo.powerOnHours);
                if (timeSinceLastDST >= 0)
                {
                    char timeLastDst[10];
                    snprintf_err_handle(timeLastDst, sizeof(timeLastDst), "%0.02f", timeSinceLastDST);
                    json_object_object_add(dstDataObj, "Time since last DST (hours)",
                                           json_object_new_string(timeLastDst));
                }
                else
                {
                    json_object_object_add(dstDataObj, "Time since last DST (hours)",
                                           json_object_new_string("Indeterminate"));
                }
                char statusResult[10];
                snprintf_err_handle(statusResult, sizeof(statusResult), "0x%" PRIX8, driveInfo->dstInfo.resultOrStatus);
                json_object_object_add(dstDataObj, "DST Status/Result", json_object_new_string(statusResult));

                char testRun[10];
                snprintf_err_handle(testRun, sizeof(testRun), "0x%" PRIX8, driveInfo->dstInfo.testNumber);
                json_object_object_add(dstDataObj, "DST Test run", json_object_new_string(testRun));

                if (driveInfo->dstInfo.resultOrStatus != 0 && driveInfo->dstInfo.resultOrStatus != 0xF &&
                    driveInfo->dstInfo.errorLBA != UINT64_MAX)
                {
                    // Show the Error LBA
                    json_object_object_add(dstDataObj, "Error occurred at LBA",
                                           json_object_new_uint64(driveInfo->dstInfo.errorLBA));
                }
                json_object_object_add(nvmeInfoObject, "Last DST information", dstDataObj);
            }
            else
            {
                json_object_object_add(nvmeInfoObject, "Last DST information", json_object_new_string("DST has never been run"));
            }
        }
        else
        {
            json_object_object_add(nvmeInfoObject, "Last DST information", json_object_new_string("Not supported"));
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
            char dstTimeString[256] = {0};
            // Append each time component to the string if it's greater than 0
            create_Time_String(dstTimeString, years, days, hours, minutes, seconds);
            json_object_object_add(nvmeInfoObject, "Long Drive Self Test Time", json_object_new_string(dstTimeString));
        }
        else
        {
            json_object_object_add(nvmeInfoObject, "Long Drive Self Test Time", json_object_new_string("Not Supported"));
        }

        // Workload Rate (Annualized)
#ifndef MINUTES_IN_1_YEAR
#    define MINUTES_IN_1_YEAR 525600.0
#endif // !MINUTES_IN_1_YEAR
        double totalTerabytesRead    = (driveInfo->smartData.dataUnitsReadD * 512.0 * 1000.0) / 1000000000000.0;
        double totalTerabytesWritten = (driveInfo->smartData.dataUnitsWrittenD * 512.0 * 1000.0) / 1000000000000.0;
        double calculatedUsage       = C_CAST(double, totalTerabytesRead + totalTerabytesWritten) *
                                 C_CAST(double, MINUTES_IN_1_YEAR / (driveInfo->smartData.powerOnHoursD * 60.0));
        char usage[64];
        snprintf_err_handle(usage, sizeof(usage), "%0.02f", calculatedUsage);
        json_object_object_add(nvmeInfoObject, "Annualized Workload Rate (TB/yr)", json_object_new_string(usage));

        // Total Bytes Read
        double totalBytesRead = driveInfo->smartData.dataUnitsReadD * 512.0 * 1000.0;
        DECLARE_ZERO_INIT_ARRAY(char, unitReadString, UNIT_STRING_LENGTH);
        char* unitRead = &unitReadString[0];
        metric_Unit_Convert(&totalBytesRead, &unitRead);
        char bytesRead[64];
        char bytesReadStr[40];
        snprintf_err_handle(bytesReadStr, sizeof(bytesReadStr), "Total Bytes Read (%s)", unitRead);
        snprintf_err_handle(bytesRead, sizeof(bytesRead), "%0.02f", totalBytesRead);
        json_object_object_add(nvmeInfoObject, bytesReadStr, json_object_new_string(bytesRead));

        // Total Bytes Written
        double totalBytesWritten = driveInfo->smartData.dataUnitsWrittenD * 512.0 * 1000.0;
        DECLARE_ZERO_INIT_ARRAY(char, unitWrittenString, UNIT_STRING_LENGTH);
        char* unitWritten = &unitWrittenString[0];
        metric_Unit_Convert(&totalBytesWritten, &unitWritten);
        char bytesWritten[64];
        char bytesWrittenStr[40];
        snprintf_err_handle(bytesWrittenStr, sizeof(bytesWrittenStr), "Total Bytes Written (%s)", unitWritten);
        snprintf_err_handle(bytesWritten, sizeof(bytesWritten), "%0.02f", totalBytesWritten);
        json_object_object_add(nvmeInfoObject, bytesWrittenStr, json_object_new_string(bytesWritten));
    }

    // Encryption Support
    switch (driveInfo->controllerData.encryptionSupport)
    {
    case ENCRYPTION_SELF_ENCRYPTING:
        json_object_object_add(nvmeInfoObject, "Encryption Support", json_object_new_string("Self Encrypting"));
        /*if (driveInfo->trustedCommandsBeingBlocked)
        {
            printf("\t\tWARNING: OS is blocking TCG commands over passthrough. Please enable it before running any TCG
        commands\n");
        }*/
        break;
    case ENCRYPTION_FULL_DISK:
        json_object_object_add(nvmeInfoObject, "Encryption Support", json_object_new_string("Full Disk Encryption"));
        break;
    case ENCRYPTION_NONE:
    default:
        json_object_object_add(nvmeInfoObject, "Encryption Support", json_object_new_string("Not Supported"));
        break;
    }
    // number of firmware slots
    json_object_object_add(nvmeInfoObject, "Number of Firmware Slots",
                           json_object_new_int(driveInfo->controllerData.numberOfFirmwareSlots));
    // Print out Controller features! (admin commands, etc)

    json_object* controllerFeature = json_object_new_array();

    for (uint16_t featureIter = UINT16_C(0); featureIter < driveInfo->controllerData.numberOfControllerFeatures;
         ++featureIter)
    {
        json_object_array_add(
            controllerFeature,
            json_object_new_string(driveInfo->controllerData.controllerFeaturesSupported[featureIter]));
    }
    json_object_object_add(nvmeInfoObject, "Controller Features", controllerFeature);

    json_object_object_add(rootObject, "NVMe Controller Information", nvmeInfoObject);

    if (driveInfo->namespaceData.valid)
    {
        json_object* namespaceObj = json_object_new_object();
        if (namespaceObj == M_NULLPTR)
            return MEMORY_FAILURE;
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

        char namespaceSize[64];
        char namespaceStr[64];
        snprintf_err_handle(namespaceStr, sizeof(namespaceStr), "Namespace Size (%s/%s)", mSizeUnit, sizeUnit);
        snprintf_err_handle(namespaceSize, sizeof(namespaceSize), "%0.02f/%0.02f", nvmMSize, nvmSize);
        json_object_object_add(namespaceObj, namespaceStr, json_object_new_string(namespaceSize));

        json_object_object_add(namespaceObj, "Namespace Size (LBAs)",
                               json_object_new_uint64(driveInfo->namespaceData.namespaceSize));
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

        char namespaceCapacity[64];
        snprintf_err_handle(namespaceStr, sizeof(namespaceStr), "Namespace Capacity (%s/%s)", mCapUnit, capUnit);
        snprintf_err_handle(namespaceCapacity, sizeof(namespaceCapacity), "%0.02f/%0.02f", nvmMCap, nvmCap);
        json_object_object_add(namespaceObj, namespaceStr, json_object_new_string(namespaceCapacity));
        json_object_object_add(namespaceObj, "Namespace Capacity (LBAs)",
                               json_object_new_uint64(driveInfo->namespaceData.namespaceCapacity));

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

        char utilization[64];
        char utilizationStr[64];
        snprintf_err_handle(utilizationStr, sizeof(utilizationStr), "Namespace Utilization (%s/%s)", mUtilizationUnit,
                            utilizationUnit);
        snprintf_err_handle(utilization, sizeof(utilization), "%0.02f/%0.02f", nvmMUtilization, nvmUtilization);
        json_object_object_add(namespaceObj, utilizationStr, json_object_new_string(utilization));
        json_object_object_add(namespaceObj, "Namespace Utilization (LBAs)",
                               json_object_new_uint64(driveInfo->namespaceData.namespaceUtilization));


        // Formatted LBA Size
        json_object_object_add(namespaceObj, "Logical Block Size (B)",
                               json_object_new_int(driveInfo->namespaceData.formattedLBASizeBytes));

        // relative performance
        switch (driveInfo->namespaceData.relativeFormatPerformance)
        {
        case 0:
            json_object_object_add(namespaceObj, "Logical Block Size Relative Performance",
                                   json_object_new_string("Best Performance"));
            break;
        case 1:
            json_object_object_add(namespaceObj, "Logical Block Size Relative Performance",
                                   json_object_new_string("Better Performance"));
            break;
        case 2:
            json_object_object_add(namespaceObj, "Logical Block Size Relative Performance",
                                   json_object_new_string("Good Performance"));
            break;
        case 3:
            json_object_object_add(namespaceObj, "Logical Block Size Relative Performance",
                                   json_object_new_string("Degraded Performance"));
            break;
        default: // this case shouldn't ever happen...just reducing a warning - TJE
            json_object_object_add(namespaceObj, "Logical Block Size Relative Performance",
                                   json_object_new_string("Unknown Performance"));
            break;
        }
        if (driveInfo->namespaceData.nvmCapacityD > 0)
        {
            safe_memset(mCapUnits, UNIT_STRING_LENGTH, 0, UNIT_STRING_LENGTH * sizeof(char));
            safe_memset(capUnits, UNIT_STRING_LENGTH, 0, UNIT_STRING_LENGTH * sizeof(char));
            double mCapacity = driveInfo->namespaceData.nvmCapacityD;
            double capacity  = mCapacity;
            metric_Unit_Convert(&mCapacity, &mCapUnit);
            capacity_Unit_Convert(&capacity, &capUnit);
            char cap[64];
            char capStr[40];
            snprintf_err_handle(capStr, sizeof(capStr), "NVM Capacity (%s/%s)", mCapUnit, capUnit);
            snprintf_err_handle(cap, sizeof(cap), "%0.02f/%0.02f", mCapacity, capacity);
            json_object_object_add(namespaceObj, capStr, json_object_new_string(cap));
        }

        if (memcmp(zero128Bit, driveInfo->namespaceData.namespaceGloballyUniqueIdentifier, 16) != 0)
        {
            char         nguid[33] = {0};
            for (uint8_t i = UINT8_C(0); i < 16; ++i)
            {
                char temp[6];
                snprintf_err_handle(temp, sizeof(temp), "%02" PRIX8, driveInfo->controllerData.fguid[i]);
                strcat(nguid, temp);
            }
            json_object_object_add(namespaceObj, "NGUID", json_object_new_string(nguid));
        }
        else
        {
            json_object_object_add(namespaceObj, "NGUID", json_object_new_string("Not Supported"));
        }

        if (driveInfo->namespaceData.ieeeExtendedUniqueIdentifier != 0)
        {
            char temp[20];
            snprintf_err_handle(temp, sizeof(temp), "%016" PRIX64, driveInfo->namespaceData.ieeeExtendedUniqueIdentifier);
            json_object_object_add(namespaceObj, "EUI64", json_object_new_string(temp));
        }
        else
        {
            json_object_object_add(namespaceObj, "EUI64", json_object_new_string("Not Supported"));
        }
        // Namespace features.
        json_object* features = json_object_new_array();
        for (uint16_t featureIter = UINT16_C(0); featureIter < driveInfo->namespaceData.numberOfNamespaceFeatures;
             ++featureIter)
        {
            json_object_array_add(
                features, json_object_new_string(driveInfo->namespaceData.namespaceFeaturesSupported[featureIter]));
        }
        json_object_object_add(namespaceObj, "Namespace Features", features);

        json_object_object_add(rootObject, "NVMe Namespace Information", namespaceObj);
    }
    else
    {
        json_object_object_add(rootObject, "NVMe Namespace Information",
                               json_object_new_string("ERROR: Could not get namespace data!"));
    }

    return ret;
}

eReturnValues create_JSON_For_Device_Information(json_object* rootObject, ptrDriveInformation driveInfo)
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

eReturnValues create_JSON_Output_For_Drive_Information(tDevice* device, bool showChildInformation, char** jsonFormat)
{
    eReturnValues ret = NOT_SUPPORTED;
    if (device == M_NULLPTR)
    {
        return BAD_PARAMETER;
    }

    json_object* rootNode = json_object_new_object();
    if (rootNode == M_NULLPTR)
        return MEMORY_FAILURE;

    ptrDriveInformation ataDriveInfo  = M_NULLPTR;
    ptrDriveInformation scsiDriveInfo = M_NULLPTR;
    ptrDriveInformation usbDriveInfo  = M_NULLPTR;
    ptrDriveInformation nvmeDriveInfo = M_NULLPTR;

    ret = get_Drive_Information(device, &ataDriveInfo, &scsiDriveInfo, &usbDriveInfo, &nvmeDriveInfo);
    if (ret == SUCCESS && (ataDriveInfo || scsiDriveInfo || usbDriveInfo || nvmeDriveInfo))
    {
        // Create JSON output for each drive type
        if (showChildInformation &&
            (device->drive_info.drive_type != SCSI_DRIVE ||
             device->drive_info.passThroughHacks.ataPTHacks.possilbyEmulatedNVMe) &&
            scsiDriveInfo && (ataDriveInfo || nvmeDriveInfo))
        {
            if ((device->drive_info.drive_type == ATA_DRIVE ||
                 device->drive_info.passThroughHacks.ataPTHacks.possilbyEmulatedNVMe) &&
                ataDriveInfo)
            {
                ret = create_JSON_Node_For_Parent_And_Child_Information(rootNode, scsiDriveInfo, ataDriveInfo);
            }
            else if (device->drive_info.drive_type == NVME_DRIVE && nvmeDriveInfo)
            {
                ret = create_JSON_Node_For_Parent_And_Child_Information(rootNode, scsiDriveInfo, nvmeDriveInfo);
            }
        }
        else
        {
            // ONLY call the external function when we are able to get some passthrough information back as well
            if ((device->drive_info.interface_type == USB_INTERFACE ||
                 device->drive_info.interface_type == IEEE_1394_INTERFACE) &&
                ataDriveInfo && scsiDriveInfo && device->drive_info.drive_type == ATA_DRIVE)
            {
                usbDriveInfo = M_REINTERPRET_CAST(ptrDriveInformation, safe_calloc(1, sizeof(driveInformation)));
                if (usbDriveInfo != M_NULLPTR)
                {
                    usbDriveInfo->infoType = DRIVE_INFO_SAS_SATA;
                    generate_External_Drive_Information(&usbDriveInfo->sasSata, &scsiDriveInfo->sasSata,
                                                        &ataDriveInfo->sasSata);
                    ret = create_JSON_For_Device_Information(rootNode, usbDriveInfo);
                }
                else
                {
                    ret = MEMORY_FAILURE;
                    printf("Error allocating memory for USB - ATA drive info\n");
                }
            }
            else if (device->drive_info.interface_type == USB_INTERFACE &&
                     device->drive_info.drive_type == NVME_DRIVE && nvmeDriveInfo && scsiDriveInfo)
            {
                usbDriveInfo = M_REINTERPRET_CAST(ptrDriveInformation, safe_calloc(1, sizeof(driveInformation)));
                if (usbDriveInfo != M_NULLPTR)
                {
                    usbDriveInfo->infoType = DRIVE_INFO_SAS_SATA;
                    generate_External_NVMe_Drive_Information(&usbDriveInfo->sasSata, &scsiDriveInfo->sasSata,
                                                             &nvmeDriveInfo->nvme);
                    ret = create_JSON_For_Device_Information(rootNode, usbDriveInfo);
                }
                else
                {
                    ret = MEMORY_FAILURE;
                    printf("Error allocating memory for USB - NVMe drive info\n");
                }
            }
            else // ata or scsi
            {
                if (device->drive_info.drive_type == ATA_DRIVE && ataDriveInfo)
                {
                    ret = create_JSON_For_Device_Information(rootNode, ataDriveInfo);
                }
                else if (device->drive_info.drive_type == NVME_DRIVE && nvmeDriveInfo)
                {
                    ret = create_JSON_For_Device_Information(rootNode, nvmeDriveInfo);
                }
                else if (scsiDriveInfo != M_NULLPTR)
                {
                    ret = create_JSON_For_Device_Information(rootNode, scsiDriveInfo);
                }
                else
                {
                    printf("Error allocating memory to get device information.\n");
                }
            }
        }
    }

    safe_free_drive_info(&ataDriveInfo);
    safe_free_drive_info(&scsiDriveInfo);
    safe_free_drive_info(&usbDriveInfo);
    safe_free_drive_info(&nvmeDriveInfo);

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
