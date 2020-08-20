// Copyright 2018 LG Electronics, Inc.
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

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_HEADLESS_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_HEADLESS_H_

#include <vector>

#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Point;
class Rect;
}

namespace views {

class DesktopScreenHeadless : public display::Screen {
 public:
  DesktopScreenHeadless();
  ~DesktopScreenHeadless() override;

 private:
  // Overridden from display::Screen:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  int GetNumDisplays() const override;
  const std::vector<display::Display>& GetAllDisplays() const override;
  display::Display GetDisplayNearestWindow(
    gfx::NativeView window) const override;
  display::Display GetDisplayNearestPoint(
    const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
    const gfx::Rect& match_rect) const override;
  display::Display GetPrimaryDisplay() const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;

  // The fake display object we present to chrome.
  std::vector<display::Display> displays_;
  DISALLOW_COPY_AND_ASSIGN(DesktopScreenHeadless);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_HEADLESS_H_
