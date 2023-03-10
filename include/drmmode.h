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

#ifndef ANDROID_DRM_MODE_H_
#define ANDROID_DRM_MODE_H_

#include <stdint.h>
#include <xf86drmMode.h>
#include <string>

#define DRM_MODE_FLAG_420_MASK			(0x03<<23)

/*
 * Legacy definitions for old code that doesn't use
 * the above mask definitions. Don't use in future code.
 */
/* rotation property bits */
#ifndef DRM_ROTATE_0
#define DRM_ROTATE_0   0
#define DRM_ROTATE_90  1
#define DRM_ROTATE_180 2
#define DRM_ROTATE_270 3
#define DRM_REFLECT_X  4
#define DRM_REFLECT_Y  5
#endif

namespace android {

class DrmMode {
 public:
  DrmMode() = default;
  DrmMode(drmModeModeInfoPtr m);

  bool operator==(const drmModeModeInfo &m) const;
  bool operator==(const DrmMode &m) const;
  bool equal(const DrmMode &m) const;
  bool equal_no_flag_and_type(const DrmMode &m) const;
  bool equal(uint32_t width, uint32_t height, uint32_t vrefresh, bool interlaced) const;
  bool equal(uint32_t width, uint32_t height, uint32_t vrefresh,
                     uint32_t flag, uint32_t clk, bool interlaced) const;
  bool equal(uint32_t width, uint32_t height, float vrefresh,
             uint32_t hsync_start, uint32_t hsync_end, uint32_t htotal,
             uint32_t vsync_start, uint32_t vsync_end, uint32_t vtotal,
             uint32_t flag) const;
  bool equal(uint32_t width, uint32_t height,
             uint32_t hsync_start, uint32_t hsync_end, uint32_t htotal,
             uint32_t vsync_start, uint32_t vsync_end, uint32_t vtotal,
             uint32_t flags, uint32_t clock) const;
  void ToDrmModeModeInfo(drm_mode_modeinfo *m) const;

  void dump() const;

  uint32_t id() const;
  void set_id(uint32_t id);

  uint32_t clock() const;

  uint32_t h_display() const;
  uint32_t h_sync_start() const;
  uint32_t h_sync_end() const;
  uint32_t h_total() const;
  uint32_t h_skew() const;

  uint32_t v_display() const;
  uint32_t v_sync_start() const;
  uint32_t v_sync_end() const;
  uint32_t v_total() const;
  uint32_t v_scan() const;
  float v_refresh() const;

  uint32_t flags() const;
  uint32_t interlaced() const;
  uint32_t type() const;
  bool is_8k_mode() const;
  bool is_4k120p_mode() const;

  std::string name() const;

 private:
  uint32_t id_ = 0;
  uint32_t blob_id_ = 0;

  uint32_t clock_ = 0;

  uint32_t h_display_ = 0;
  uint32_t h_sync_start_ = 0;
  uint32_t h_sync_end_ = 0;
  uint32_t h_total_ = 0;
  uint32_t h_skew_ = 0;

  uint32_t v_display_ = 0;
  uint32_t v_sync_start_ = 0;
  uint32_t v_sync_end_ = 0;
  uint32_t v_total_ = 0;
  uint32_t v_scan_ = 0;
  uint32_t v_refresh_ = 0;

  uint32_t flags_ = 0;
  uint32_t type_ = 0;
  int interlaced_ =0;

  std::string name_;
};
}  // namespace android

#endif  // ANDROID_DRM_MODE_H_
