/*
 * Copyright (C) 2020 Rockchip Electronics Co.Ltd.
 *
 * Modification based on code covered by the Apache License, Version 2.0 (the "License").
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS TO YOU ON AN "AS IS" BASIS
 * AND ANY AND ALL WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH SOFTWARE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF TITLE,
 * NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY, ACCURACY OR FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.
 *
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
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

#include "rockchip/platform/drmvop.h"
#include "drmdevice.h"

#include <log/log.h>

namespace android {

bool PlanStageVop::HasLayer(std::vector<DrmHwcLayer*>& layer_vector,DrmHwcLayer *layer){
        for (std::vector<DrmHwcLayer*>::const_iterator iter = layer_vector.begin();
               iter != layer_vector.end(); ++iter) {
            if((*iter)->uId_==layer->uId_)
                return true;
          }

          return false;
}

int PlanStageVop::IsXIntersect(hwc_rect_t* rec,hwc_rect_t* rec2){
    if(rec2->top == rec->top)
        return 1;
    else if(rec2->top < rec->top)
    {
        if(rec2->bottom > rec->top)
            return 1;
        else
            return 0;
    }
    else
    {
        if(rec->bottom > rec2->top  )
            return 1;
        else
            return 0;
    }
    return 0;
}


bool PlanStageVop::IsRec1IntersectRec2(hwc_rect_t* rec1, hwc_rect_t* rec2){
    int iMaxLeft,iMaxTop,iMinRight,iMinBottom;
    ALOGD_IF(LogLevel(DBG_DEBUG),"is_not_intersect: rec1[%d,%d,%d,%d],rec2[%d,%d,%d,%d]",rec1->left,rec1->top,
        rec1->right,rec1->bottom,rec2->left,rec2->top,rec2->right,rec2->bottom);

    iMaxLeft = rec1->left > rec2->left ? rec1->left: rec2->left;
    iMaxTop = rec1->top > rec2->top ? rec1->top: rec2->top;
    iMinRight = rec1->right <= rec2->right ? rec1->right: rec2->right;
    iMinBottom = rec1->bottom <= rec2->bottom ? rec1->bottom: rec2->bottom;

    if(iMaxLeft > iMinRight || iMaxTop > iMinBottom)
        return false;
    else
        return true;

    return false;
}

bool PlanStageVop::IsLayerCombine(DrmHwcLayer * layer_one,DrmHwcLayer * layer_two){
 #ifdef TARGET_BOARD_PLATFORM_RK3328
     ALOGD_IF(LogLevel(DBG_INFO),"rk3328 can't support multi region");
     return false;
 #endif
    //multi region only support RGBA888 RGBX8888 RGB888 565 BGRA888
    if(layer_one->iFormat_ >= HAL_PIXEL_FORMAT_YCrCb_NV12
        || layer_two->iFormat_ >= HAL_PIXEL_FORMAT_YCrCb_NV12
    //RK3288 Rk3326 multi region format must be the same
#if RK_MULTI_AREAS_FORMAT_LIMIT
        || (layer_one->iFormat_ != layer_two->iFormat_)
#endif
        || layer_one->alpha!= layer_two->alpha
        || layer_one->bScale_ || layer_two->bScale_
        || IsRec1IntersectRec2(&layer_one->display_frame,&layer_two->display_frame)
 #if RK_HOR_INTERSECT_LIMIT
        || IsXIntersect(&layer_one->display_frame,&layer_two->display_frame)
 #endif
        )
    {
        ALOGD_IF(LogLevel(DBG_INFO),"is_layer_combine layer one alpha=%d,is_scale=%d",layer_one->alpha,layer_one->bScale_);
        ALOGD_IF(LogLevel(DBG_INFO),"is_layer_combine layer two alpha=%d,is_scale=%d",layer_two->alpha,layer_two->bScale_);
        return false;
    }

    return true;
}

int PlanStageVop::CombineLayer(LayerMap& layer_map,std::vector<DrmHwcLayer*> &layers,uint32_t iPlaneSize){

    /*Group layer*/
    int zpos = 0;
    size_t i,j;
    uint32_t sort_cnt=0;
    bool is_combine = false;

    layer_map.clear();

    for (i = 0; i < layers.size(); ) {
        if(!layers[i]->bUse_)
            continue;

        sort_cnt=0;
        if(i == 0)
        {
            layer_map[zpos].push_back(layers[0]);
        }

        for(j = i+1; j < layers.size(); j++) {
            DrmHwcLayer *layer_one = layers[j];
            //layer_one.index = j;
            is_combine = false;

            for(size_t k = 0; k <= sort_cnt; k++ ) {
                DrmHwcLayer *layer_two = layers[j-1-k];
                //layer_two.index = j-1-k;
                //juage the layer is contained in layer_vector
                bool bHasLayerOne = HasLayer(layer_map[zpos],layer_one);
                bool bHasLayerTwo = HasLayer(layer_map[zpos],layer_two);

                //If it contain both of layers,then don't need to go down.
                if(bHasLayerOne && bHasLayerTwo)
                    continue;

                if(IsLayerCombine(layer_one,layer_two)) {
                    //append layer into layer_vector of layer_map_.
                    if(!bHasLayerOne && !bHasLayerTwo)
                    {
                        layer_map[zpos].emplace_back(layer_one);
                        layer_map[zpos].emplace_back(layer_two);
                        is_combine = true;
                    }
                    else if(!bHasLayerTwo)
                    {
                        is_combine = true;
                        for(std::vector<DrmHwcLayer*>::const_iterator iter= layer_map[zpos].begin();
                            iter != layer_map[zpos].end();++iter)
                        {
                            if((*iter)->uId_==layer_one->uId_)
                                    continue;

                            if(!IsLayerCombine(*iter,layer_two))
                            {
                                is_combine = false;
                                break;
                            }
                        }

                        if(is_combine)
                            layer_map[zpos].emplace_back(layer_two);
                    }
                    else if(!bHasLayerOne)
                    {
                        is_combine = true;
                        for(std::vector<DrmHwcLayer*>::const_iterator iter= layer_map[zpos].begin();
                            iter != layer_map[zpos].end();++iter)
                        {
                            if((*iter)->uId_==layer_two->uId_)
                                    continue;

                            if(!IsLayerCombine(*iter,layer_one))
                            {
                                is_combine = false;
                                break;
                            }
                        }

                        if(is_combine)
                        {
                            layer_map[zpos].emplace_back(layer_one);
                        }
                    }
                }

                if(!is_combine)
                {
                    //if it cann't combine two layer,it need start a new group.
                    if(!bHasLayerOne)
                    {
                        zpos++;
                        layer_map[zpos].emplace_back(layer_one);
                    }
                    is_combine = false;
                    break;
                }
             }
             sort_cnt++; //update sort layer count
             if(!is_combine)
             {
                break;
             }
        }

        if(is_combine)  //all remain layer or limit MOST_WIN_ZONES layer is combine well,it need start a new group.
            zpos++;
        if(sort_cnt)
            i+=sort_cnt;    //jump the sort compare layers.
        else
            i++;
    }

