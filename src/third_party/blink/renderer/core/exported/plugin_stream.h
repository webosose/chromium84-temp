// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_PLUGIN_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_PLUGIN_STREAM_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "neva/npapi/bindings/npapi.h"

namespace blink {

class NPPlugin;

// Base class for a NPAPI stream.  Tracks basic elements
// of a stream for NPAPI notifications and stream position.
class PluginStream : public base::RefCounted<PluginStream> {
 public:
  // Create a new PluginStream object.  If needNotify is true, then the
  // plugin will be notified when the stream has been fully sent.
  PluginStream(NPPlugin* instance,
               const char* url,
               bool need_notify,
               void* notify_data);

  // Opens the stream to the Plugin.
  // If the mime-type is not specified, we'll try to find one based on the
  // mime-types table and the extension (if any) in the URL.
  // If the size of the stream is known, use length to set the size.  If
  // not known, set length to 0.
  // The request_is_seekable parameter indicates whether byte range requests
  // can be issued on the stream.
  bool Open(const std::string &mime_type,
            const std::string &headers,
            uint32_t length,
            uint32_t last_modified,
            bool request_is_seekable);

  // Writes to the stream.
  int Write(const char* buf, const int len, int data_offset);

  // Write the result as a file.
  void WriteAsFile();

  // Notify the plugin that a stream is complete.
  void Notify(NPReason reason);

  // Close the stream.
  virtual bool Close(NPReason reason);

  // Cancels any HTTP requests initiated by the stream.
  virtual void CancelRequest() {}

  NPStream* Stream() { return &stream_; }

  NPPlugin* Instance() { return instance_; }

  // setter/getter for the seekable attribute on the stream.
  bool Seekable() const { return seekable_stream_; }

  void SetSeekable(bool seekable) { seekable_stream_ = seekable; }

  // getters for reading the notification related attributes on the stream.
  bool NotifyNeeded() const { return notify_needed_; }

  void* NotifyData() const { return notify_data_; }

 protected:
  friend class base::RefCounted<PluginStream>;

  virtual ~PluginStream();

  // Check if the stream is open.
  bool Open() { return opened_; }

 private:
  // Per platform method to reset the temporary file handle.
  void ResetTempFileHandle();

  // Per platform method to reset the temporary file name.
  void ResetTempFileName();

  // Open a temporary file for this stream.
  // If successful, will set temp_file_name_, temp_file_handle_, and
  // return true.
  bool OpenTempFile();

  // Closes the temporary file if it is open.
  void CloseTempFile();

  // Sends the data to the file. Called From WriteToFile.
  size_t WriteBytes(const char* buf, size_t length);

  // Sends the data to the file if it's open.
  bool WriteToFile(const char* buf, size_t length);

  // Sends the data to the plugin.  If it's not ready, handles buffering it
  // and retrying later.
  bool WriteToPlugin(const char* buf, const int length, const int data_offset);

  // Send the data to the plugin, returning how many bytes it accepted, or -1
  // if an error occurred.
  int TryWriteToPlugin(const char* buf, const int length,
                       const int data_offset);

  // The callback which calls TryWriteToPlugin.
  void OnDelayDelivery();

  // Returns true if the temp file is valid and open for writing.
  bool TempFileIsValid() const;

  // Returns true if |requested_plugin_mode_| is NP_ASFILE or NP_ASFILEONLY.
  bool RequestedPluginModeIsAsFile() const;

 private:
  NPStream                      stream_;
  std::string                   headers_;
//  scoped_refptr<NPPlugin>       instance_;
  NPPlugin*                     instance_;
  bool                          notify_needed_;
  void*                         notify_data_;
  bool                          close_on_write_data_;
  uint16_t                      requested_plugin_mode_;
  bool                          opened_;
#if defined(OS_WIN)
  char                          temp_file_name_[MAX_PATH];
  HANDLE                        temp_file_handle_;
#elif defined(OS_POSIX)
  FILE*                         temp_file_;
  base::FilePath                temp_file_path_;
#endif
  std::vector<char>             delivery_data_;
  int                           data_offset_;
  bool                          seekable_stream_;
  std::string                   mime_type_;
  DISALLOW_COPY_AND_ASSIGN(PluginStream);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_PLUGIN_STREAM_H_
