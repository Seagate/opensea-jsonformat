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
#define MAX_UINT64_TO_DEC_STRING_LENGTH    21
#define MAX_UINT32_TO_DEC_STRING_LENGTH    11
#define MAX_UINT16_TO_DEC_STRING_LENGTH    6
#define MAX_UINT8_TO_DEC_STRING_LENGTH     4

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

static void get_Address_Descriptor_Name(eSCSIAddressDescriptors addressDescriptorType, char** addressDescriptorName)
{
    M_INITIALIZE_STRUCTURE(*addressDescriptorName, MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH);
    switch (addressDescriptorType)
    {
    case AD_SHORT_BLOCK_FORMAT_ADDRESS_DESCRIPTOR:
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(*addressDescriptorName, MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH, "Short Block Format"),
            "fixed descriptor label fits in MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH");
        break;

    case AD_EXTENDED_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR:
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(*addressDescriptorName, MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH,
                                                   "Extended Bytes From Index Format"),
                               "fixed descriptor label fits in MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH");
        break;

    case AD_EXTENDED_PHYSICAL_SECTOR_FORMAT_ADDRESS_DESCRIPTOR:
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(*addressDescriptorName, MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH,
                                                   "Extended Physical Sector Format"),
                               "fixed descriptor label fits in MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH");
        break;

    case AD_LONG_BLOCK_FORMAT_ADDRESS_DESCRIPTOR:
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(*addressDescriptorName, MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH, "Long Block Format"),
            "fixed descriptor label fits in MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH");
        break;

    case AD_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR:
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(*addressDescriptorName, MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH, "Bytes From Index Format"),
            "fixed descriptor label fits in MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH");
        break;

    case AD_PHYSICAL_SECTOR_FORMAT_ADDRESS_DESCRIPTOR:
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(*addressDescriptorName, MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH, "Physical Sector Format"),
            "fixed descriptor label fits in MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH");
        break;

    case AD_VENDOR_SPECIFIC:
    case AD_RESERVED:
    default:
        M_IGNORE_SAFE_INT_CALL(
            snprintf_err_handle(*addressDescriptorName, MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH, "Vendor Specific"),
            "fixed descriptor label fits in MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH");
        break;
    }
}

