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

#define LOG_TAG "hwc-drm-plane"

#include "drmplane.h"
#include "drmdevice.h"

#include <errno.h>
#include <stdint.h>
#include <cinttypes>

#include <log/log.h>
#include <xf86drmMode.h>

namespace android {

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

struct plane_type_name plane_type_names[] = {
  { DRM_PLANE_TYPE_CLUSTER0_WIN0, "Cluster0-win0" },
  { DRM_PLANE_TYPE_CLUSTER0_WIN1, "Cluster0-win1" },
  { DRM_PLANE_TYPE_CLUSTER1_WIN0, "Cluster1-win0" },
  { DRM_PLANE_TYPE_CLUSTER1_WIN1, "Cluster1-win1" },
  { DRM_PLANE_TYPE_ESMART0_WIN0, "Esmart0-win0" },
  { DRM_PLANE_TYPE_ESMART1_WIN0, "Esmart1-win0" },
  { DRM_PLANE_TYPE_SMART0_WIN0, "Smart0-win0" },
  { DRM_PLANE_TYPE_SMART1_WIN0, "Smart1-win0" },
  { DRM_PLANE_TYPE_Unknown, "unknown" },
};

struct plane_rotation_type_name plane_rotation_type_names[] = {
  { DRM_PLANE_ROTATION_0, "rotate-0" },
  { DRM_PLANE_ROTATION_90, "rotate-90" },
  { DRM_PLANE_ROTATION_270, "rotate-270" },
  { DRM_PLANE_ROTATION_X_MIRROR, "reflect-x" },
  { DRM_PLANE_ROTATION_Y_MIRROR, "reflect-y" },
  { DRM_PLANE_ROTATION_Unknown, "unknown" },
};

DrmPlane::DrmPlane(DrmDevice *drm, drmModePlanePtr p)
    : drm_(drm), id_(p->plane_id), possible_crtc_mask_(p->possible_crtcs), plane_(p) {
}

int DrmPlane::Init() {
  DrmProperty p;

  int ret = drm_->GetPlaneProperty(*this, "type", &p);
  if (ret) {
    ALOGE("Could not get plane type property");
    return ret;
  }

  uint64_t type;
  std::tie(ret, type) = p.value();
  if (ret) {
    ALOGE("Failed to get plane type property value");
    return ret;
  }
  switch (type) {
    case DRM_PLANE_TYPE_OVERLAY:
    case DRM_PLANE_TYPE_PRIMARY:
    case DRM_PLANE_TYPE_CURSOR:
      type_ = (uint32_t)type;
      break;
    default:
      ALOGE("Invalid plane type %" PRIu64, type);
      return -EINVAL;
  }

  ret = drm_->GetPlaneProperty(*this, "CRTC_ID", &crtc_property_);
  if (ret) {
    ALOGE("Could not get CRTC_ID property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "FB_ID", &fb_property_);
  if (ret) {
    ALOGE("Could not get FB_ID property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "CRTC_X", &crtc_x_property_);
  if (ret) {
    ALOGE("Could not get CRTC_X property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "CRTC_Y", &crtc_y_property_);
  if (ret) {
    ALOGE("Could not get CRTC_Y property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "CRTC_W", &crtc_w_property_);
  if (ret) {
    ALOGE("Could not get CRTC_W property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "CRTC_H", &crtc_h_property_);
  if (ret) {
    ALOGE("Could not get CRTC_H property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "SRC_X", &src_x_property_);
  if (ret) {
    ALOGE("Could not get SRC_X property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "SRC_Y", &src_y_property_);
  if (ret) {
    ALOGE("Could not get SRC_Y property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "SRC_W", &src_w_property_);
  if (ret) {
    ALOGE("Could not get SRC_W property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "SRC_H", &src_h_property_);
  if (ret) {
    ALOGE("Could not get SRC_H property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "EOTF", &eotf_property_);
  if (ret)
    ALOGI("Could not get eotf property");

  ret = drm_->GetPlaneProperty(*this, "COLOR_SPACE", &colorspace_property_);
  if (ret)
    ALOGI("Could not get colorspace property");

  ret = drm_->GetPlaneProperty(*this, "ZPOS", &zpos_property_);
  if (ret){
    ALOGE("Could not get ZPOS property, try to get zpos property");
    ret = drm_->GetPlaneProperty(*this, "zpos", &zpos_property_);
    if (ret)
      ALOGE("Could not get zpos property");
  }

  ret = drm_->GetPlaneProperty(*this, "SHARE_FLAGS", &area_id_property_);
  if (ret)
    ALOGE("Could not get AREA_ID property");

  ret = drm_->GetPlaneProperty(*this, "SHARE_ID", &share_id_property_);
  if (ret)
    ALOGE("Could not get SHARE_ID property");

  ret = drm_->GetPlaneProperty(*this, "FEATURE", &feature_property_);
  if (ret)
    ALOGE("Could not get FEATURE property");

  support_format_list.clear();
  for (uint32_t j = 0; j < plane_->count_formats; j++) {
    support_format_list.insert(plane_->formats[j]);
  }

#ifdef VOP2
  ret = drm_->GetPlaneProperty(*this, "alpha", &alpha_property_);
  if (ret)
    ALOGI("Could not get alpha property");

  ret = drm_->GetPlaneProperty(*this, "pixel blend mode", &blend_mode_property_);
  if (ret)
    ALOGI("Could not get pixel blend mode property");


  std::tie(ret,b_scale_)   = feature_property_.bitmask("scale");
  std::tie(ret,b_alpha_)   = feature_property_.bitmask("alpha");
  std::tie(ret,b_hdr2sdr_) = feature_property_.bitmask("hdr2sdr");
  std::tie(ret,b_sdr2hdr_) = feature_property_.bitmask("sdr2hdr");
  std::tie(ret,b_afbdc_)   = feature_property_.bitmask("afbdc");
  b_afbc_prop_ = true;

  bool find_name = false;
  ret = drm_->GetPlaneProperty(*this, "rotation", &rotation_property_);
  if (ret)
    ALOGE("Could not get FEATURE property");
  else{
    for(int i = 0; i < ARRAY_SIZE(plane_rotation_type_names); i++){
      find_name = false;
      std::tie(ret,find_name) = rotation_property_.bitmask(plane_rotation_type_names[i].name);
      if(find_name){
        rotate_ |= plane_rotation_type_names[i].type;
      }
    }
  }

  ret = drm_->GetPlaneProperty(*this, "NAME", &name_property_);
  if (ret)
    ALOGE("Could not get NAME property");
  else{
    for(int i = 0; i < ARRAY_SIZE(plane_type_names); i++){
      find_name = false;
      std::tie(ret,find_name) = name_property_.bitmask(plane_type_names[i].name);
      if(find_name){
        win_type_ = plane_type_names[i].type;
        break;
      }
    }
  }

  ret = drm_->GetPlaneProperty(*this, "INPUT_WIDTH", &input_w_property_);
  if (ret)
    ALOGE("Could not get INPUT_WIDTH property");

  ret = drm_->GetPlaneProperty(*this, "INPUT_HEIGHT", &input_h_property_);
  if (ret)
    ALOGE("Could not get INPUT_HEIGHT property");

  ret = drm_->GetPlaneProperty(*this, "OUTPUT_WIDTH", &output_w_property_);
  if (ret)
    ALOGE("Could not get OUTPUT_WIDTH property");

  ret = drm_->GetPlaneProperty(*this, "OUTPUT_HEIGHT", &output_h_property_);
  if (ret)
    ALOGE("Could not get OUTPUT_HEIGHT property");

  ret = drm_->GetPlaneProperty(*this, "SCALE_RATE", &scale_rate_property_);
  if (ret)
    ALOGE("Could not get SCALE_RATE property");
  else{
    uint64_t scale_rate=0;
    std::tie(ret,scale_rate) = scale_rate_property_.range_min();
    if(!ret)
      scale_min_ = 1/scale_rate;
    else
      ALOGE("Could not get INPUT_WIDTH range_min property");
    std::tie(ret,scale_rate) = scale_rate_property_.range_max();
    if(!ret)
      scale_max_ = scale_rate;
    else
      ALOGE("Could not get INPUT_WIDTH range_max property");
  }

#else

  ret = drm_->GetPlaneProperty(*this, "GLOBAL_ALPHA", &alpha_property_);
  if (ret)
    ALOGI("Could not get alpha property");

  ret = drm_->GetPlaneProperty(*this, "BLEND_MODE", &blend_mode_property_);
  if (ret)
    ALOGI("Could not get pixel blend mode property");


  uint64_t scale=0, alpha=0, hdr2sdr=0, sdr2hdr=0, afbdc=0;

  feature_property_.set_feature("scale");
  std::tie(ret,scale) = feature_property_.value();
  b_scale_ = ((scale & DRM_PLANE_FEARURE_BIT_SCALE) > 0)?true:false;

  rotation_property_.set_feature("alpha");
  std::tie(ret, alpha) = rotation_property_.value();
  b_alpha_ = ((alpha & DRM_PLANE_FEARURE_BIT_ALPHA) > 0)?true:false;;

  feature_property_.set_feature("hdr2sdr");
  std::tie(ret, hdr2sdr) = feature_property_.value();
  b_hdr2sdr_ = ((hdr2sdr == DRM_PLANE_FEARURE_BIT_HDR2SDR) > 0)?true:false;

  feature_property_.set_feature("sdr2hdr");
  std::tie(ret, sdr2hdr) = feature_property_.value();
  b_sdr2hdr_ = ((sdr2hdr & DRM_PLANE_FEARURE_BIT_SDR2HDR) > 0)?true:false;

  feature_property_.set_feature("afbdc");
  std::tie(ret, afbdc) = feature_property_.value();
  b_afbdc_ = ((afbdc & DRM_PLANE_FEARURE_BIT_AFBDC) > 0)?true:false;
  if(0xFF == afbdc)
    b_afbc_prop_ = false;
  else
    b_afbc_prop_ = true;
#endif
  return 0;
}

uint32_t DrmPlane::id() const {
  return id_;
}

bool DrmPlane::GetCrtcSupported(const DrmCrtc &crtc) const {
  return !!((1 << crtc.pipe()) & possible_crtc_mask_);
}

uint32_t DrmPlane::type() const {
  return type_;
}

DrmPlaneType DrmPlane::win_type() const{
  return win_type_;
}

const DrmProperty &DrmPlane::crtc_property() const {
  return crtc_property_;
}

const DrmProperty &DrmPlane::fb_property() const {
  return fb_property_;
}

const DrmProperty &DrmPlane::crtc_x_property() const {
  return crtc_x_property_;
}

const DrmProperty &DrmPlane::crtc_y_property() const {
  return crtc_y_property_;
}

const DrmProperty &DrmPlane::crtc_w_property() const {
  return crtc_w_property_;
}

const DrmProperty &DrmPlane::crtc_h_property() const {
  return crtc_h_property_;
}

const DrmProperty &DrmPlane::src_x_property() const {
  return src_x_property_;
}

const DrmProperty &DrmPlane::src_y_property() const {
  return src_y_property_;
}

const DrmProperty &DrmPlane::src_w_property() const {
  return src_w_property_;
}

const DrmProperty &DrmPlane::src_h_property() const {
  return src_h_property_;
}

const DrmProperty &DrmPlane::zpos_property() const {
  return zpos_property_;
}

const DrmProperty &DrmPlane::rotation_property() const {
  return rotation_property_;
}

const DrmProperty &DrmPlane::alpha_property() const {
  return alpha_property_;
}

const DrmProperty &DrmPlane::blend_property() const {
  return blend_mode_property_;
}

// RK support
const DrmProperty &DrmPlane::eotf_property() const {
  return eotf_property_;
}

  const DrmProperty &DrmPlane::colorspace_property() const {
    return colorspace_property_;
  }

  const DrmProperty &DrmPlane::area_id_property() const {
  return area_id_property_;
}

const DrmProperty &DrmPlane::share_id_property() const {
  return share_id_property_;
}

const DrmProperty &DrmPlane::feature_property() const {
  return feature_property_;
}

const DrmProperty &DrmPlane::name_property() const{
  return name_property_;
}

const DrmProperty &DrmPlane::input_w_property() const{
  return input_w_property_;
}

const DrmProperty &DrmPlane::input_h_property() const{
  return input_h_property_;
}

const DrmProperty &DrmPlane::output_w_property() const{
  return output_w_property_;
}

const DrmProperty &DrmPlane::output_h_property() const{
  return output_h_property_;
}

const DrmProperty &DrmPlane::scale_rate_property() const{
  return scale_rate_property_;
}

bool DrmPlane::get_scale(){
    return b_scale_;
}

bool DrmPlane::get_rotate(){
    return (rotate_ & DRM_PLANE_ROTATION_90)
        || (rotate_ & DRM_PLANE_ROTATION_270);
}

bool DrmPlane::get_hdr2sdr(){
    return b_hdr2sdr_;
}

bool DrmPlane::get_sdr2hdr(){
    return b_sdr2hdr_;
}

bool DrmPlane::get_afbc(){
    return b_afbdc_;
}

bool DrmPlane::get_afbc_prop(){
    return b_afbc_prop_;
}

bool DrmPlane::get_yuv(){
    return b_yuv_;
}

void DrmPlane::set_yuv(bool b_yuv)
{
    b_yuv_ = b_yuv;
}

bool DrmPlane::is_use(){
    return b_use_;
}

void DrmPlane::set_use(bool b_use)
{
    b_use_ = b_use;
}

bool DrmPlane::is_reserved(){
  return b_reserved_;
}

void DrmPlane::set_reserved(bool b_reserved) {
    b_reserved_ = b_reserved;
}

bool DrmPlane::is_support_scale(float scale_rate){
  return (scale_rate > scale_min_) || (scale_rate < scale_max_ );
}

bool DrmPlane::is_support_format(uint32_t format, bool afbcd){
  if((win_type_ & DRM_PLANE_TYPE_CLUSTER_MASK) > 0 && afbcd)
    return support_format_list.count(format);
  else if((win_type_ & DRM_PLANE_TYPE_CLUSTER_MASK) == 0 && !afbcd)
    return support_format_list.count(format);
  else
    return false;
}

}  // namespace android
