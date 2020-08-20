// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_screen_ozone.h"

#include "ui/aura/screen_ozone.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

#if defined(USE_OZONE) && defined(OZONE_PLATFORM_WAYLAND_EXTERNAL)
#include "base/command_line.h"
#include "ui/gfx/switches.h"
#include "ui/views/widget/desktop_aura/desktop_screen_headless.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/desktop_aura/desktop_factory_ozone.h"
#endif

namespace views {

DesktopScreenOzone::DesktopScreenOzone() = default;

DesktopScreenOzone::~DesktopScreenOzone() = default;

gfx::NativeWindow DesktopScreenOzone::GetNativeWindowFromAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
#if defined(USE_OZONE) && defined(OZONE_PLATFORM_WAYLAND_EXTERNAL)
  return nullptr;
#else
  if (!widget)
    return nullptr;
  return views::DesktopWindowTreeHostPlatform::GetContentWindowForWidget(
      widget);
#endif  // defined(USE_OZONE) && defined(OZONE_PLATFORM_WAYLAND_EXTERNAL)
}

#if defined(USE_OZONE) && defined(OZONE_PLATFORM_WAYLAND_EXTERNAL)
display::Screen* CreateDesktopScreen() {
  return DesktopFactoryOzone::GetInstance()->CreateDesktopScreen();
}
#else
display::Screen* CreateDesktopScreen() {
  return new DesktopScreenOzone();
}
#endif  // defined(USE_OZONE) && defined(OZONE_PLATFORM_WAYLAND_EXTERNAL)

}  // namespace views