#if RK_SORT_AREA_BY_XPOS
  //sort layer by xpos
  for (LayerMap::iterator iter = layer_map.begin();
       iter != layer_map.end(); ++iter) {
        if(iter->second.size() > 1) {
            for(uint32_t i=0;i < iter->second.size()-1;i++) {
                for(uint32_t j=i+1;j < iter->second.size();j++) {
                     if(iter->second[i]->display_frame.left > iter->second[j]->display_frame.left) {
                        ALOGD_IF(LogLevel(DBG_DEBUG),"swap %d and %d",iter->second[i]->uId_,iter->second[j]->uId_);
                        std::swap(iter->second[i],iter->second[j]);
                     }
                 }
            }
        }
  }
#else
  //sort layer by ypos
  for (LayerMap::iterator iter = layer_map.begin();
       iter != layer_map.end(); ++iter) {
        if(iter->second.size() > 1) {
            for(uint32_t i=0;i < iter->second.size()-1;i++) {
                for(uint32_t j=i+1;j < iter->second.size();j++) {
                     if(iter->second[i]->display_frame.top > iter->second[j]->display_frame.top) {
                        ALOGD_IF(LogLevel(DBG_DEBUG),"swap %d and %d",iter->second[i]->uId_,iter->second[j]->uId_);
                        std::swap(iter->second[i],iter->second[j]);
                     }
                 }
            }
        }
  }
