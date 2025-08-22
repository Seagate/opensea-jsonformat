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
// \file scsi_defect_list_json.c
// \brief This file defines types and functions related to the JSON-based output for Device Statistics log.

#include <json.h>
#include <json_object.h>
#include "scsi_defect_list_json.h"

eReturnValues create_JSON_Output_For_SCSI_Defect_List(ptrSCSIDefectList defects, char** jsonFormat)
{
    eReturnValues ret = NOT_SUPPORTED;
    bool          atleastOneStatisticsAvailable = false;

    if (defects == M_NULLPTR)
    {
        return BAD_PARAMETER;
    }

    json_object* scsiDefectJson = json_object_new_object();

    if (defects->containsPrimaryList)
    {
        json_object_object_add(scsiDefectJson, "Contains Primary List", json_object_new_boolean(true));
        atleastOneStatisticsAvailable = true;
    }

    if (defects->containsGrownList)
    {
        json_object_object_add(scsiDefectJson, "Contains Grown List", json_object_new_boolean(true));
        atleastOneStatisticsAvailable = true;
    }

    if (defects->generation > 0)
    {
        json_object_object_add(scsiDefectJson, "Generation Code", json_object_new_int(defects->generation));
        atleastOneStatisticsAvailable = true;
    }

    if (defects->deviceHasMultipleLogicalUnits)
    {
        json_object_object_add(scsiDefectJson, "Device Has Multiple Logical Units", json_object_new_boolean(true));
        atleastOneStatisticsAvailable = true;
    }

    switch (defects->format)
    {
    case AD_SHORT_BLOCK_FORMAT_ADDRESS_DESCRIPTOR:
    {
        atleastOneStatisticsAvailable = true;
        json_object_object_add(scsiDefectJson, "Format", json_object_new_string("Short Block Format"));

        if (defects->numberOfElements > UINT32_C(0))
        {
            json_object_object_add(scsiDefectJson, "Total Defects in List",
                                   json_object_new_int(defects->numberOfElements));

            json_object* defectList = json_object_new_array();
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
        atleastOneStatisticsAvailable = true;
        json_object_object_add(scsiDefectJson, "Format", json_object_new_string("Long Block Format"));

        if (defects->numberOfElements > UINT32_C(0))
        {
            json_object_object_add(scsiDefectJson, "Total Defects in List",
                                   json_object_new_int(defects->numberOfElements));

            json_object* defectList = json_object_new_array();
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
        atleastOneStatisticsAvailable = true;
        json_object_object_add(scsiDefectJson, "Format", json_object_new_string("Extended Physical Sector Format"));
        json_object_object_add(scsiDefectJson, "Total Defects in List", json_object_new_int(defects->numberOfElements));

        json_object* defectRanges = json_object_new_array();

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
                    json_object* rangeObj = json_object_new_object();
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
            json_object* rangeObj = json_object_new_object();
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
        atleastOneStatisticsAvailable = true;
        json_object_object_add(scsiDefectJson, "Format", json_object_new_string("Physical Sector Format"));
        json_object_object_add(scsiDefectJson, "Total Defects in List", json_object_new_int(defects->numberOfElements));

        json_object* defectRanges = json_object_new_array();

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
                    json_object* rangeObj = json_object_new_object();
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
            json_object* rangeObj = json_object_new_object();
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
        atleastOneStatisticsAvailable = true;
        json_object_object_add(scsiDefectJson, "Format", json_object_new_string("Extended Bytes From Index Format"));

        if (defects->numberOfElements > UINT32_C(0))
        {
            json_object_object_add(scsiDefectJson, "Total Defects in List",
                                   json_object_new_int(defects->numberOfElements));

            json_object* defectList = json_object_new_array();
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
        atleastOneStatisticsAvailable = true;
        json_object_object_add(scsiDefectJson, "Format", json_object_new_string("Bytes From Index Format"));

        if (defects->numberOfElements > UINT32_C(0))
        {
            json_object_object_add(scsiDefectJson, "Total Defects in List",
                                   json_object_new_int(defects->numberOfElements));

            json_object* defectList = json_object_new_array();

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

    }


        const char* jstr =
        json_object_to_json_string_ext(scsiDefectJson, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);


                    if (atleastOneStatisticsAvailable)
        {
            ret = SUCCESS;

            // copy the json output into string
            if (asprintf(jsonFormat, "%s", jstr) < 0)
            {
                ret = MEMORY_FAILURE;
            }
        }
        json_object_put(scsiDefectJson);
    return ret;
}