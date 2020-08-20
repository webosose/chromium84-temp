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

#include "net/neva/webos/appdrm_file_manager.h"

#include "base/location.h"
#include "base/logging.h"
#include "mojo/public/cpp/system/file_data_source.h"
#include "net/base/filename_util.h"
#include "net/neva/appdrm_common.h"
#include "url/gurl.h"

namespace net {

bool AppDRMFileManager::is_initialized_ = false;

AppDRMFileManager::AppDRMFileManager(const base::FilePath& file_path)
    : appdrm_file_path_(file_path) {}

AppDRMFileManager::AppDRMFileManager(const std::string url_str) {
  FileURLToFilePath(GURL(url_str), &appdrm_file_path_);
}

AppDRMFileManager::~AppDRMFileManager() {
  Close();
}

bool AppDRMFileManager::Check() {
  // APPDRM_RET_T Info
  // APPDRM_OK = 0, APPDRM_ERR = -1, APPDRM_NOT_OK = -1,
  // APPDRM_NOT_NCG = -2, APPDRM_INVALID_PARAMS = -3,
  // APPDRM_GETCEK_FAILURE = -4, APPDRM_OPEN_ERR = -5
  if (!Init())
    return false;

  APPDRM_RET_T result =
      SM_APPDRM_CheckNcgDrm(appdrm_file_path_.value().c_str());
  switch (result) {
    case APPDRM_OK:
      LOG(INFO) << "AppDRM: It's a encrypted file "
                << appdrm_file_path_.value().c_str();
      is_appdrm_ = true;
      return true;
    case APPDRM_NOT_NCG:
      LOG(INFO) << "AppDRM: It's not a encrypted file "
                << appdrm_file_path_.value().c_str();
      break;
    default:
      LOG(ERROR) << "AppDRM: CheckNcgDrm for file "
                 << appdrm_file_path_.value().c_str() << ", result: " << result;
      break;
  }

  return false;
}

void AppDRMFileManager::Close() {
  if (is_appdrm_ && !IsOpened()) {
    LOG(WARNING) << "AppDRM: Close, Skipped, Not opened file "
                 << appdrm_file_path_.value().c_str();
    return;
  }

  APPDRM_RET_T result;
  if ((result = SM_APPDRM_CloseDrmFile(&filehandle_appdrm_)) != APPDRM_OK)
    LOG(ERROR) << "AppDRM: Close, Failed to close file "
               << appdrm_file_path_.value().c_str() << ", result: " << result;

  is_appdrm_ = false;
}

bool AppDRMFileManager::Init() {
  if (is_initialized_)
    return true;

  APPDRM_RET_T result;
  if ((result = SM_APPDRM_Initialize()) == APPDRM_OK) {
    is_initialized_ = true;

    class Final {
     public:
      ~Final() {
        APPDRM_RET_T result;
        if ((result = SM_APPDRM_Finalize()) != APPDRM_OK) {
          LOG(ERROR) << "AppDRM: Init, Failed to finalize "
                     << ", result: " << result;
        } else
          is_initialized_ = false;
      }
    };
    static Final final;

    return true;
  }

  LOG(ERROR) << "AppDRM: Init, Failed to initialize for file "
             << appdrm_file_path_.value().c_str() << ", result: " << result;
  return false;
}

bool AppDRMFileManager::Open() {
  if (IsOpened()) {
    LOG(WARNING) << "AppDRM: Open, Already opened file "
                 << appdrm_file_path_.value().c_str();
    return false;
  }

  const int nFlags = O_RDONLY;
  const int nMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
  APPDRM_RET_T result;
  if ((result = SM_APPDRM_OpenDrmFile(&filehandle_appdrm_,
                                      appdrm_file_path_.value().c_str(), nFlags,
                                      nMode)) == APPDRM_OK)
    return true;
  LOG(ERROR) << "AppDRM: Open, OpenDrmFile failed for file "
             << appdrm_file_path_.value().c_str() << ", result: " << result;
  return false;
}

int AppDRMFileManager::Read(char* buffer, int count) {
  if (!IsOpened()) {
    LOG(WARNING) << "AppDRM: Read, Not opened file "
                 << appdrm_file_path_.value().c_str();
    return base::File::Error::FILE_ERROR_FAILED;
  }

  unsigned long bytes;
  APPDRM_RET_T result;
  if ((result =
           SM_APPDRM_ReadDrmFile(filehandle_appdrm_, (unsigned char*)buffer,
                                 (unsigned long)count, &bytes)) == APPDRM_OK) {
    return static_cast<int>(bytes);
  }
  LOG(ERROR) << "AppDRM: Read, ReadDrmFile failed for file "
             << appdrm_file_path_.value().c_str() << ", result: " << result;
  Close();
  return base::File::Error::FILE_ERROR_FAILED;
}

AppDRMFileDataSource::AppDRMFileDataSource(base::File file,
                                           const base::FilePath& path)
    : appdrm_(new AppDRMFileManager(path)) {
  if (!appdrm_->Check())
    file_data_source_ = std::make_unique<mojo::FileDataSource>(std::move(file));
  else
    appdrm_->Open();
}

AppDRMFileDataSource::~AppDRMFileDataSource() {
  delete appdrm_;
}

void AppDRMFileDataSource::SetRange(uint64_t start, uint64_t end) {
  if (!appdrm_->IsAppDRM())
    return static_cast<mojo::FileDataSource*>(file_data_source_.get())
        ->SetRange(start, end);
}

uint64_t AppDRMFileDataSource::GetLength() const {
  return std::numeric_limits<size_t>::max();
}

mojo::DataPipeProducer::DataSource::ReadResult AppDRMFileDataSource::Read(
    uint64_t offset,
    base::span<char> buffer) {
  if (!appdrm_->IsAppDRM())
    return file_data_source_->Read(offset, std::move(buffer));

  ReadResult result{0, base::File::FILE_ERROR_FAILED};
  if (!appdrm_->IsOpened())
    return result;

  // Note that as file in encrypted and there is no possibility to map offset
  // in decrypted bytes to offset in encrypted bytes to use SM_APPDRM_LSeek,
  // now only consequent file reading is supported.
  int bytes_read = appdrm_->Read(buffer.data(), buffer.size());
  if (bytes_read >= 0) {
    result.bytes_read = bytes_read;
    result.result = MOJO_RESULT_OK;
  }
  return result;
}

AppDRMFileManagerWrapper::AppDRMFileManagerWrapper(const std::string url_str)
    : url_(url_str) {}

AppDRMFileManagerWrapper::~AppDRMFileManagerWrapper() {}

bool AppDRMFileManagerWrapper::Check() {
  AppDRMFileManager adfm = AppDRMFileManager(url_);
  return adfm.Check();
}

const std::string AppDRMFileManagerWrapper::Decrypt() {
  AppDRMFileManager adfm = AppDRMFileManager(url_);
  if (!adfm.Open())
    return std::string();

  int bytesRead;
  const int LOCAL_READ_BUFFER_SIZE = 4096;
  char tempBuffer[LOCAL_READ_BUFFER_SIZE] = {0};
  std::string contents;
  do {
    bytesRead = adfm.Read(tempBuffer, LOCAL_READ_BUFFER_SIZE - 1);
    if (bytesRead < 0)
      return std::string();

    if (bytesRead) {
      tempBuffer[bytesRead] = 0;
      contents.append(tempBuffer);
    }
  } while (bytesRead);

  return contents;
}

}  // namespace net
