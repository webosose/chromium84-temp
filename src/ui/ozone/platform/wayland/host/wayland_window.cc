// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_window.h"

#include <wayland-client.h>

#include <memory>

#include "base/bind.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_position.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"

///@name USE_NEVA_APPRUNTIME
///@{
#include "ui/ozone/platform/wayland/host/extended_input_wrapper.h"
#include "ui/ozone/platform/wayland/host/input_panel.h"
#include "ui/ozone/platform/wayland/host/input_panel_manager.h"
#include "ui/ozone/platform/wayland/host/surface_group_compositor_wrapper.h"
#include "ui/ozone/platform/wayland/host/surface_group_wrapper.h"
#include "ui/ozone/platform/wayland/host/wayland_extension.h"
#include "ui/ozone/platform/wayland/host/wayland_input_method_context.h"
///@}

namespace ui {

WaylandWindow::WaylandWindow(PlatformWindowDelegate* delegate,
                             WaylandConnection* connection)
    : delegate_(delegate), connection_(connection) {}

WaylandWindow::~WaylandWindow() {
  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  if (surface_)
    connection_->wayland_window_manager()->RemoveWindow(GetWidget());

  if (parent_window_)
    parent_window_->set_child_window(nullptr);
}

// static
WaylandWindow* WaylandWindow::FromSurface(wl_surface* surface) {
  if (!surface)
    return nullptr;
  return static_cast<WaylandWindow*>(
      wl_proxy_get_user_data(reinterpret_cast<wl_proxy*>(surface)));
}

void WaylandWindow::OnWindowLostCapture() {
  delegate_->OnLostCapture();
}

void WaylandWindow::UpdateBufferScale(bool update_bounds) {
  DCHECK(connection_->wayland_output_manager());
  const auto* screen = connection_->wayland_output_manager()->wayland_screen();

  // The client might not create screen at all.
  if (!screen)
    return;

  const auto widget = GetWidget();

  int32_t new_scale = 0;
  if (parent_window_) {
    new_scale = parent_window_->buffer_scale_;
    ui_scale_ = parent_window_->ui_scale_;
  } else {
    const auto display = (widget == gfx::kNullAcceleratedWidget)
                             ? screen->GetPrimaryDisplay()
                             : screen->GetDisplayForAcceleratedWidget(widget);
    new_scale = connection_->wayland_output_manager()
                    ->GetOutput(display.id())
                    ->scale_factor();

    if (display::Display::HasForceDeviceScaleFactor())
      ui_scale_ = display::Display::GetForcedDeviceScaleFactor();
    else
      ui_scale_ = display.device_scale_factor();
  }
  SetBufferScale(new_scale, update_bounds);
}

gfx::AcceleratedWidget WaylandWindow::GetWidget() const {
  if (!surface_)
    return gfx::kNullAcceleratedWidget;
  return surface_.id();
}
void WaylandWindow::SetPointerFocus(bool focus) {
  has_pointer_focus_ = focus;

  // Whenever the window gets the pointer focus back, we must reinitialize the
  // cursor. Otherwise, it is invalidated whenever the pointer leaves the
  // surface and is not restored by the Wayland compositor.
  if (has_pointer_focus_ && bitmap_)
    connection_->SetCursorBitmap(bitmap_->bitmaps(), bitmap_->hotspot());
}

void WaylandWindow::Show(bool inactive) {
  NOTREACHED();
}

void WaylandWindow::Hide() {
  NOTREACHED();
}

void WaylandWindow::Close() {
  delegate_->OnClosed();
}

bool WaylandWindow::IsVisible() const {
  NOTREACHED();
  return false;
}

void WaylandWindow::PrepareForShutdown() {}

void WaylandWindow::SetBounds(const gfx::Rect& bounds_px) {
  if (bounds_px_ == bounds_px)
    return;
  bounds_px_ = bounds_px;

  // Opaque region is based on the size of the window. Thus, update the region
  // on each update.
  MaybeUpdateOpaqueRegion();

  delegate_->OnBoundsChanged(bounds_px_);
}

gfx::Rect WaylandWindow::GetBounds() {
  return bounds_px_;
}

void WaylandWindow::SetTitle(const base::string16& title) {}

void WaylandWindow::SetCapture() {
  // Wayland doesn't allow explicit grabs. Instead, it sends events to "entered"
  // windows. That is, if user enters their mouse pointer to a window, that
  // window starts to receive events. However, Chromium may want to reroute
  // these events to another window. In this case, tell the window manager that
  // this specific window has grabbed the events, and they will be rerouted in
  // WaylandWindow::DispatchEvent method.
  if (!HasCapture())
    connection_->wayland_window_manager()->GrabLocatedEvents(this);
}

void WaylandWindow::ReleaseCapture() {
  if (HasCapture())
    connection_->wayland_window_manager()->UngrabLocatedEvents(this);
  // See comment in SetCapture() for details on wayland and grabs.
}

bool WaylandWindow::HasCapture() const {
  return connection_->wayland_window_manager()->located_events_grabber() ==
         this;
}

void WaylandWindow::ToggleFullscreen() {}

void WaylandWindow::Maximize() {}

void WaylandWindow::Minimize() {}

void WaylandWindow::Restore() {}

PlatformWindowState WaylandWindow::GetPlatformWindowState() const {
  // Remove normal state for all the other types of windows as it's only the
  // WaylandSurface that supports state changes.
  return PlatformWindowState::kNormal;
}

void WaylandWindow::Activate() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandWindow::Deactivate() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandWindow::SetUseNativeFrame(bool use_native_frame) {
  // See comment below in ShouldUseNativeFrame.
  NOTIMPLEMENTED_LOG_ONCE();
}

bool WaylandWindow::ShouldUseNativeFrame() const {
  // This depends on availability of XDG-Decoration protocol extension.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void WaylandWindow::SetCursor(PlatformCursor cursor) {
  scoped_refptr<BitmapCursorOzone> bitmap =
      BitmapCursorFactoryOzone::GetBitmapCursor(cursor);
  if (bitmap_ == bitmap)
    return;

  bitmap_ = bitmap;

  if (bitmap_) {
    connection_->SetCursorBitmap(bitmap_->bitmaps(), bitmap_->hotspot());
  } else {
    connection_->SetCursorBitmap(std::vector<SkBitmap>(), gfx::Point());
  }
}

void WaylandWindow::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void WaylandWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

void WaylandWindow::SetRestoredBoundsInPixels(const gfx::Rect& bounds_px) {
  restored_bounds_px_ = bounds_px;
}

gfx::Rect WaylandWindow::GetRestoredBoundsInPixels() const {
  return restored_bounds_px_;
}

bool WaylandWindow::ShouldWindowContentsBeTransparent() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void WaylandWindow::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandWindow::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                   const gfx::ImageSkia& app_icon) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandWindow::SizeConstraintsChanged() {}

