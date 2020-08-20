// Copyright 2018 LG Electronics, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_NP_PLUGIN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_NP_PLUGIN_H_

#include "base/logging.h"
#include "neva/npapi/bindings/npapi.h"

#define LWP_LOG_FUNCTION VLOG(2) << "### in " << __FUNCTION__;
#define LWP_LOG_MEMBER_FUNCTION VLOG(2) << "###" << (uint64_t)this << " in "\
        << __FUNCTION__;
#define LWP_LOG_MEMBER_FUNCTION_ENTER VLOG(2) << "### " << __FUNCTION__\
        << " Enter";
#define LWP_LOG_MEMBER_FUNCTION_EXIT VLOG(2) << "### " << __FUNCTION__\
        << " Exit";
#define LWP_LOG_MEMBER_FUNCTION_EXIT_RETURN(str) VLOG(2) << "### "\
        << __FUNCTION__ << " Exit return " << str;
#define LWP_LOG(str) VLOG(2) << "###: " << str;
#define LWP_LOG_MEMBER_ERROR(str) VLOG(0) << "###ERROR("\
        << (uint64_t)this << "): " << str;
#define LWP_LOG_ERROR(str) VLOG(0) << "###ERROR: " << str;

namespace blink {

class PluginStreamUrl;

class NPPlugin {
 public:
  // NPAPI's instance identifier for this instance
  virtual NPP GetNPP() = 0;
  virtual NPError NPP_NewStream(NPMIMEType, NPStream*,
                                NPBool, unsigned short*) = 0;
  virtual NPError NPP_DestroyStream(NPStream*, NPReason) = 0;
  virtual int NPP_WriteReady(NPStream*) = 0;
  virtual int NPP_Write(NPStream*, int, int, void*) = 0;
  virtual void NPP_StreamAsFile(NPStream*, const char*) = 0;
  virtual void NPP_URLNotify(const char*, NPReason, void*) = 0;
  virtual void NPP_URLRedirectNotify(const char* url, int32_t status,
                                     void* notify_data) = 0;
  virtual void RemoveStream(PluginStreamUrl*) = 0;
  virtual void CancelResource(unsigned long) = 0;
  virtual void SetDeferLoading(unsigned long, bool) = 0;
  virtual bool handles_url_redirects() const = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_NP_PLUGIN_H_
