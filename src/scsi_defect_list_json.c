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
// \file scsi_defect_list_json.c
// \brief This file defines types and functions related to the JSON-based output for SCSI Defect log.

#include "scsi_defect_list_json.h"
#include "io_utils.h"

#define COMBINE_SCSI_DEFECT_LIST_JSON_VERSIONS_(x, y, z) #x "." #y "." #z
#define COMBINE_SCSI_DEFECT_LIST_JSON_VERSIONS(x, y, z)  COMBINE_SCSI_DEFECT_LIST_JSON_VERSIONS_(x, y, z)

#define SCSI_DEFECT_LIST_JSON_MAJOR_VERSION              1
#define SCSI_DEFECT_LIST_JSON_MINOR_VERSION              0
#define SCSI_DEFECT_LIST_JSON_PATCH_VERSION              0

#define SCSI_DEFECT_LIST_JSON_VERSION                                                                                  \
    COMBINE_SCSI_DEFECT_LIST_JSON_VERSIONS(SCSI_DEFECT_LIST_JSON_MAJOR_VERSION, SCSI_DEFECT_LIST_JSON_MINOR_VERSION,   \
                                           SCSI_DEFECT_LIST_JSON_PATCH_VERSION)

#define MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH 35
#define MAX_UINT64_TO_DEC_STRING_LENGHT    21
#define MAX_UINT32_TO_DEC_STRING_LENGHT    11
#define MAX_UINT16_TO_DEC_STRING_LENGHT    6
#define MAX_UINT8_TO_DEC_STRING_LENGHT     4

static void get_Address_Descriptor_Name(eSCSIAddressDescriptors addressDescriptorType, char** addressDescriptorName)
{
    safe_memset(*addressDescriptorName, MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH, 0, MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH);
    switch (addressDescriptorType)
    {
    case AD_SHORT_BLOCK_FORMAT_ADDRESS_DESCRIPTOR:
        snprintf_err_handle(*addressDescriptorName, MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH, "Short Block Format");
        break;

    case AD_EXTENDED_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR:
        snprintf_err_handle(*addressDescriptorName, MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH,
                            "Extended Bytes From Index Format");
        break;

    case AD_EXTENDED_PHYSICAL_SECTOR_FORMAT_ADDRESS_DESCRIPTOR:
        snprintf_err_handle(*addressDescriptorName, MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH,
                            "Extended Physical Sector Format");
        break;

    case AD_LONG_BLOCK_FORMAT_ADDRESS_DESCRIPTOR:
        snprintf_err_handle(*addressDescriptorName, MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH, "Long Block Format");
        break;

    case AD_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR:
        snprintf_err_handle(*addressDescriptorName, MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH, "Bytes From Index Format");
        break;

    case AD_PHYSICAL_SECTOR_FORMAT_ADDRESS_DESCRIPTOR:
        snprintf_err_handle(*addressDescriptorName, MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH, "Physical Sector Format");
        break;

    case AD_VENDOR_SPECIFIC:
    case AD_RESERVED:
    default:
        snprintf_err_handle(*addressDescriptorName, MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH, "Vendor Specific");
        break;
    }
}

