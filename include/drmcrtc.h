/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_DRM_CRTC_H_
#define ANDROID_DRM_CRTC_H_

#include "drmmode.h"
#include "drmproperty.h"

#include <stdint.h>
#include <xf86drmMode.h>

namespace android {

class DrmDevice;

class DrmCrtc {
 public:
  DrmCrtc(DrmDevice *drm, drmModeCrtcPtr c, unsigned pipe);
  DrmCrtc(const DrmCrtc &) = delete;
  DrmCrtc &operator=(const DrmCrtc &) = delete;

  int Init();

  uint32_t id() const;
  unsigned pipe() const;

  int display() const;
  void set_display(int display);

  bool can_bind(int display) const;
  bool can_overscan() const;
  bool get_afbc() const;
  bool get_alpha_scale() const;
  uint32_t get_soc_id() const { return soc_id_; }
  uint32_t get_port_id() const { return port_id_; }
  const DrmProperty &active_property() const;
  const DrmProperty &mode_property() const;
  const DrmProperty &out_fence_ptr_property() const;
  const DrmProperty &left_margin_property() const;
  const DrmProperty &right_margin_property() const;
  const DrmProperty &top_margin_property() const;
  const DrmProperty &bottom_margin_property() const;
  const DrmProperty &alpha_scale_property() const;

 DrmDevice *getDrmDevice(){ return drm_; }

 private:
  DrmDevice *drm_;

  uint32_t id_;
  unsigned pipe_;
  int display_;

  DrmMode mode_;

  bool can_overscan_;
  bool can_alpha_scale_;
  bool b_afbc_;

  DrmProperty active_property_;
  DrmProperty mode_property_;
  DrmProperty out_fence_ptr_property_;
  //RK support
  DrmProperty feature_property_;
  DrmProperty left_margin_property_;
  DrmProperty top_margin_property_;
  DrmProperty right_margin_property_;
  DrmProperty bottom_margin_property_;
  DrmProperty alpha_scale_property_;
  DrmProperty soc_type_property_;
  DrmProperty port_id_property_;

  // Vop2
  uint32_t soc_id_;
  uint32_t port_id_;
};
}  // namespace android

#endif  // ANDROID_DRM_CRTC_H_