bool WaylandWindow::CanDispatchEvent(const PlatformEvent& event) {
  if (event->IsMouseEvent())
    return has_pointer_focus_;
  if (event->IsKeyEvent())
    return has_keyboard_focus_;
  if (event->IsTouchEvent())
    return has_touch_focus_;
  return false;
}

uint32_t WaylandWindow::DispatchEvent(const PlatformEvent& native_event) {
  Event* event = static_cast<Event*>(native_event);

  if (event->IsLocatedEvent()) {
    auto* event_grabber =
        connection_->wayland_window_manager()->located_events_grabber();
    auto* root_parent_window = GetRootParentWindow();
    // We must reroute the events to the event grabber iff these windows belong
    // to the same root parent window. For example, there are 2 top level
    // Wayland windows. One of them (window_1) has a child menu window that is
    // the event grabber. If the mouse is moved over the window_1, it must
    // reroute the events to the event grabber. If the mouse is moved over the
    // window_2, the events mustn't be rerouted, because that belongs to another
    // stack of windows. Remember that Wayland sends local surface coordinates,
    // and continuing rerouting all the events may result in events sent to the
    // grabber even though the mouse is over another root window.
    //
    if (event_grabber &&
        root_parent_window == event_grabber->GetRootParentWindow()) {
      ConvertEventLocationToTargetWindowLocation(
          event_grabber->GetBounds().origin(), GetBounds().origin(),
          event->AsLocatedEvent());
      return event_grabber->DispatchEventToDelegate(native_event);
    }
  }

  // Dispatch all keyboard events to the root window.
  if (event->IsKeyEvent())
    return GetRootParentWindow()->DispatchEventToDelegate(event);

  return DispatchEventToDelegate(native_event);
}