OPENSEA_JSONFORMAT_API eReturnValues create_JSON_Output_For_SCSI_Defect_List(const tDevice* M_NONNULL    device,
                                                                             ptrSCSIDefectList M_NONNULL defects,
                                                                             const char* M_NONNULL       utilityName,
                                                                             const char* M_NONNULL       buildVersion,
                                                                             char**                      jsonFormat)
{
    if (defects == M_NULLPTR)
    {
        return BAD_PARAMETER;
    }

    json_object* rootNode = json_object_new_object();

    create_Node_For_Utility_Version(rootNode, utilityName, buildVersion, "SCSI Defect List",
                                    SCSI_DEFECT_LIST_JSON_VERSION);
    create_Node_For_Drive_Information(rootNode, device);

    // Add general information about the defect list
    {
        // create node
        json_object* scsiDefectNode = json_object_new_object();

        // add details
        json_object_object_add(scsiDefectNode, "Contains Primary List",
                               json_object_new_boolean(defects->containsPrimaryList));
        json_object_object_add(scsiDefectNode, "Contains Grown List",
                               json_object_new_boolean(defects->containsGrownList));
        if (defects->generation > 0)
        {
            DECLARE_ZERO_INIT_ARRAY(char, generationCodeValue, MAX_UINT16_TO_DEC_STRING_LENGHT);
            snprintf_err_handle(generationCodeValue, MAX_UINT16_TO_DEC_STRING_LENGHT, "%" PRIu16 "",
                                defects->generation);
            json_object_object_add(scsiDefectNode, "Generation Code", json_object_new_string(generationCodeValue));
        }
        json_object_object_add(scsiDefectNode, "Device Has Multiple Logical Units",
                               json_object_new_boolean(defects->deviceHasMultipleLogicalUnits));
        DECLARE_ZERO_INIT_ARRAY(char, totalDefectsValue, MAX_UINT32_TO_DEC_STRING_LENGHT);
        snprintf_err_handle(totalDefectsValue, MAX_UINT32_TO_DEC_STRING_LENGHT, "%" PRIu32 "",
                            defects->numberOfElements);
        json_object_object_add(scsiDefectNode, "Total Defects in list", json_object_new_string(totalDefectsValue));
        if (defects->numberOfElements > UINT32_C(0))
        {
            char* addressDescriptorName =
                M_REINTERPRET_CAST(char*, safe_calloc(MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH, sizeof(char)));
            get_Address_Descriptor_Name(defects->format, &addressDescriptorName);
            json_object_object_add(scsiDefectNode, "Format", json_object_new_string(addressDescriptorName));
            safe_free(&addressDescriptorName);
        }

        // add this node to root node
        json_object_object_add(rootNode, "Defect List Information", scsiDefectNode);
    }

    // Now add defects
    if (defects->numberOfElements > UINT32_C(0))
    {
        // create array for defect entries
        json_object* defectList = json_object_new_array();

        // add individual defect in array
        for (uint32_t iter = UINT32_C(0); iter < defects->numberOfElements; ++iter)
        {
            switch (defects->format)
            {
            case AD_SHORT_BLOCK_FORMAT_ADDRESS_DESCRIPTOR:
            {
                DECLARE_ZERO_INIT_ARRAY(char, addressValue, MAX_UINT32_TO_DEC_STRING_LENGHT);
                snprintf_err_handle(addressValue, MAX_UINT32_TO_DEC_STRING_LENGHT, "%" PRIu32 "",
                                    defects->defect[iter].block.shortBlockAddress);
                json_object_array_add(defectList, json_object_new_string(addressValue));
            }
            break;

            case AD_LONG_BLOCK_FORMAT_ADDRESS_DESCRIPTOR:
            {
                DECLARE_ZERO_INIT_ARRAY(char, addressValue, MAX_UINT64_TO_DEC_STRING_LENGHT);
                snprintf_err_handle(addressValue, MAX_UINT64_TO_DEC_STRING_LENGHT, "%" PRIu64 "",
                                    defects->defect[iter].block.longBlockAddress);
                json_object_array_add(defectList, json_object_new_string(addressValue));
            }
            break;

            case AD_EXTENDED_PHYSICAL_SECTOR_FORMAT_ADDRESS_DESCRIPTOR:
            case AD_PHYSICAL_SECTOR_FORMAT_ADDRESS_DESCRIPTOR:
            {
                // create defect node
                json_object* defectEntry = json_object_new_object();

                DECLARE_ZERO_INIT_ARRAY(char, cylinderValue, MAX_UINT32_TO_DEC_STRING_LENGHT);
                snprintf_err_handle(cylinderValue, MAX_UINT32_TO_DEC_STRING_LENGHT, "%" PRIu32 "",
                                    defects->defect[iter].physical.cylinderNumber);
                json_object_object_add(defectEntry, "Cylinder", json_object_new_string(cylinderValue));

                DECLARE_ZERO_INIT_ARRAY(char, headValue, MAX_UINT8_TO_DEC_STRING_LENGHT);
                snprintf_err_handle(headValue, MAX_UINT8_TO_DEC_STRING_LENGHT, "%" PRIu8 "",
                                    defects->defect[iter].physical.headNumber);
                json_object_object_add(defectEntry, "Head", json_object_new_string(headValue));

                if ((defects->format == AD_PHYSICAL_SECTOR_FORMAT_ADDRESS_DESCRIPTOR &&
                     defects->defect[iter].physical.sectorNumber == UINT32_MAX) ||
                    (defects->format == AD_EXTENDED_PHYSICAL_SECTOR_FORMAT_ADDRESS_DESCRIPTOR &&
                     defects->defect[iter].physical.sectorNumber == MAX_28BIT))
                {
                    json_object_object_add(defectEntry, "Sector", json_object_new_string("Full Track"));
                }
                else
                {
                    DECLARE_ZERO_INIT_ARRAY(char, sectorValue, MAX_UINT32_TO_DEC_STRING_LENGHT);
                    snprintf_err_handle(sectorValue, MAX_UINT32_TO_DEC_STRING_LENGHT, "%" PRIu8 "",
                                        defects->defect[iter].physical.sectorNumber);
                    json_object_object_add(defectEntry, "Sector", json_object_new_string(sectorValue));

                    // check if there are multiple sector with same cylinder and head
                    uint32_t checkRepeatIter = iter + 1;
                    uint32_t sectorLength    = 1;
                    for (; checkRepeatIter < defects->numberOfElements &&
                           defects->defect[checkRepeatIter].physical.cylinderNumber ==
                               defects->defect[iter].physical.cylinderNumber &&
                           defects->defect[checkRepeatIter].physical.headNumber ==
                               defects->defect[iter].physical.headNumber &&
                           defects->defect[checkRepeatIter].physical.sectorNumber ==
                               (defects->defect[iter].physical.sectorNumber + 1);
                         checkRepeatIter++)
                    {
                        sectorLength++;
                        iter++;
                    }

                    // add sector length in node
                    DECLARE_ZERO_INIT_ARRAY(char, sectorLengthValue, MAX_UINT32_TO_DEC_STRING_LENGHT);
                    snprintf_err_handle(sectorLengthValue, MAX_UINT32_TO_DEC_STRING_LENGHT, "%" PRIu32 "",
                                        sectorLength);
                    json_object_object_add(defectEntry, "Sector Length", json_object_new_string(sectorLengthValue));

                    iter = checkRepeatIter - 1;
                }

                // add this entry into list
                json_object_array_add(defectList, defectEntry);
            }
            break;

            case AD_EXTENDED_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR:
            case AD_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR:
            {
                if (defects->defect[iter].bfi.multiAddressDescriptorStart)
                {
                    // create array that will hold individual defect entry for that span
                    json_object* multipleSpanDefectList = json_object_new_array();

                    // add first defect of multiple span in array
                    json_object* firstDefectEntry = json_object_new_object();

                    DECLARE_ZERO_INIT_ARRAY(char, cylinderValue, MAX_UINT32_TO_DEC_STRING_LENGHT);
                    snprintf_err_handle(cylinderValue, MAX_UINT32_TO_DEC_STRING_LENGHT, "%" PRIu32 "",
                                        defects->defect[iter].bfi.cylinderNumber);
                    json_object_object_add(firstDefectEntry, "Cylinder", json_object_new_string(cylinderValue));

                    DECLARE_ZERO_INIT_ARRAY(char, headValue, MAX_UINT8_TO_DEC_STRING_LENGHT);
                    snprintf_err_handle(headValue, MAX_UINT8_TO_DEC_STRING_LENGHT, "%" PRIu8 "",
                                        defects->defect[iter].bfi.headNumber);
                    json_object_object_add(firstDefectEntry, "Head", json_object_new_string(headValue));

                    DECLARE_ZERO_INIT_ARRAY(char, byteIndexValue, MAX_UINT32_TO_DEC_STRING_LENGHT);
                    if ((defects->format == AD_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR &&
                         defects->defect[iter].bfi.bytesFromIndex == UINT32_MAX) ||
                        (defects->format == AD_EXTENDED_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR &&
                         defects->defect[iter].bfi.bytesFromIndex == MAX_28BIT))
                    {
                        snprintf_err_handle(byteIndexValue, MAX_UINT32_TO_DEC_STRING_LENGHT, "%s", "Full Track");
                    }
                    else
                    {
                        snprintf_err_handle(byteIndexValue, MAX_UINT32_TO_DEC_STRING_LENGHT, "%" PRIu32 "",
                                            defects->defect[iter].bfi.bytesFromIndex);
                    }
                    json_object_object_add(firstDefectEntry, "Bytes From Index",
                                           json_object_new_string(byteIndexValue));

                    // add this node into span array node
                    json_object_array_add(multipleSpanDefectList, firstDefectEntry);

                    // now loop through the list, until we reach at the end of the span
                    uint32_t multiBitIter = iter + 1;
                    for (; multiBitIter < defects->numberOfElements &&
                           defects->defect[multiBitIter].bfi.multiAddressDescriptorStart;
                         ++multiBitIter)
                    {
                        // create defect node
                        json_object* subsequentDefectEntry = json_object_new_object();

                        snprintf_err_handle(cylinderValue, MAX_UINT32_TO_DEC_STRING_LENGHT, "%" PRIu32 "",
                                            defects->defect[multiBitIter].bfi.cylinderNumber);
                        json_object_object_add(subsequentDefectEntry, "Cylinder",
                                               json_object_new_string(cylinderValue));

                        snprintf_err_handle(headValue, MAX_UINT8_TO_DEC_STRING_LENGHT, "%" PRIu8 "",
                                            defects->defect[multiBitIter].bfi.headNumber);
                        json_object_object_add(subsequentDefectEntry, "Head", json_object_new_string(headValue));

                        if ((defects->format == AD_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR &&
                             defects->defect[multiBitIter].bfi.bytesFromIndex == UINT32_MAX) ||
                            (defects->format == AD_EXTENDED_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR &&
                             defects->defect[multiBitIter].bfi.bytesFromIndex == MAX_28BIT))
                        {
                            snprintf_err_handle(byteIndexValue, MAX_UINT32_TO_DEC_STRING_LENGHT, "%s", "Full Track");
                        }
                        else
                        {
                            snprintf_err_handle(byteIndexValue, MAX_UINT32_TO_DEC_STRING_LENGHT, "%" PRIu32 "",
                                                defects->defect[multiBitIter].bfi.bytesFromIndex);
                        }
                        json_object_object_add(subsequentDefectEntry, "Bytes From Index",
                                               json_object_new_string(byteIndexValue));

                        // add this node into span array node
                        json_object_array_add(multipleSpanDefectList, subsequentDefectEntry);
                    }

                    // add array in the outer array list
                    json_object_array_add(defectList, multipleSpanDefectList);

                    iter = multiBitIter - 1;
                }
                else
                {
                    // create defect node
                    json_object* defectEntry = json_object_new_object();

                    DECLARE_ZERO_INIT_ARRAY(char, cylinderValue, MAX_UINT32_TO_DEC_STRING_LENGHT);
                    snprintf_err_handle(cylinderValue, MAX_UINT32_TO_DEC_STRING_LENGHT, "%" PRIu32 "",
                                        defects->defect[iter].bfi.cylinderNumber);
                    json_object_object_add(defectEntry, "Cylinder", json_object_new_string(cylinderValue));

                    DECLARE_ZERO_INIT_ARRAY(char, headValue, MAX_UINT8_TO_DEC_STRING_LENGHT);
                    snprintf_err_handle(headValue, MAX_UINT8_TO_DEC_STRING_LENGHT, "%" PRIu8 "",
                                        defects->defect[iter].bfi.headNumber);
                    json_object_object_add(defectEntry, "Head", json_object_new_string(headValue));

                    DECLARE_ZERO_INIT_ARRAY(char, byteIndexValue, MAX_UINT32_TO_DEC_STRING_LENGHT);
                    if ((defects->format == AD_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR &&
                         defects->defect[iter].bfi.bytesFromIndex == UINT32_MAX) ||
                        (defects->format == AD_EXTENDED_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR &&
                         defects->defect[iter].bfi.bytesFromIndex == MAX_28BIT))
                    {
                        snprintf_err_handle(byteIndexValue, MAX_UINT32_TO_DEC_STRING_LENGHT, "%s", "Full Track");
                    }
                    else
                    {
                        snprintf_err_handle(byteIndexValue, MAX_UINT32_TO_DEC_STRING_LENGHT, "%" PRIu32 "",
                                            defects->defect[iter].bfi.bytesFromIndex);
                    }
                    json_object_object_add(defectEntry, "Bytes From Index", json_object_new_string(byteIndexValue));

                    // add this entry into list
                    json_object_array_add(defectList, defectEntry);
                }
            }
            break;

            case AD_VENDOR_SPECIFIC:
            case AD_RESERVED:
            default:
                json_object_array_add(defectList, json_object_new_string("Reserved"));
                break;
            }
        }

        // add array in root node
        json_object_object_add(rootNode, "Defect Addresses", defectList);
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
