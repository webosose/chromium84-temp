// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/builtin_service_manifests.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "content/public/app/content_browser_manifest.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"

#if defined(USE_NEVA_APPRUNTIME)
#include "neva/pal_service/neva_pal_manifest.h"
#endif

#if defined(USE_NEVA_MEDIA)
#include "neva/neva_media_service/neva_media_manifest.h"
#endif

namespace content {

const std::vector<service_manager::Manifest>& GetBuiltinServiceManifests() {
  static base::NoDestructor<std::vector<service_manager::Manifest>> manifests{
    std::vector<service_manager::Manifest>{
#if defined(USE_NEVA_APPRUNTIME)
      pal::GetNevaPalManifest(),
#endif
#if defined(USE_NEVA_MEDIA)
      neva_media::GetNevaMediaManifest(),
#endif
      GetContentBrowserManifest()
    }
  };
  return *manifests;
}

}  // namespace content
