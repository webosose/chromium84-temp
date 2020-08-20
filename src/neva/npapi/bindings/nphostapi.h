// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NEVA_NPAPI_BINDINGS_NPHOSTAPI_H_
#define NEVA_NPAPI_BINDINGS_NPHOSTAPI_H_

#include "build/build_config.h"
#include "neva/npapi/bindings/npapi.h"
#include "neva/npapi/bindings/npapi_extensions.h"
#include "neva/npapi/bindings/npfunctions.h"
#include "neva/npapi/bindings/npruntime.h"

// Define an OS-neutral wrapper for shared library entry points
#define API_CALL

#ifdef __cplusplus
extern "C" {
#endif

//
// NPAPI library entry points
//
#if defined(OS_POSIX)
typedef NPError (API_CALL * NP_InitializeFunc)(NPNetscapeFuncs* pNFuncs,
                                               NPPluginFuncs* pPFuncs);
#else
typedef NPError (API_CALL * NP_InitializeFunc)(NPNetscapeFuncs* pFuncs);
typedef NPError (API_CALL * NP_GetEntryPointsFunc)(NPPluginFuncs* pFuncs);
#endif  // OS_POSIX
typedef NPError (API_CALL * NP_ShutdownFunc)(void);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // NEVA_NPAPI_BINDINGS_NPHOSTAPI_H_
