// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_PLUGIN_STREAM_URL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_PLUGIN_STREAM_URL_H_

#include <memory>
#include <vector>

#include "third_party/blink/renderer/core/exported/plugin_stream.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {
class NPPlugin;

// A NPAPI Stream based on a URL.
class PluginStreamUrl : public PluginStream {
 public:
  // Create a new stream for sending to the plugin by fetching
  // a URL. If notifyNeeded is set, then the plugin will be notified
  // when the stream has been fully sent to the plugin.  Initialize
  // must be called before the object is used.
  PluginStreamUrl(unsigned long resource_id,
                  const KURL &url,
                  NPPlugin *instance,
                  bool notify_needed,
                  void *notify_data);

  void URLRedirectResponse(bool allow);

  // Stop sending the stream to the client.
  // Overrides the base Close so we can cancel our fetching the URL if
  // it is still loading.
  bool Close(NPReason reason) override;
  void CancelRequest() override;

  // WebPluginResourceClient methods
  void WillSendRequest(const KURL& url, int http_status_code);
  void DidReceiveResponse(const std::string& mime_type,
                          const std::string& headers,
                          uint32_t expected_length,
                          uint32_t last_modified,
                          bool request_is_seekable);
  void DidReceiveData(const char* buffer, int length, int data_offset);
  void DidFinishLoading(unsigned long resource_id);
  void DidFail(unsigned long resource_id);
  bool IsMultiByteResponseExpected();
  int ResourceId();
  void AddRangeRequestResourceId(unsigned long resource_id);

 protected:
  ~PluginStreamUrl() override;

 private:
  void SetDeferLoading(bool value);

  // In case of a redirect, this can be called to update the url.  But it must
  // be called before Open().
  void UpdateUrl(const char* url);

  KURL url_;
  unsigned long id_;

  // Ids of additional resources requested via range requests issued on
  // seekable streams.
  // This is used when we're loading resources through the renderer, i.e. not
  // using plugin_url_fetcher_.
  std::vector<unsigned long> range_requests_;

  // If the plugin participates in HTTP URL redirect handling then this member
  // holds the url being redirected to while we wait for the plugin to make a
  // decision on whether to allow or deny the redirect.
  std::string pending_redirect_url_;

  DISALLOW_COPY_AND_ASSIGN(PluginStreamUrl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_PLUGIN_STREAM_URL_H_