#endif

  for (LayerMap::iterator iter = layer_map.begin();
       iter != layer_map.end(); ++iter) {
        ALOGD_IF(LogLevel(DBG_DEBUG),"layer map id=%d,size=%zu",iter->first,iter->second.size());
        for(std::vector<DrmHwcLayer*>::const_iterator iter_layer = iter->second.begin();
            iter_layer != iter->second.end();++iter_layer)
        {
             ALOGD_IF(LogLevel(DBG_DEBUG),"\tlayer id=%d",(*iter_layer)->uId_);
        }
  }

    if((int)layer_map.size() > iPlaneSize)
    {
        ALOGD_IF(LogLevel(DBG_DEBUG),"map size=%zu should not bigger than plane size=%d", layer_map.size(), iPlaneSize);
        return -1;
    }

    return 0;

}

bool PlanStageVop::HasGetNoAfbcUsablePlanes(DrmCrtc *crtc, std::vector<PlaneGroup *> &plane_groups) {
    std::vector<DrmPlane *> usable_planes;
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(!(*iter)->bUse)
                //only count the first plane in plane group.
                std::copy_if((*iter)->planes.begin(), (*iter)->planes.begin()+1,
                       std::back_inserter(usable_planes),
                       [=](DrmPlane *plane) {
                       return !plane->is_use() && plane->GetCrtcSupported(*crtc) && !plane->get_afbc(); }
                       );
  }
  return usable_planes.size() > 0;;
}

bool PlanStageVop::HasGetNoYuvUsablePlanes(DrmCrtc *crtc, std::vector<PlaneGroup *> &plane_groups) {
    std::vector<DrmPlane *> usable_planes;
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(!(*iter)->bUse)
                //only count the first plane in plane group.
                std::copy_if((*iter)->planes.begin(), (*iter)->planes.begin()+1,
                       std::back_inserter(usable_planes),
                       [=](DrmPlane *plane) {
                       return !plane->is_use() && plane->GetCrtcSupported(*crtc) && !plane->get_yuv(); }
                       );
  }
  return usable_planes.size() > 0;;
}

bool PlanStageVop::HasGetNoScaleUsablePlanes(DrmCrtc *crtc, std::vector<PlaneGroup *> &plane_groups) {
    std::vector<DrmPlane *> usable_planes;
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(!(*iter)->bUse)
                //only count the first plane in plane group.
                std::copy_if((*iter)->planes.begin(), (*iter)->planes.begin()+1,
                       std::back_inserter(usable_planes),
                       [=](DrmPlane *plane) {
                       return !plane->is_use() && plane->GetCrtcSupported(*crtc) && !plane->get_scale(); }
                       );
  }
  return usable_planes.size() > 0;;
}

bool PlanStageVop::HasGetNoAlphaUsablePlanes(DrmCrtc *crtc, std::vector<PlaneGroup *> &plane_groups) {
    std::vector<DrmPlane *> usable_planes;
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(!(*iter)->bUse)
                //only count the first plane in plane group.
                std::copy_if((*iter)->planes.begin(), (*iter)->planes.begin()+1,
                       std::back_inserter(usable_planes),
                       [=](DrmPlane *plane) {
                       return !plane->is_use() && plane->GetCrtcSupported(*crtc) && !plane->alpha_property().id(); }
                       );
  }
  return usable_planes.size() > 0;
}