void WaylandWindow::HandleSurfaceConfigure(int32_t widht,
                                           int32_t height,
                                           bool is_maximized,
                                           bool is_fullscreen,
                                           bool is_activated) {
  NOTREACHED()
      << "Only shell surfaces must receive HandleSurfaceConfigure calls.";
}

void WaylandWindow::HandlePopupConfigure(const gfx::Rect& bounds_dip) {
  NOTREACHED() << "Only shell popups must receive HandlePopupConfigure calls.";
}

///@name USE_NEVA_APPRUNTIME
///@{
WaylandInputMethodContext* WaylandWindow::GetInputMethodContext() {
  return static_cast<WaylandInputMethodContext*>(
      delegate_->GetInputMethodContext());
}

void WaylandWindow::HandleStateChanged(PlatformWindowState state) {}

void WaylandWindow::HandleActivationChanged(bool is_activated) {}

void WaylandWindow::HandleCursorVisibilityChanged(bool is_visible) {
  delegate_->OnCursorVisibilityChanged(is_visible);
}

void WaylandWindow::HandleExposed() {
  delegate_->OnWindowExposed();
}

void WaylandWindow::HandleStateAboutToChange(PlatformWindowState state) {
  delegate_->OnWindowStateAboutToChange(state);
}

void WaylandWindow::CreateGroup(const ui::WindowGroupConfiguration& config) {
  if (connection_->extension()) {
    SurfaceGroupCompositorWrapper* surface_group_compositor =
        connection_->extension()->GetSurfaceGroupCompositor();

    if (surface_group_compositor)
      surface_group_ =
          surface_group_compositor->CreateSurfaceGroup(this, config.name);
  }

  if (surface_group_) {
    surface_group_->AllowAnonymousLayers(config.is_anonymous);
    for (auto& layer : config.layers)
      surface_group_->CreateLayer(layer.name, layer.z_order);
    connection_->ScheduleFlush();
  }
}

void WaylandWindow::AttachToGroup(const std::string& group_name,
                                  const std::string& layer_name) {
  if (connection_->extension()) {
    SurfaceGroupCompositorWrapper* surface_group_compositor =
        connection_->extension()->GetSurfaceGroupCompositor();

    if (surface_group_compositor)
      surface_group_ = surface_group_compositor->GetSurfaceGroup(group_name);
  }

  if (surface_group_) {
    surface_group_->AttachToLayer(this, layer_name);
    connection_->ScheduleFlush();
  }
}

void WaylandWindow::FocusGroupOwner() {
  if (surface_group_) {
    surface_group_->FocusOwner();
    connection_->ScheduleFlush();
  }
}

void WaylandWindow::FocusGroupLayer() {
  if (surface_group_) {
    surface_group_->FocusLayer();
    connection_->ScheduleFlush();
  }
}

void WaylandWindow::DetachGroup() {
  if (surface_group_) {
    surface_group_->Detach(this);
    connection_->ScheduleFlush();
  }
}

