// Copyright 2013 Intel Corporation. All rights reserved.
// Copyright 2016-2018 LG Electronics, Inc.
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

#ifndef OZONE_WAYLAND_DISPLAY_POLL_THREAD_H_
#define OZONE_WAYLAND_DISPLAY_POLL_THREAD_H_

#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"

struct wl_display;

namespace ozonewayland {

// This class lets you poll on a given Wayland display (passed in constructor),
// read any pending events coming from Wayland compositor and dispatch them.
// Caller should ensure that StopProcessingEvents is called before display is
// destroyed.
class WaylandDisplayPollThread : public base::Thread {
 public:
  explicit WaylandDisplayPollThread(wl_display* display);
  ~WaylandDisplayPollThread() override;

  // Starts polling on wl_display fd and read/flush requests coming from Wayland
  // compositor.
  void StartProcessingEvents();
  // Stops polling and handling of any events from Wayland compositor.
  void StopProcessingEvents();

 protected:
  void CleanUp() override;

 private:
  static void DisplayRun(WaylandDisplayPollThread* data);
  wl_display* display_;
  base::WaitableEvent polling_;  // Is set as long as the thread is polling.
  base::WaitableEvent stop_polling_;
  DISALLOW_COPY_AND_ASSIGN(WaylandDisplayPollThread);
};

}  // namespace ozonewayland

#endif  // OZONE_WAYLAND_DISPLAY_POLL_THREAD_H_