bool PlanStageVop::HasGetNoEotfUsablePlanes(DrmCrtc *crtc, std::vector<PlaneGroup *> &plane_groups) {
    std::vector<DrmPlane *> usable_planes;
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(!(*iter)->bUse)
                //only count the first plane in plane group.
                std::copy_if((*iter)->planes.begin(), (*iter)->planes.begin()+1,
                       std::back_inserter(usable_planes),
                       [=](DrmPlane *plane) {
                       return !plane->is_use() && plane->GetCrtcSupported(*crtc) && !plane->get_hdr2sdr(); }
                       );
  }
  return usable_planes.size() > 0;
}

bool PlanStageVop::GetCrtcSupported(const DrmCrtc &crtc, uint32_t possible_crtc_mask) {
  return !!((1 << crtc.pipe()) & possible_crtc_mask);
}

bool PlanStageVop::HasPlanesWithSize(DrmCrtc *crtc, int layer_size, std::vector<PlaneGroup *> &plane_groups) {
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(GetCrtcSupported(*crtc, (*iter)->possible_crtcs) && !(*iter)->bUse &&
                (*iter)->planes.size() == (size_t)layer_size)
                return true;
  }
  return false;
}

int PlanStageVop::ValidatePlane(DrmPlane *plane, DrmHwcLayer *layer) {
  int ret = 0;
  uint64_t blend;

  if ((plane->rotation_property().id() == 0) &&
      layer->transform != DrmHwcTransform::kIdentity) {
    ALOGE("Rotation is not supported on plane %d", plane->id());
    return -EINVAL;
  }

  if (plane->alpha_property().id() == 0 && layer->alpha != 0xffff) {
    ALOGE("Alpha is not supported on plane %d", plane->id());
    return -EINVAL;
  }

  if (plane->blend_property().id() > 0) {
    if ((layer->blending != DrmHwcBlending::kNone) &&
        (layer->blending != DrmHwcBlending::kPreMult)) {
      ALOGE("Blending is not supported on plane %d", plane->id());
      return -EINVAL;
    }
  } else {
    switch (layer->blending) {
      case DrmHwcBlending::kPreMult:
        std::tie(blend, ret) = plane->blend_property().GetEnumValueWithName(
            "Pre-multiplied");
        break;
      case DrmHwcBlending::kCoverage:
        std::tie(blend, ret) = plane->blend_property().GetEnumValueWithName(
            "Coverage");
        break;
      case DrmHwcBlending::kNone:
      default:
        std::tie(blend,
                 ret) = plane->blend_property().GetEnumValueWithName("None");
        break;
    }
    if (ret)
      ALOGE("Expected a valid blend mode on plane %d", plane->id());
  }

  return ret;
}

