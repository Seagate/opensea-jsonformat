// SPDX-License-Identifier: MPL-2.0
//
// Do NOT modify or remove this copyright and license
//
// Copyright (c) 2012-2024 Seagate Technology LLC and/or its Affiliates, All Rights Reserved
//
// This software is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// ******************************************************************************************
//
// \file opensea_jsonformat_version.h
// \brief Defines the versioning information for opensea-jsonformat API

#pragma once

#if defined(__cplusplus)
extern "C"
{
#endif

#define COMBINE_JSONFORMAT_VERSIONS_(x, y, z) #x "." #y "." #z
#define COMBINE_JSONFORMAT_VERSIONS(x, y, z)  COMBINE_JSONFORMAT_VERSIONS_(x, y, z)

#define OPENSEA_JSONFORMAT_MAJOR_VERSION      1
#define OPENSEA_JSONFORMAT_MINOR_VERSION      0
#define OPENSEA_JSONFORMAT_PATCH_VERSION      0

#define OPENSEA_JSONFORMAT_VERSION                                                                                     \
    COMBINE_JSONFORMAT_VERSIONS(OPENSEA_JSONFORMAT_MAJOR_VERSION, OPENSEA_JSONFORMAT_MINOR_VERSION,                    \
                                OPENSEA_JSONFORMAT_PATCH_VERSION)

#if defined(__cplusplus)
} // extern "C"
#endif
