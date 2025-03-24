//
// smart_attribute_json.h
//
// Do NOT modify or remove this copyright and confidentiality notice.
//
// Copyright (c) 2012-2024 Seagate Technology LLC and/or its Affiliates, All Rights Reserved
//
// The code contained herein is CONFIDENTIAL to Seagate Technology LLC
// and may be covered under one or more Non-Disclosure Agreements.
// All or portions are also trade secret.
// Any use, modification, duplication, derivation, distribution or disclosure
// of this code, for any reason, not expressly authorized is prohibited.
// All other rights are expressly reserved by Seagate Technology LLC.
//
// *****************************************************************************

// \file smart_attribute_json.h
// \brief This file defines types and functions related to the JSON-based output for SMART Attributes.

#pragma once

#include "common_public.h"
#include "common_types.h"
#include "jsonformat_common.h"
#include "seagate_operations.h"

#if defined(__cplusplus)
extern "C"
{
#endif

    M_NONNULL_PARAM_LIST(1)
    M_PARAM_WO(2)
    OPENSEA_JSONFORMAT_API eReturnValues create_JSON_Output_For_SMART_Attributes(tDevice* device, char** jsonFormat);

#if defined(__cplusplus)
}
#endif