void WaylandWindow::ShowInputPanel() {
  if (connection_->extension()) {
    InputPanelManager* input_panel_manager =
        connection_->extension()->GetInputPanelManager();

    if (input_panel_manager) {
      InputPanel* input_panel = input_panel_manager->GetInputPanel(this);

      if (input_panel) {
        input_panel->ShowInputPanel();
        connection_->ScheduleFlush();
      }
    }
  }
}

void WaylandWindow::HideInputPanel() {
  if (connection_->extension()) {
    InputPanelManager* input_panel_manager =
        connection_->extension()->GetInputPanelManager();

    if (input_panel_manager) {
      InputPanel* input_panel = input_panel_manager->GetInputPanel(this);

      if (input_panel) {
        input_panel->HideInputPanel();
        connection_->ScheduleFlush();
      }
    }
  }
}

void WaylandWindow::SetInputContentType(TextInputType text_input_type,
                                        int text_input_flags) {
  if (connection_->extension()) {
    InputPanelManager* input_panel_manager =
        connection_->extension()->GetInputPanelManager();

    if (input_panel_manager) {
      InputPanel* input_panel = input_panel_manager->GetInputPanel(this);

      if (input_panel) {
        input_panel->SetInputContentType(text_input_type, text_input_flags);
        connection_->ScheduleFlush();
      }
    }
  }
}

void WaylandWindow::SetSurroundingText(const std::string& text,
                                       std::size_t cursor_position,
                                       std::size_t anchor_position) {
  if (connection_->extension()) {
    InputPanelManager* input_panel_manager =
        connection_->extension()->GetInputPanelManager();

    if (input_panel_manager) {
      InputPanel* input_panel = input_panel_manager->GetInputPanel(this);

      if (input_panel) {
        input_panel->SetSurroundingText(text, cursor_position, anchor_position);
        connection_->ScheduleFlush();
      }
    }
  }
}

void WaylandWindow::XInputActivate(const std::string& type) {
  if (connection_->extension()) {
    ExtendedInputWrapper* extended_input =
        connection_->extension()->GetExtendedInput();

    if (extended_input) {
      extended_input->Activate(type);
      connection_->ScheduleFlush();
    }
  }
}

void WaylandWindow::XInputDeactivate() {
  if (connection_->extension()) {
    ExtendedInputWrapper* extended_input =
        connection_->extension()->GetExtendedInput();

    if (extended_input) {
      extended_input->Deactivate();
      connection_->ScheduleFlush();
    }
  }
}

void WaylandWindow::XInputInvokeAction(std::uint32_t keysym,
                                       XInputKeySymbolType symbol_type,
                                       XInputEventType event_type) {
  if (connection_->extension()) {
    ExtendedInputWrapper* extended_input =
        connection_->extension()->GetExtendedInput();

    if (extended_input) {
      extended_input->InvokeAction(keysym, symbol_type, event_type);
      connection_->ScheduleFlush();
    }
  }
}

void WaylandWindow::HandleKeyboardEnter() {
  delegate_->OnKeyboardEnter();
}

void WaylandWindow::HandleKeyboardLeave() {
  delegate_->OnKeyboardLeave();
}
///@}

void WaylandWindow::OnCloseRequest() {
  delegate_->OnCloseRequest();
}

void WaylandWindow::OnDragEnter(const gfx::PointF& point,
                                std::unique_ptr<OSExchangeData> data,
                                int operation) {}

int WaylandWindow::OnDragMotion(const gfx::PointF& point,
                                uint32_t time,
                                int operation) {
  return -1;
}

void WaylandWindow::OnDragDrop(std::unique_ptr<OSExchangeData> data) {}

void WaylandWindow::OnDragLeave() {}

void WaylandWindow::OnDragSessionClose(uint32_t dnd_action) {}

void WaylandWindow::SetBoundsDip(const gfx::Rect& bounds_dip) {
  SetBounds(gfx::ScaleToRoundedRect(bounds_dip, buffer_scale_));
}

