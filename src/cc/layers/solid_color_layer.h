// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CC_LAYERS_SOLID_COLOR_LAYER_H_
#define CC_LAYERS_SOLID_COLOR_LAYER_H_

#include "cc/cc_export.h"
#include "cc/layers/layer.h"

namespace cc {

// A Layer that renders a solid color. The color is specified by using
// SetBackgroundColor() on the base class.
class CC_EXPORT SolidColorLayer : public Layer {
 public:
  static scoped_refptr<SolidColorLayer> Create();

  SolidColorLayer(const SolidColorLayer&) = delete;
  SolidColorLayer& operator=(const SolidColorLayer&) = delete;

  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;

  void SetBackgroundColor(SkColor color) override;

#if defined(USE_NEVA_PUNCH_HOLE)
  void PushPropertiesTo(LayerImpl* layer) override;

  // Sets whether the quads with transparent color will be drawn
  // or ignore as meaninless.
  // Defaults to false.
  void SetForceDrawTransparentColor(bool force_draw);
#endif  // USE_NEVA_PUNCH_HOLE

 protected:
  SolidColorLayer();

 private:
  ~SolidColorLayer() override;

#if defined(USE_NEVA_PUNCH_HOLE)
  bool force_draw_transparent_color_;
#endif  // USE_NEVA_PUNCH_HOLE
};

}  // namespace cc
#endif  // CC_LAYERS_SOLID_COLOR_LAYER_H_
