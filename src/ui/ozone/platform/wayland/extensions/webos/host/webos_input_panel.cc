// Copyright 2020 LG Electronics, Inc.
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

#include "ui/ozone/platform/wayland/extensions/webos/host/webos_input_panel.h"

#include "ui/ozone/platform/wayland/extensions/webos/host/wayland_webos_extension.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

WebosInputPanel::WebosInputPanel(WaylandConnection* connection,
                                 WaylandWindow* window)
    : connection_(connection), associated_window_(window) {}

WebosInputPanel::~WebosInputPanel() = default;

void WebosInputPanel::HideInputPanel() {
  Deactivate();
}

void WebosInputPanel::SetInputContentType(TextInputType type, int flags) {
  if (webos_text_model_ && webos_text_model_->IsActivated())
    webos_text_model_->SetContentType(type, flags);
}

void WebosInputPanel::SetSurroundingText(const std::string& text,
                                         std::size_t cursor_position,
                                         std::size_t anchor_position) {
  if (webos_text_model_)
    webos_text_model_->SetSurroundingText(text, cursor_position,
                                          anchor_position);
}

void WebosInputPanel::ShowInputPanel() {
  if (!webos_text_model_ && !CreateTextModel())
    return;

  webos_text_model_->IsActivated() ? webos_text_model_->ShowInputPanel()
                                   : webos_text_model_->Activate();
}

bool WebosInputPanel::CreateTextModel() {
  if (webos_text_model_)
    return false;

  if (connection_ && connection_->extension()) {
    WaylandWebosExtension* webos_extension =
        static_cast<WaylandWebosExtension*>(connection_->extension());

    WebosTextModelFactoryWrapper* webos_text_model_factory =
        webos_extension->text_model_factory();
    if (webos_text_model_factory)
      webos_text_model_.reset(
          webos_text_model_factory
              ->CreateTextModel(connection_, associated_window_)
              .release());

    return !!webos_text_model_;
  }

  return false;
}

void WebosInputPanel::Deactivate() {
  if (webos_text_model_ && webos_text_model_->IsActivated()) {
    webos_text_model_->Reset();
    webos_text_model_->Deactivate();
    webos_text_model_.reset();
  }
}

}  // namespace ui