bool WaylandWindow::Initialize(PlatformWindowInitProperties properties) {
  // Properties contain DIP bounds but the buffer scale is initially 1 so it's
  // OK to assign.  The bounds will be recalculated when the buffer scale
  // changes.
  DCHECK_EQ(buffer_scale_, 1);
  bounds_px_ = properties.bounds;
  opacity_ = properties.opacity;
  type_ = properties.type;

  surface_.reset(wl_compositor_create_surface(connection_->compositor()));
  if (!surface_) {
    LOG(ERROR) << "Failed to create wl_surface";
    return false;
  }
  wl_surface_set_user_data(surface_.get(), this);
  AddSurfaceListener();

  connection_->wayland_window_manager()->AddWindow(GetWidget(), this);

  if (!OnInitialize(std::move(properties)))
    return false;

  connection_->ScheduleFlush();

  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  delegate_->OnAcceleratedWidgetAvailable(GetWidget());

  // Will do nothing for menus because they have got their scale above.
  UpdateBufferScale(false);

  MaybeUpdateOpaqueRegion();
  return true;
}

void WaylandWindow::SetBufferScale(int32_t new_scale, bool update_bounds) {
  DCHECK_GT(new_scale, 0);

  if (new_scale == buffer_scale_)
    return;

  auto old_scale = buffer_scale_;
  buffer_scale_ = new_scale;
  if (update_bounds)
    SetBoundsDip(gfx::ScaleToRoundedRect(bounds_px_, 1.0 / old_scale));

  DCHECK(surface());
  wl_surface_set_buffer_scale(surface(), buffer_scale_);
  connection_->ScheduleFlush();
}

WaylandWindow* WaylandWindow::GetParentWindow(
    gfx::AcceleratedWidget parent_widget) {
  auto* parent_window =
      connection_->wayland_window_manager()->GetWindow(parent_widget);

  // If propagated parent has already had a child, it means that |this| is a
  // submenu of a 3-dot menu. In aura, the parent of a 3-dot menu and its
  // submenu is the main native widget, which is the main window. In contrast,
  // Wayland requires a menu window to be a parent of a submenu window. Thus,
  // check if the suggested parent has a child. If yes, take its child as a
  // parent of |this|.
  // Another case is a notifcation window or a drop down window, which do not
  // have a parent in aura. In this case, take the current focused window as a
  // parent.
  if (parent_window && parent_window->child_window_)
    return parent_window->child_window_;
  if (!parent_window)
    return connection_->wayland_window_manager()->GetCurrentFocusedWindow();
  return parent_window;
}

WaylandWindow* WaylandWindow::GetRootParentWindow() {
  return parent_window_ ? parent_window_->GetRootParentWindow() : this;
}

void WaylandWindow::AddSurfaceListener() {
  static struct wl_surface_listener surface_listener = {
      &WaylandWindow::Enter,
      &WaylandWindow::Leave,
  };
  wl_surface_add_listener(surface_.get(), &surface_listener, this);
}

void WaylandWindow::AddEnteredOutputId(struct wl_output* output) {
  // Wayland does weird things for menus so instead of tracking outputs that
  // we entered or left, we take that from the parent window and ignore this
  // event.
  if (wl::IsMenuType(type()))
    return;

  const uint32_t entered_output_id =
      connection_->wayland_output_manager()->GetIdForOutput(output);
  DCHECK_NE(entered_output_id, 0u);
  auto result = entered_outputs_ids_.insert(entered_output_id);
  DCHECK(result.first != entered_outputs_ids_.end());

  UpdateBufferScale(true);
}

