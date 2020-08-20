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

#include "ui/views/widget/desktop_aura/desktop_screen_headless.h"

#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace views {
namespace {

const int64_t kHeadlessDisplayId = 0;
const float kHeadlessScaleFactor = 1.0f;
const int kHeadlessDisplayWidth  = 1920;
const int kHeadlessDisplayHeight = 1080;

}  // namespace

DesktopScreenHeadless::DesktopScreenHeadless() {
  display::Display display(kHeadlessDisplayId);
  display.SetScaleAndBounds(kHeadlessScaleFactor,
                            gfx::Rect(gfx::Size(kHeadlessDisplayWidth,
                                                kHeadlessDisplayHeight)));
  displays_.push_back(display);
}

DesktopScreenHeadless::~DesktopScreenHeadless() {
}

gfx::Point DesktopScreenHeadless::GetCursorScreenPoint() {
  NOTIMPLEMENTED();
  return gfx::Point();
}

bool DesktopScreenHeadless::IsWindowUnderCursor(gfx::NativeWindow window) {
  NOTIMPLEMENTED();
  return false;
}

gfx::NativeWindow DesktopScreenHeadless::GetWindowAtScreenPoint(
    const gfx::Point& point) {
  NOTIMPLEMENTED();
  return nullptr;
}

int DesktopScreenHeadless::GetNumDisplays() const {
  return displays_.size();
}

const std::vector<display::Display>&
DesktopScreenHeadless::GetAllDisplays() const {
  return displays_;
}

display::Display DesktopScreenHeadless::GetDisplayNearestWindow(
    gfx::NativeView window) const {
  NOTIMPLEMENTED();
  return GetPrimaryDisplay();
}

display::Display DesktopScreenHeadless::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  NOTIMPLEMENTED();
  return GetPrimaryDisplay();
}

display::Display DesktopScreenHeadless::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  NOTIMPLEMENTED();
  return GetPrimaryDisplay();
}

display::Display DesktopScreenHeadless::GetPrimaryDisplay() const {
  return displays_.front();
}

void DesktopScreenHeadless::AddObserver(display::DisplayObserver* observer) {
  NOTIMPLEMENTED();
}

void DesktopScreenHeadless::RemoveObserver(display::DisplayObserver* observer) {
  NOTIMPLEMENTED();
}

}  // namespace views