OPENSEA_JSONFORMAT_API eReturnValues create_JSON_Output_For_SCSI_Defect_List(const tDevice* M_NONNULL    device,
                                                                             ptrSCSIDefectList M_NONNULL defects,
                                                                             const char* M_NONNULL       utilityName,
                                                                             const char* M_NONNULL       buildVersion,
                                                                             char**                      jsonFormat)
{
    if (device == M_NULLPTR || defects == M_NULLPTR || jsonFormat == M_NULLPTR)
    {
        return BAD_PARAMETER;
    }

    *jsonFormat = M_NULLPTR;

    json_object* rootNode = json_object_new_object();
    if (rootNode == M_NULLPTR)
    {
        return MEMORY_FAILURE;
    }

    if (create_Node_For_Utility_Version(rootNode, utilityName, buildVersion, "SCSI Defect List",
                                        SCSI_DEFECT_LIST_JSON_VERSION) != SUCCESS ||
        create_Node_For_Drive_Information(rootNode, device) != SUCCESS)
    {
        json_object_put(rootNode);
        return MEMORY_FAILURE;
    }

    // Add general information about the defect list
    {
        // create node
        json_object* scsiDefectNode = json_object_new_object();
        if (scsiDefectNode == M_NULLPTR)
        {
            json_object_put(rootNode);
            return MEMORY_FAILURE;
        }
        if (add_JSON_Object(rootNode, "Defect List Information", scsiDefectNode) != 0)
        {
            json_object_put(rootNode);
            return MEMORY_FAILURE;
        }

        // add details
        if (add_JSON_Object(scsiDefectNode, "Contains Primary List",
                            json_object_new_boolean(defects->containsPrimaryList)) != 0 ||
            add_JSON_Object(scsiDefectNode, "Contains Grown List",
                            json_object_new_boolean(defects->containsGrownList)) != 0)
        {
            json_object_put(rootNode);
            return MEMORY_FAILURE;
        }
        if (defects->generation > 0)
        {
            DECLARE_ZERO_INIT_ARRAY(char, generationCodeValue, MAX_UINT16_TO_DEC_STRING_LENGTH);
            M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(generationCodeValue, MAX_UINT16_TO_DEC_STRING_LENGTH,
                                                       "%" PRIu16 "", defects->generation),
                                   "maximum uint16_t decimal text fits in MAX_UINT16_TO_DEC_STRING_LENGTH");
            if (add_JSON_Object(scsiDefectNode, "Generation Code", json_object_new_string(generationCodeValue)) != 0)
            {
                json_object_put(rootNode);
                return MEMORY_FAILURE;
            }
        }
        if (add_JSON_Object(scsiDefectNode, "Device Has Multiple Logical Units",
                            json_object_new_boolean(defects->deviceHasMultipleLogicalUnits)) != 0)
        {
            json_object_put(rootNode);
            return MEMORY_FAILURE;
        }
        DECLARE_ZERO_INIT_ARRAY(char, totalDefectsValue, MAX_UINT32_TO_DEC_STRING_LENGTH);
        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(totalDefectsValue, MAX_UINT32_TO_DEC_STRING_LENGTH, "%" PRIu32 "",
                                                   defects->numberOfElements),
                               "maximum uint32_t decimal text fits in MAX_UINT32_TO_DEC_STRING_LENGTH");
        if (add_JSON_Object(scsiDefectNode, "Total Defects in list", json_object_new_string(totalDefectsValue)) != 0)
        {
            json_object_put(rootNode);
            return MEMORY_FAILURE;
        }
        if (defects->numberOfElements > UINT32_C(0))
        {
            char* addressDescriptorName =
                M_REINTERPRET_CAST(char*, safe_calloc(MAX_ADDRESS_DESCRIPTOR_NAME_LENGTH, sizeof(char)));
            if (addressDescriptorName == M_NULLPTR)
            {
                json_object_put(rootNode);
                return MEMORY_FAILURE;
            }
            get_Address_Descriptor_Name(defects->format, &addressDescriptorName);
            if (add_JSON_Object(scsiDefectNode, "Format", json_object_new_string(addressDescriptorName)) != 0)
            {
                safe_free(&addressDescriptorName);
                json_object_put(rootNode);
                return MEMORY_FAILURE;
            }
            safe_free(&addressDescriptorName);
        }
    }

    // Now add defects
    if (defects->numberOfElements > UINT32_C(0))
    {
        // create array for defect entries
        json_object* defectList = json_object_new_array();
        if (defectList == M_NULLPTR)
        {
            json_object_put(rootNode);
            return MEMORY_FAILURE;
        }
        if (add_JSON_Object(rootNode, "Defect Addresses", defectList) != 0)
        {
            json_object_put(rootNode);
            return MEMORY_FAILURE;
        }

        // add individual defect in array
        for (uint32_t iter = UINT32_C(0); iter < defects->numberOfElements; ++iter)
        {
            switch (defects->format)
            {
            case AD_SHORT_BLOCK_FORMAT_ADDRESS_DESCRIPTOR:
            {
                DECLARE_ZERO_INIT_ARRAY(char, addressValue, MAX_UINT32_TO_DEC_STRING_LENGTH);
                M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(addressValue, MAX_UINT32_TO_DEC_STRING_LENGTH, "%" PRIu32 "",
                                                           defects->defect[iter].block.shortBlockAddress),
                                       "fixed numeric defect value fits in its type-sized JSON buffer");
                if (add_JSON_Array_Element(defectList, json_object_new_string(addressValue)) != 0)
                {
                    json_object_put(rootNode);
                    return MEMORY_FAILURE;
                }
            }
            break;

            case AD_LONG_BLOCK_FORMAT_ADDRESS_DESCRIPTOR:
            {
                DECLARE_ZERO_INIT_ARRAY(char, addressValue, MAX_UINT64_TO_DEC_STRING_LENGTH);
                M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(addressValue, MAX_UINT64_TO_DEC_STRING_LENGTH, "%" PRIu64 "",
                                                           defects->defect[iter].block.longBlockAddress),
                                       "fixed numeric defect value fits in its type-sized JSON buffer");
                if (add_JSON_Array_Element(defectList, json_object_new_string(addressValue)) != 0)
                {
                    json_object_put(rootNode);
                    return MEMORY_FAILURE;
                }
            }
            break;

            case AD_EXTENDED_PHYSICAL_SECTOR_FORMAT_ADDRESS_DESCRIPTOR:
            case AD_PHYSICAL_SECTOR_FORMAT_ADDRESS_DESCRIPTOR:
            {
                // create defect node
                json_object* defectEntry = json_object_new_object();
                if (defectEntry == M_NULLPTR)
                {
                    json_object_put(rootNode);
                    return MEMORY_FAILURE;
                }

                DECLARE_ZERO_INIT_ARRAY(char, cylinderValue, MAX_UINT32_TO_DEC_STRING_LENGTH);
                M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(cylinderValue, MAX_UINT32_TO_DEC_STRING_LENGTH,
                                                           "%" PRIu32 "",
                                                           defects->defect[iter].physical.cylinderNumber),
                                       "fixed numeric defect value fits in its type-sized JSON buffer");
                if (add_JSON_Object(defectEntry, "Cylinder", json_object_new_string(cylinderValue)) != 0)
                {
                    json_object_put(defectEntry);
                    json_object_put(rootNode);
                    return MEMORY_FAILURE;
                }

                DECLARE_ZERO_INIT_ARRAY(char, headValue, MAX_UINT8_TO_DEC_STRING_LENGTH);
                M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(headValue, MAX_UINT8_TO_DEC_STRING_LENGTH, "%" PRIu8 "",
                                                           defects->defect[iter].physical.headNumber),
                                       "fixed numeric defect value fits in its type-sized JSON buffer");
                if (add_JSON_Object(defectEntry, "Head", json_object_new_string(headValue)) != 0)
                {
                    json_object_put(defectEntry);
                    json_object_put(rootNode);
                    return MEMORY_FAILURE;
                }

                if ((defects->format == AD_PHYSICAL_SECTOR_FORMAT_ADDRESS_DESCRIPTOR &&
                     defects->defect[iter].physical.sectorNumber == UINT32_MAX) ||
                    (defects->format == AD_EXTENDED_PHYSICAL_SECTOR_FORMAT_ADDRESS_DESCRIPTOR &&
                     defects->defect[iter].physical.sectorNumber == MAX_28BIT))
                {
                    if (add_JSON_Object(defectEntry, "Sector", json_object_new_string("Full Track")) != 0)
                    {
                        json_object_put(defectEntry);
                        json_object_put(rootNode);
                        return MEMORY_FAILURE;
                    }
                }
                else
                {
                    DECLARE_ZERO_INIT_ARRAY(char, sectorValue, MAX_UINT32_TO_DEC_STRING_LENGTH);
                    M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(sectorValue, MAX_UINT32_TO_DEC_STRING_LENGTH,
                                                               "%" PRIu32 "",
                                                               defects->defect[iter].physical.sectorNumber),
                                           "fixed numeric defect value fits in its type-sized JSON buffer");
                    if (add_JSON_Object(defectEntry, "Sector", json_object_new_string(sectorValue)) != 0)
                    {
                        json_object_put(defectEntry);
                        json_object_put(rootNode);
                        return MEMORY_FAILURE;
                    }

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
                    DECLARE_ZERO_INIT_ARRAY(char, sectorLengthValue, MAX_UINT32_TO_DEC_STRING_LENGTH);
                    M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(sectorLengthValue, MAX_UINT32_TO_DEC_STRING_LENGTH,
                                                               "%" PRIu32 "", sectorLength),
                                           "fixed numeric defect value fits in its type-sized JSON buffer");
                    if (add_JSON_Object(defectEntry, "Sector Length", json_object_new_string(sectorLengthValue)) != 0)
                    {
                        json_object_put(defectEntry);
                        json_object_put(rootNode);
                        return MEMORY_FAILURE;
                    }

                    iter = checkRepeatIter - 1;
                }

                // add this entry into list
                if (add_JSON_Array_Element(defectList, defectEntry) != 0)
                {
                    json_object_put(rootNode);
                    return MEMORY_FAILURE;
                }
            }
            break;

            case AD_EXTENDED_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR:
            case AD_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR:
            {
                if (defects->defect[iter].bfi.multiAddressDescriptorStart)
                {
                    // create array that will hold individual defect entry for that span
                    json_object* multipleSpanDefectList = json_object_new_array();
                    if (multipleSpanDefectList == M_NULLPTR)
                    {
                        json_object_put(rootNode);
                        return MEMORY_FAILURE;
                    }
                    if (add_JSON_Array_Element(defectList, multipleSpanDefectList) != 0)
                    {
                        json_object_put(rootNode);
                        return MEMORY_FAILURE;
                    }

                    // add first defect of multiple span in array
                    json_object* firstDefectEntry = json_object_new_object();
                    if (firstDefectEntry == M_NULLPTR)
                    {
                        json_object_put(rootNode);
                        return MEMORY_FAILURE;
                    }

                    DECLARE_ZERO_INIT_ARRAY(char, cylinderValue, MAX_UINT32_TO_DEC_STRING_LENGTH);
                    M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(cylinderValue, MAX_UINT32_TO_DEC_STRING_LENGTH,
                                                               "%" PRIu32 "", defects->defect[iter].bfi.cylinderNumber),
                                           "fixed numeric defect value fits in its type-sized JSON buffer");
                    if (add_JSON_Object(firstDefectEntry, "Cylinder", json_object_new_string(cylinderValue)) != 0)
                    {
                        json_object_put(firstDefectEntry);
                        json_object_put(rootNode);
                        return MEMORY_FAILURE;
                    }

                    DECLARE_ZERO_INIT_ARRAY(char, headValue, MAX_UINT8_TO_DEC_STRING_LENGTH);
                    M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(headValue, MAX_UINT8_TO_DEC_STRING_LENGTH, "%" PRIu8 "",
                                                               defects->defect[iter].bfi.headNumber),
                                           "fixed numeric defect value fits in its type-sized JSON buffer");
                    if (add_JSON_Object(firstDefectEntry, "Head", json_object_new_string(headValue)) != 0)
                    {
                        json_object_put(firstDefectEntry);
                        json_object_put(rootNode);
                        return MEMORY_FAILURE;
                    }

                    DECLARE_ZERO_INIT_ARRAY(char, byteIndexValue, MAX_UINT32_TO_DEC_STRING_LENGTH);
                    if ((defects->format == AD_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR &&
                         defects->defect[iter].bfi.bytesFromIndex == UINT32_MAX) ||
                        (defects->format == AD_EXTENDED_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR &&
                         defects->defect[iter].bfi.bytesFromIndex == MAX_28BIT))
                    {
                        M_IGNORE_SAFE_ERRNO_CALL(
                            safe_strcpy(byteIndexValue, MAX_UINT32_TO_DEC_STRING_LENGTH, "Full Track"),
                            "fixed Full Track label fits in the decimal value buffer");
                    }
                    else
                    {
                        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(byteIndexValue, MAX_UINT32_TO_DEC_STRING_LENGTH,
                                                                   "%" PRIu32 "",
                                                                   defects->defect[iter].bfi.bytesFromIndex),
                                               "fixed numeric defect value fits in its type-sized JSON buffer");
                    }
                    if (add_JSON_Object(firstDefectEntry, "Bytes From Index", json_object_new_string(byteIndexValue)) !=
                        0)
                    {
                        json_object_put(firstDefectEntry);
                        json_object_put(rootNode);
                        return MEMORY_FAILURE;
                    }

                    // add this node into span array node
                    if (add_JSON_Array_Element(multipleSpanDefectList, firstDefectEntry) != 0)
                    {
                        json_object_put(rootNode);
                        return MEMORY_FAILURE;
                    }

                    // now loop through the list, until we reach at the end of the span
                    uint32_t multiBitIter = iter + 1;
                    for (; multiBitIter < defects->numberOfElements &&
                           defects->defect[multiBitIter].bfi.multiAddressDescriptorStart;
                         ++multiBitIter)
                    {
                        // create defect node
                        json_object* subsequentDefectEntry = json_object_new_object();
                        if (subsequentDefectEntry == M_NULLPTR)
                        {
                            json_object_put(rootNode);
                            return MEMORY_FAILURE;
                        }

                        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(cylinderValue, MAX_UINT32_TO_DEC_STRING_LENGTH,
                                                                   "%" PRIu32 "",
                                                                   defects->defect[multiBitIter].bfi.cylinderNumber),
                                               "fixed numeric defect value fits in its type-sized JSON buffer");
                        if (add_JSON_Object(subsequentDefectEntry, "Cylinder", json_object_new_string(cylinderValue)) !=
                            0)
                        {
                            json_object_put(subsequentDefectEntry);
                            json_object_put(rootNode);
                            return MEMORY_FAILURE;
                        }

                        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(headValue, MAX_UINT8_TO_DEC_STRING_LENGTH,
                                                                   "%" PRIu8 "",
                                                                   defects->defect[multiBitIter].bfi.headNumber),
                                               "fixed numeric defect value fits in its type-sized JSON buffer");
                        if (add_JSON_Object(subsequentDefectEntry, "Head", json_object_new_string(headValue)) != 0)
                        {
                            json_object_put(subsequentDefectEntry);
                            json_object_put(rootNode);
                            return MEMORY_FAILURE;
                        }

                        if ((defects->format == AD_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR &&
                             defects->defect[multiBitIter].bfi.bytesFromIndex == UINT32_MAX) ||
                            (defects->format == AD_EXTENDED_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR &&
                             defects->defect[multiBitIter].bfi.bytesFromIndex == MAX_28BIT))
                        {
                            M_IGNORE_SAFE_ERRNO_CALL(
                                safe_strcpy(byteIndexValue, MAX_UINT32_TO_DEC_STRING_LENGTH, "Full Track"),
                                "fixed Full Track label fits in the decimal value buffer");
                        }
                        else
                        {
                            M_IGNORE_SAFE_INT_CALL(
                                snprintf_err_handle(byteIndexValue, MAX_UINT32_TO_DEC_STRING_LENGTH, "%" PRIu32 "",
                                                    defects->defect[multiBitIter].bfi.bytesFromIndex),
                                "fixed numeric defect value fits in its type-sized JSON buffer");
                        }
                        if (add_JSON_Object(subsequentDefectEntry, "Bytes From Index",
                                            json_object_new_string(byteIndexValue)) != 0)
                        {
                            json_object_put(subsequentDefectEntry);
                            json_object_put(rootNode);
                            return MEMORY_FAILURE;
                        }

                        // add this node into span array node
                        if (add_JSON_Array_Element(multipleSpanDefectList, subsequentDefectEntry) != 0)
                        {
                            json_object_put(rootNode);
                            return MEMORY_FAILURE;
                        }
                    }

                    iter = multiBitIter - 1;
                }
                else
                {
                    // create defect node
                    json_object* defectEntry = json_object_new_object();
                    if (defectEntry == M_NULLPTR)
                    {
                        json_object_put(rootNode);
                        return MEMORY_FAILURE;
                    }

                    DECLARE_ZERO_INIT_ARRAY(char, cylinderValue, MAX_UINT32_TO_DEC_STRING_LENGTH);
                    M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(cylinderValue, MAX_UINT32_TO_DEC_STRING_LENGTH,
                                                               "%" PRIu32 "", defects->defect[iter].bfi.cylinderNumber),
                                           "fixed numeric defect value fits in its type-sized JSON buffer");
                    if (add_JSON_Object(defectEntry, "Cylinder", json_object_new_string(cylinderValue)) != 0)
                    {
                        json_object_put(defectEntry);
                        json_object_put(rootNode);
                        return MEMORY_FAILURE;
                    }

                    DECLARE_ZERO_INIT_ARRAY(char, headValue, MAX_UINT8_TO_DEC_STRING_LENGTH);
                    M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(headValue, MAX_UINT8_TO_DEC_STRING_LENGTH, "%" PRIu8 "",
                                                               defects->defect[iter].bfi.headNumber),
                                           "fixed numeric defect value fits in its type-sized JSON buffer");
                    if (add_JSON_Object(defectEntry, "Head", json_object_new_string(headValue)) != 0)
                    {
                        json_object_put(defectEntry);
                        json_object_put(rootNode);
                        return MEMORY_FAILURE;
                    }

                    DECLARE_ZERO_INIT_ARRAY(char, byteIndexValue, MAX_UINT32_TO_DEC_STRING_LENGTH);
                    if ((defects->format == AD_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR &&
                         defects->defect[iter].bfi.bytesFromIndex == UINT32_MAX) ||
                        (defects->format == AD_EXTENDED_BYTES_FROM_INDEX_FORMAT_ADDRESS_DESCRIPTOR &&
                         defects->defect[iter].bfi.bytesFromIndex == MAX_28BIT))
                    {
                        M_IGNORE_SAFE_ERRNO_CALL(
                            safe_strcpy(byteIndexValue, MAX_UINT32_TO_DEC_STRING_LENGTH, "Full Track"),
                            "fixed Full Track label fits in the decimal value buffer");
                    }
                    else
                    {
                        M_IGNORE_SAFE_INT_CALL(snprintf_err_handle(byteIndexValue, MAX_UINT32_TO_DEC_STRING_LENGTH,
                                                                   "%" PRIu32 "",
                                                                   defects->defect[iter].bfi.bytesFromIndex),
                                               "fixed numeric defect value fits in its type-sized JSON buffer");
                    }
                    if (add_JSON_Object(defectEntry, "Bytes From Index", json_object_new_string(byteIndexValue)) != 0)
                    {
                        json_object_put(defectEntry);
                        json_object_put(rootNode);
                        return MEMORY_FAILURE;
                    }

                    // add this entry into list
                    if (add_JSON_Array_Element(defectList, defectEntry) != 0)
                    {
                        json_object_put(rootNode);
                        return MEMORY_FAILURE;
                    }
                }
            }
            break;

            case AD_VENDOR_SPECIFIC:
            case AD_RESERVED:
            default:
                if (add_JSON_Array_Element(defectList, json_object_new_string("Reserved")) != 0)
                {
                    json_object_put(rootNode);
                    return MEMORY_FAILURE;
                }
                break;
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