int PlanStageVop::MatchPlane(std::vector<DrmCompositionPlane> *composition_planes,
                   std::vector<PlaneGroup *> &plane_groups,
                   DrmCompositionPlane::Type type, DrmCrtc *crtc,
                   std::pair<int, std::vector<DrmHwcLayer*>> layers, int *zpos) {
  uint32_t combine_layer_count = 0;
  uint32_t layer_size = layers.second.size();
  bool b_yuv=false,b_scale=false,b_alpha=false,b_hdr2sdr=false,b_afbc=false;
  std::vector<PlaneGroup *> ::const_iterator iter;
  uint64_t rotation = 0;
  uint64_t alpha = 0xFF;
  uint16_t eotf = TRADITIONAL_GAMMA_SDR;
  bool bMulArea = layer_size > 0 ? true : false;

  //loop plane groups.
  for (iter = plane_groups.begin();
     iter != plane_groups.end(); ++iter) {
     ALOGD_IF(LogLevel(DBG_DEBUG),"line=%d,last zpos=%d,group(%" PRIu64 ") zpos=%d,group bUse=%d,crtc=0x%x,possible_crtcs=0x%x",
                  __LINE__, *zpos, (*iter)->share_id, (*iter)->zpos, (*iter)->bUse, (1<<crtc->pipe()), (*iter)->possible_crtcs);
      //find the match zpos plane group
      if(!(*iter)->bUse && !(*iter)->b_reserved)
      {
          ALOGD_IF(LogLevel(DBG_DEBUG),"line=%d,layer_size=%d,planes size=%zu",__LINE__,layer_size,(*iter)->planes.size());

          //find the match combine layer count with plane size.
          if(layer_size <= (*iter)->planes.size())
          {
              //loop layer
              for(std::vector<DrmHwcLayer*>::const_iterator iter_layer= layers.second.begin();
                  iter_layer != layers.second.end();++iter_layer)
              {
                  //reset is_match to false
                  (*iter_layer)->bMatch_ = false;

                  if(bMulArea
                      && !(*iter_layer)->bYuv_
                      && !(*iter_layer)->bScale_
                      && !((*iter_layer)->blending == DrmHwcBlending::kPreMult && (*iter_layer)->alpha != 0xFF)
                      && layer_size == 1
                      && layer_size < (*iter)->planes.size())
                  {
                      if(HasPlanesWithSize(crtc, layer_size, plane_groups))
                      {
                          ALOGD_IF(LogLevel(DBG_DEBUG),"Planes(%" PRIu64 ") don't need use multi area feature",(*iter)->share_id);
                          continue;
                      }
                  }

                  //loop plane
                  for(std::vector<DrmPlane*> ::const_iterator iter_plane=(*iter)->planes.begin();
                      !(*iter)->planes.empty() && iter_plane != (*iter)->planes.end(); ++iter_plane)
                  {
                      ALOGD_IF(LogLevel(DBG_DEBUG),"line=%d,crtc=0x%x,plane(%d) is_use=%d,possible_crtc_mask=0x%x",__LINE__,(1<<crtc->pipe()),
                              (*iter_plane)->id(),(*iter_plane)->is_use(),(*iter_plane)->get_possible_crtc_mask());
                      if(!(*iter_plane)->is_use() && (*iter_plane)->GetCrtcSupported(*crtc))
                      {
                          bool bNeed = false;

                          b_yuv  = (*iter_plane)->get_yuv();
                          if((*iter_layer)->bYuv_)
                          {
                              if(!b_yuv)
                              {
                                  ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) cann't support yuv",(*iter_plane)->id());
                                  continue;
                              }
                              else
                                  bNeed = true;
                          }

                          b_scale = (*iter_plane)->get_scale();
                          if((*iter_layer)->bScale_)
                          {
                              if(!b_scale)
                              {
                                  ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) cann't support scale",(*iter_plane)->id());
                                  continue;
                              }
                              else
                              {
                                  if((*iter_layer)->fHScaleMul_ >= 8.0 || (*iter_layer)->fVScaleMul_ >= 8.0 ||
                                      (*iter_layer)->fHScaleMul_ <= 0.125 || (*iter_layer)->fVScaleMul_ <= 0.125)
                                  {
                                      ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) cann't support scale factor(%f,%f)",
                                              (*iter_plane)->id(), (*iter_layer)->fHScaleMul_, (*iter_layer)->fVScaleMul_);
                                      continue;
                                  }
                                  else
                                      bNeed = true;
                              }
                          }

                          if ((*iter_layer)->blending == DrmHwcBlending::kPreMult)
                              alpha = (*iter_layer)->alpha;

#ifdef TARGET_BOARD_PLATFORM_RK3328
                          //disable global alpha feature for rk3328,since vop has bug on rk3328.
                          b_alpha = false;
#else
                          b_alpha = (*iter_plane)->alpha_property().id()?true:false;
#endif
                          if(alpha != 0xFF)
                          {
                              if(!b_alpha)
                              {
                                  ALOGV("layer id=%d, plane id=%d",(*iter_layer)->uId_,(*iter_plane)->id());
                                  ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) cann't support alpha,layer alpha=0x%x,alpha id=%d",
                                          (*iter_plane)->id(),(*iter_layer)->alpha,(*iter_plane)->alpha_property().id());
                                  continue;
                              }
                              else
                                  bNeed = true;
                          }

                          eotf = (*iter_layer)->uEOTF;
                          b_hdr2sdr = (*iter_plane)->get_hdr2sdr();
                          if(eotf != TRADITIONAL_GAMMA_SDR)
                          {
                              if(!b_hdr2sdr)
                              {
                                  ALOGV("layer id=%d, plane id=%d",(*iter_layer)->uId_,(*iter_plane)->id());
                                  ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) cann't support etof,layer eotf=%d,hdr2sdr=%d",
                                          (*iter_plane)->id(),(*iter_layer)->uEOTF,(*iter_plane)->get_hdr2sdr());
                                  continue;
                              }
                              else
                                  bNeed = true;
                          }

