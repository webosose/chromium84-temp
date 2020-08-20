// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/gpu_platform_support_host.h"

#include "base/trace_event/trace_event.h"

namespace ui {

namespace {

// No-op implementations of GpuPlatformSupportHost.
class StubGpuPlatformSupportHost : public GpuPlatformSupportHost {
 public:
  // GpuPlatformSupportHost:

#if defined(USE_OZONE) && defined(OZONE_PLATFORM_WAYLAND_EXTERNAL)
  void OnGpuProcessLaunched(
      int host_id,
      scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
      scoped_refptr<base::SingleThreadTaskRunner> send_runner,
      base::RepeatingCallback<void(IPC::Message*)> send_callback) override {}
  void OnMessageReceived(const IPC::Message&) override {}
#endif  // defined(USE_OZONE) && defined(OZONE_PLATFORM_WAYLAND_EXTERNAL)

  void OnChannelDestroyed(int host_id) override {}
  void OnGpuServiceLaunched(
      int host_id,
      scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_runner,
      GpuHostBindInterfaceCallback binder,
      GpuHostTerminateCallback terminate_callback) override {}
};

}  // namespace

GpuPlatformSupportHost::GpuPlatformSupportHost() {
}

GpuPlatformSupportHost::~GpuPlatformSupportHost() {
}

GpuPlatformSupportHost* CreateStubGpuPlatformSupportHost() {
  return new StubGpuPlatformSupportHost;
}

}  // namespace ui
