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
// \file jsonformat_common.h
// \brief This file defines common things for the opensea-jsonformat Library.

#pragma once

#include "common_public.h"

#if defined(__cplusplus)
#    define __STDC_FORMAT_MACROS
extern "C"
{
#endif

// This is a bunch of stuff for creating opensea-jsonformat as a dynamic library (DLL in Windows or shared object in
// linux)
#if defined(OPENSEA_JSONFORMAT_API)
#    undef(OPENSEA_JSONFORMAT_API)
#endif

#if defined(EXPORT_OPENSEA_JSONFORMAT) && defined(STATIC_OPENSEA_JSONFORMAT)
#    error "The preprocessor definitions EXPORT_OPENSEA_JSONFORMAT and STATIC_OPENSEA_JSONFORMAT cannot be combined!"
#elif defined(EXPORT_OPENSEA_JSONFORMAT)
#    if defined(_DEBUG) && !defined(OPENSEA_JSONFORMAT_COMPILATION_MESSAGE_OUTPUT)
#        pragma message("Compiling opensea-jsonformat as exporting DLL!")
#        define OPENSEA_JSONFORMAT_COMPILATION_MESSAGE_OUTPUT
#    endif
#    define OPENSEA_JSONFORMAT_API DLL_EXPORT
#elif defined(IMPORT_OPENSEA_JSONFORMAT)
#    if defined(_DEBUG) && !defined(OPENSEA_JSONFORMAT_COMPILATION_MESSAGE_OUTPUT)
#        pragma message("Compiling opensea-jsonformat as importing DLL!")
#        define OPENSEA_JSONFORMAT_COMPILATION_MESSAGE_OUTPUT
#    endif
#    define OPENSEA_JSONFORMAT_API DLL_IMPORT
#else
#    if defined(_DEBUG) && !defined(OPENSEA_JSONFORMAT_COMPILATION_MESSAGE_OUTPUT)
#        pragma message("Compiling opensea-jsonformat as a static library!")
#        define OPENSEA_JSONFORMAT_COMPILATION_MESSAGE_OUTPUT
#    endif
#    define OPENSEA_JSONFORMAT_API
#endif

#if defined(__cplusplus)
}
#endif