#if USE_AFBC_LAYER
                          b_afbc = (*iter_plane)->get_afbc();
                          if((*iter_layer)->bFbTarget_ && (*iter_plane)->get_afbc_prop())
                          {
                              if(!b_afbc)
                              {
                                  ALOGV("layer id=%d, plane id=%d",(*iter_layer)->uId_,(*iter_plane)->id());
                                  ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) cann't support afbc,layer", (*iter_plane)->id());
                                  continue;
                              }
                              else
                                  bNeed = true;
                          }
#else
                          UN_USED(b_afbc);

#endif

#ifdef TARGET_BOARD_PLATFORM_RK3288
                          int src_w,src_h;

                          src_w = (int)((*iter_layer)->source_crop.right - (*iter_layer)->source_crop.left);
#if RK_VIDEO_SKIP_LINE
                          if((*iter_layer)->iSkipLine_)
                          {
                              src_h = (int)((*iter_layer)->source_crop.bottom - (*iter_layer)->source_crop.top)/(*iter_layer)->iSkipLine_;
                          }
                          else
#endif
                              src_h = (int)((*iter_layer)->source_crop.bottom - (*iter_layer)->source_crop.top);

                          float src_size = (float)src_w * src_h;
                          if(src_size/(1536*2048) > 0.75)
                          {
                              bNeed = true;
                              ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) need by big area,src_size=%f,fbSize=%d",(*iter_plane)->id(),src_size,1536*2048);
                          }
#endif

//                          //Reserve some plane with no need for specific features in current layer.
//                          if(!bNeed && !bMulArea)
//                          {
//#if USE_AFBC_LAYER
//                              if(!(*iter_layer)->bFbTarget_ && b_afbc)
//                              {
//                                  if(HasGetNoAfbcUsablePlanes(crtc,plane_groups))
//                                  {
//                                      ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) don't need use afbc feature",(*iter_plane)->id());
//                                      continue;
//                                  }
//                              }
//#endif

//                              if(!(*iter_layer)->bYuv_ && b_yuv)
//                              {
//                                  if(HasGetNoYuvUsablePlanes(crtc,plane_groups))
//                                  {
//                                      ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) don't need use yuv feature",(*iter_plane)->id());
//                                      continue;
//                                  }
//                              }

//                              if(!(*iter_layer)->bScale_ && b_scale)
//                              {
//                                  if(HasGetNoScaleUsablePlanes(crtc,plane_groups))
//                                  {
//                                      ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) don't need use scale feature",(*iter_plane)->id());
//                                      continue;
//                                  }
//                              }

//                              if(alpha == 0xFF && b_alpha)
//                              {
//                                  if(HasGetNoAlphaUsablePlanes(crtc,plane_groups))
//                                  {
//                                      ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) don't need use alpha feature",(*iter_plane)->id());
//                                      continue;
//                                  }
//                              }

