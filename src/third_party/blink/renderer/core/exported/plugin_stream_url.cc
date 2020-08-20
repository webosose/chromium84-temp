// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/exported/plugin_stream_url.h"

#include <algorithm>
#include <iostream>

#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "net/http/http_response_headers.h"
#include "third_party/blink/renderer/core/exported/np_plugin.h"

namespace blink {

PluginStreamUrl::PluginStreamUrl(
    unsigned long resource_id,
    const KURL &url,
    NPPlugin *instance,
    bool notify_needed,
    void *notify_data)
    : PluginStream(instance, url.GetString().Utf8().data(), notify_needed, notify_data),
      url_(url),
      id_(resource_id) {
}

void PluginStreamUrl::URLRedirectResponse(bool allow) {
  // Instance()->webplugin()->URLRedirectResponse(allow, id_);

  if (allow)
    UpdateUrl(pending_redirect_url_.c_str());
}

bool PluginStreamUrl::Close(NPReason reason) {
  // Protect the stream against it being destroyed or the whole plugin instance
  // being destroyed within the destroy stream handler.
  scoped_refptr<PluginStream> protect(this);
  CancelRequest();
  bool result = PluginStream::Close(reason);
  Instance()->RemoveStream(this);
  LWP_LOG("Instance()->RemoveStream return");
  return result;
}

void PluginStreamUrl::CancelRequest() {
  if (id_ > 0) {
    Instance()->CancelResource(id_);
    id_ = 0;
  }

  range_requests_.clear();
}

void PluginStreamUrl::WillSendRequest(const KURL& url, int http_status_code) {
  std::string url_string = url.GetString().Utf8().data();
  if (NotifyNeeded()) {
    // If the plugin participates in HTTP url redirect handling then notify it.
    if (net::HttpResponseHeaders::IsRedirectResponseCode(http_status_code) &&
        Instance()->handles_url_redirects()) {
      pending_redirect_url_ = url_string;
      Instance()->NPP_URLRedirectNotify(url_string.c_str(), http_status_code,
                                        NotifyData());
      return;
    }
  }
  url_ = url;
  UpdateUrl(url_string.c_str());
}

void PluginStreamUrl::DidReceiveResponse(const std::string& mime_type,
                                         const std::string& headers,
                                         uint32_t expected_length,
                                         uint32_t last_modified,
                                         bool request_is_seekable) {
  // Protect the stream against it being destroyed or the whole plugin instance
  // being destroyed within the new stream handler.
  //scoped_refptr<PluginStream> protect(this);

  bool did_open = Open(mime_type,
                     headers,
                     expected_length,
                     last_modified,
                     request_is_seekable);
  if (!did_open) {
    CancelRequest();
    Instance()->RemoveStream(this);
  } else {
    SetDeferLoading(false);
  }
}

void PluginStreamUrl::DidReceiveData(const char* buffer, int length,
                                     int data_offset) {
  if (!Open())
    return;

  // Protect the stream against it being destroyed or the whole plugin instance
  // being destroyed within the write handlers
  scoped_refptr<PluginStream> protect(this);

  if (length > 0) {
    // The PluginStreamUrl instance could get deleted if the plugin fails to
    // accept data in NPP_Write.
    if (Write(const_cast<char*>(buffer), length, data_offset) > 0) {
      SetDeferLoading(false);
    }
  }
}

void PluginStreamUrl::DidFinishLoading(unsigned long resource_id) {
  if (!Seekable()) {
    Close(NPRES_DONE);
  } else {
    std::vector<unsigned long>::iterator it_resource = std::find(
        range_requests_.begin(),
        range_requests_.end(),
        resource_id);
    // Resource id must be known to us - either main resource id, or one
    // of the resources, created for range requests.
    DCHECK(resource_id == id_ || it_resource != range_requests_.end());
    // We should notify the plugin about failed/finished requests to ensure
    // that the number of active resource clients does not continue to grow.
    if (it_resource != range_requests_.end())
      range_requests_.erase(it_resource);
  }
}

void PluginStreamUrl::DidFail(unsigned long resource_id) {
  Close(NPRES_NETWORK_ERR);
}

bool PluginStreamUrl::IsMultiByteResponseExpected() {
  return Seekable();
}

int PluginStreamUrl::ResourceId() {
  return id_;
}

PluginStreamUrl::~PluginStreamUrl() {
  LWP_LOG_MEMBER_FUNCTION;
}

void PluginStreamUrl::AddRangeRequestResourceId(unsigned long resource_id) {
  DCHECK_NE(resource_id, 0u);
  range_requests_.push_back(resource_id);
}

void PluginStreamUrl::SetDeferLoading(bool value) {
  // If we determined that the request had failed via the HTTP headers in the
  // response then we send out a failure notification to the plugin process, as
  // certain plugins don't handle HTTP failure codes correctly.
  Instance()->SetDeferLoading(id_, value);
}

void PluginStreamUrl::UpdateUrl(const char* url) {
  DCHECK(!Open());
  free(const_cast<char*>(Stream()->url));
  Stream()->url = base::strdup(url);
  pending_redirect_url_.clear();
}

}  // namespace blink
