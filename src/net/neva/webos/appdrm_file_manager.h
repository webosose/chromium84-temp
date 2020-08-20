// Copyright 2019 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef NET_NEVA_WEBOS_APPDRM_FILE_MANAGER_H_
#define NET_NEVA_WEBOS_APPDRM_FILE_MANAGER_H_

#include <memory>

#include "base/files/file_path.h"
#include "net/base/net_export.h"

#include <fcntl.h>
#include <sm_appdrm_openapi.h>

namespace net {

class NET_EXPORT AppDRMFileManager {
 public:
  AppDRMFileManager(const base::FilePath&);
  AppDRMFileManager(const std::string);
  ~AppDRMFileManager();
  bool Check();
  inline bool IsAppDRM() const { return is_appdrm_; }
  bool Open();
  int Read(char*, int);
  inline bool IsOpened() const { return filehandle_appdrm_ != 0; }

 private:
  bool Init();
  void Close();

 private:
  base::FilePath appdrm_file_path_;
  FILE_HNDL_T filehandle_appdrm_ = 0;
  bool is_appdrm_ = false;
  static bool is_initialized_;
};

}  // namespace net

#endif  // NET_NEVA_WEBOS_APPDRM_FILE_MANAGER_H_