void WaylandWindow::RemoveEnteredOutputId(struct wl_output* output) {
  // Wayland does weird things for menus so instead of tracking outputs that
  // we entered or left, we take that from the parent window and ignore this
  // event.
  if (wl::IsMenuType(type()))
    return;

  const uint32_t left_output_id =
      connection_->wayland_output_manager()->GetIdForOutput(output);
  auto entered_output_id_it = entered_outputs_ids_.find(left_output_id);
  // Workaround: when a user switches physical output between two displays,
  // a window does not necessarily receive enter events immediately or until
  // a user resizes/moves the window. It means that switching output between
  // displays in a single output mode results in leave events, but the surface
  // might not have received enter event before. Thus, remove the id of left
  // output only if it was stored before.
  if (entered_output_id_it != entered_outputs_ids_.end())
    entered_outputs_ids_.erase(entered_output_id_it);

  UpdateBufferScale(true);
}

void WaylandWindow::UpdateCursorPositionFromEvent(
    std::unique_ptr<Event> event) {
  DCHECK(event->IsLocatedEvent());

  // This is a tricky part. Initially, Wayland sends events to surfaces the
  // events are targeted for. But, in order to fulfill Chromium's assumptions
  // about event targets, some of the events are rerouted and their locations
  // are converted. The event we got here is rerouted and it has had its
  // location fixed.
  //
  // Basically, this method must translate coordinates of all events
  // in regards to top-level windows' coordinates as it's always located at
  // origin (0,0) from Chromium point of view (remember that Wayland doesn't
  // provide global coordinates to its clients). And it's totally fine to use it
  // as the target. Thus, the location of the |event| is always converted using
  // the top-level window's bounds as the target excluding cases, when the
  // mouse/touch is over a top-level window.
  auto* toplevel_window = GetRootParentWindow();
  if (toplevel_window != this) {
    ConvertEventLocationToTargetWindowLocation(
        toplevel_window->GetBounds().origin(), GetBounds().origin(),
        event->AsLocatedEvent());
  }
  auto* cursor_position = connection_->wayland_cursor_position();
  if (cursor_position) {
    cursor_position->OnCursorPositionChanged(
        event->AsLocatedEvent()->location());
  }
}

WaylandWindow* WaylandWindow::GetTopLevelWindow() {
  return parent_window_ ? parent_window_->GetTopLevelWindow() : this;
}

void WaylandWindow::MaybeUpdateOpaqueRegion() {
  if (!IsOpaqueWindow())
    return;

  wl::Object<wl_region> region(
      wl_compositor_create_region(connection_->compositor()));
  wl_region_add(region.get(), 0, 0, bounds_px_.width(), bounds_px_.height());
  wl_surface_set_opaque_region(surface(), region.get());

  connection_->ScheduleFlush();
}

bool WaylandWindow::IsOpaqueWindow() const {
  return opacity_ == ui::PlatformWindowOpacity::kOpaqueWindow;
}

uint32_t WaylandWindow::DispatchEventToDelegate(
    const PlatformEvent& native_event) {
  auto* event = static_cast<Event*>(native_event);
  if (event->IsLocatedEvent())
    UpdateCursorPositionFromEvent(Event::Clone(*event));

  DispatchEventFromNativeUiEvent(
      native_event, base::BindOnce(&PlatformWindowDelegate::DispatchEvent,
                                   base::Unretained(delegate_)));
  return POST_DISPATCH_STOP_PROPAGATION;
}

// static
void WaylandWindow::Enter(void* data,
                          struct wl_surface* wl_surface,
                          struct wl_output* output) {
  auto* window = static_cast<WaylandWindow*>(data);
  if (window) {
    DCHECK(window->surface_.get() == wl_surface);
    window->AddEnteredOutputId(output);
  }
}

// static
void WaylandWindow::Leave(void* data,
                          struct wl_surface* wl_surface,
                          struct wl_output* output) {
  auto* window = static_cast<WaylandWindow*>(data);
  if (window) {
    DCHECK(window->surface_.get() == wl_surface);
    window->RemoveEnteredOutputId(output);
  }
}

}  // namespace ui