//                              if(eotf == TRADITIONAL_GAMMA_SDR && b_hdr2sdr)
//                              {
//                                  if(HasGetNoEotfUsablePlanes(crtc,plane_groups))
//                                  {
//                                      ALOGD_IF(LogLevel(DBG_DEBUG),"Plane(%d) don't need use eotf feature",(*iter_plane)->id());
//                                      continue;
//                                  }
//                              }
//                          }
                          rotation = 0;
                          if ((*iter_layer)->transform & DrmHwcTransform::kFlipH)
                              rotation |= 1 << DRM_REFLECT_X;
                          if ((*iter_layer)->transform & DrmHwcTransform::kFlipV)
                              rotation |= 1 << DRM_REFLECT_Y;
                          if ((*iter_layer)->transform & DrmHwcTransform::kRotate90)
                              rotation |= 1 << DRM_ROTATE_90;
                          else if ((*iter_layer)->transform & DrmHwcTransform::kRotate180)
                              rotation |= 1 << DRM_ROTATE_180;
                          else if ((*iter_layer)->transform & DrmHwcTransform::kRotate270)
                              rotation |= 1 << DRM_ROTATE_270;
                          if(rotation && !(rotation & (*iter_plane)->get_rotate()))
                              continue;

                          ALOGD_IF(LogLevel(DBG_DEBUG),"MatchPlane: match layer id=%d, plane=%d,,zops = %d",(*iter_layer)->uId_,
                              (*iter_plane)->id(),*zpos);
                          //Find the match plane for layer,it will be commit.
                          composition_planes->emplace_back(type, (*iter_plane), crtc, (*iter_layer)->iZpos_);
                          (*iter_layer)->bMatch_ = true;
                          (*iter_plane)->set_use(true);
                          composition_planes->back().set_zpos(*zpos);
                          combine_layer_count++;
                          break;

                      }
                  }
              }
              if(combine_layer_count == layer_size)
              {
                  ALOGD_IF(LogLevel(DBG_DEBUG),"line=%d all match",__LINE__);
                  //update zpos for the next time.
                   *zpos += 1;
                  (*iter)->bUse = true;
                  return 0;
              }
          }
          /*else
          {
              //1. cut out combine_layer_count to (*iter)->planes.size().
              //2. combine_layer_count layer assign planes.
              //3. extern layers assign planes.
              return false;
          }*/
      }

  }


  return -1;
}

int PlanStageVop::MatchPlanes(
    std::vector<DrmCompositionPlane> *composition,
    std::map<size_t, DrmHwcLayer *> &layers, DrmCrtc *crtc,
    std::vector<DrmPlane *> *planes) {

  DrmDevice *drm = crtc->getDrmDevice();

  std::vector<PlaneGroup *>& all_plane_groups = drm->GetPlaneGroups();
  std::vector<PlaneGroup *> plane_groups;
  for(auto &plane_group : all_plane_groups){
    for(auto &plane : *planes){
      uint64_t ret,share_id;
      std::tie(ret, share_id) = plane->share_id_property().value();
      if(share_id == plane_group->share_id){
        for(auto &p : plane_group->planes)
          p->set_use(false);
        plane_group->bUse = false;
        plane_groups.push_back(plane_group);
        break;
      }
    }
  }

  std::vector<DrmHwcLayer *> match_layers;
  for(auto &drmHwcLayer : layers){
      if(drmHwcLayer.second->bFbTarget_)
        continue;
      drmHwcLayer.second->iZpos_ = drmHwcLayer.first;
      match_layers.emplace_back(drmHwcLayer.second);
  }

  LayerMap layer_map;
  int ret = CombineLayer(layer_map, match_layers, planes->size());

  // Fill up the remaining planes
  int zpos = 0;
  for (auto i = layer_map.begin(); i != layer_map.end(); i = layer_map.erase(i)) {
    ret = MatchPlane(composition, plane_groups, DrmCompositionPlane::Type::kLayer,
                      crtc, std::make_pair(i->first, i->second),&zpos);
    // We don't have any planes left
    if (ret == -ENOENT)
      break;
    else if (ret) {
      ALOGE("Failed to emplace layer %d, dropping it", i->first);
      return ret;
    }
  }

  return 0;
}

}

