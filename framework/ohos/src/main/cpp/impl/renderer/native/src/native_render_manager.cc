/*
 *
 * Tencent is pleased to support the open source community by making
 * Hippy available.
 *
 * Copyright (C) 2019 THL A29 Limited, a Tencent company.
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "renderer/native_render_manager.h"
#include "renderer/native_render_provider_napi.h"
#include "renderer/native_render_provider_manager.h"
#include "renderer/api/hippy_view_provider.h"
#include <cstdint>
#include <iostream>
#include <utility>
#include "footstone/logging.h"
#include "footstone/macros.h"
#include "dom/root_node.h"
#include "oh_napi/ark_ts.h"
#include "oh_napi/oh_measure_text.h"

#define USE_C_MEASURE 1

constexpr char kId[] = "id";
constexpr char kPid[] = "pId";
constexpr char kIndex[] = "index";
constexpr char kName[] = "name";
constexpr char kWidth[] = "width";
constexpr char kHeight[] = "height";
constexpr char kLeft[] = "left";
constexpr char kTop[] = "top";
constexpr char kProps[] = "props";
constexpr char kDeleteProps[] = "deleteProps";
constexpr char kFontStyle[] = "fontStyle";
constexpr char kLetterSpacing[] = "letterSpacing";
constexpr char kColor[] = "kColor";
constexpr char kFontSize[] = "fontSize";
constexpr char kFontFamily[] = "fontFamily";
constexpr char kFontWeight[] = "fontWeight";
constexpr char kTextDecorationLine[] = "textDecorationLine";
constexpr char kTextShadowOffset[] = "textShadowOffset";
constexpr char kTextShadowRadius[] = "textShadowRadius";
constexpr char kTextShadowColor[] = "textShadowColor";
constexpr char kLineHeight[] = "lineHeight";
constexpr char kTextAlign[] = "textAlign";
constexpr char kText[] = "text";
constexpr char kEnableScale[] = "enableScale";
constexpr char kNumberOfLines[] = "numberOfLines";

#define MARK_DIRTY_PROPERTY(STYLES, FIND_STYLE, NODE) \
  do {                                                \
    FOOTSTONE_DCHECK(NODE != nullptr);                \
    if (STYLES->find(FIND_STYLE) != STYLES->end()) {  \
      NODE->MarkDirty();                              \
      return;                                         \
    }                                                 \
  } while (0)

namespace hippy {
inline namespace render {
inline namespace native {

static bool IsMeasureNode(const std::string &name) {
  return name == "Text" || name == "TextInput";
}

std::atomic<uint32_t> NativeRenderManager::unique_native_render_manager_id_{1};
footstone::utils::PersistentObjectMap<uint32_t, std::shared_ptr<hippy::NativeRenderManager>> NativeRenderManager::persistent_map_;

StyleFilter::StyleFilter() {
  // 过滤属性列表说明：
  // 1 传递到ts的属性需要过滤，否则业务页面填一堆渲染用不到的属性，序列化/反序列化的时候严重影响性能；
  // 2 这个列表是从Android复制过来的，ts不好自动收集，删了nativeBackgroundAndroid；
  styles_ = {
    "backgroundColor",
    "borderColor",
    "borderRadius",
    "borderStyle",
    "borderWidth",
    "borderBottomColor",
    "borderBottomStyle",
    "borderBottomWidth",
    "borderBottomLeftRadius",
    "borderBottomRightRadius",
    "borderLeftColor",
    "borderLeftStyle",
    "borderLeftWidth",
    "linearGradient",
    "borderRightColor",
    "borderRightStyle",
    "borderRightWidth",
    "shadowColor",
    "shadowOffset",
    "shadowOffsetX",
    "shadowOffsetY",
    "shadowOpacity",
    "shadowRadius",
    "borderTopColor",
    "borderTopStyle",
    "borderTopWidth",
    "borderTopLeftRadius",
    "borderTopRightRadius",
    "zIndex",
    "backgroundImage",
    "backgroundPositionX",
    "backgroundPositionY",
    "backgroundSize",
    "capInsets",
    "defaultSource",
    "resizeMode",
    "tintColor",
    "tintColorBlendMode",
    "src",
    "fakeBold",
    "backgroundColor",
    "breakStrategy",
    "color",
    "ellipsizeMode",
    "enableScale",
    "fontFamily",
    "fontSize",
    "fontStyle",
    "fontWeight",
    "letterSpacing",
    "lineHeight",
    "lineSpacingExtra",
    "lineSpacingMultiplier",
    "numberOfLines",
    "opacity",
    "text",
    "textAlign",
    "textDecorationColor",
    "textDecorationLine",
    "textDecorationStyle",
    "textShadowColor",
    "textShadowOffset",
    "textShadowRadius",
    "verticalAlign",
    "width",
    "height",
    "left",
    "top",
    "visibility",
    "transform",
    "opacity",
    "overflow",
    "direction",
  };
}

NativeRenderManager::NativeRenderManager() : RenderManager("NativeRenderManager"),
      serializer_(std::make_shared<footstone::value::Serializer>()) {
  id_ = unique_native_render_manager_id_.fetch_add(1);
}

NativeRenderManager::~NativeRenderManager() {
  ArkTS arkTs(ts_env_);
  arkTs.DeleteReference(ts_render_provider_ref_);
  ts_render_provider_ref_ = 0;
  ts_env_ = 0;
  
  if (enable_ark_c_api_) {
    NativeRenderProviderManager::RemoveRenderProvider(id_);
  }
}

void NativeRenderManager::SetRenderDelegate(napi_env ts_env, bool enable_ark_c_api, napi_ref ts_render_provider_ref,
    std::set<std::string> &custom_views, std::set<std::string> &custom_measure_views, std::map<std::string, std::string> &mapping_views,
    std::string &bundle_path) {
  persistent_map_.Insert(id_, shared_from_this());
  ts_env_ = ts_env;
  ts_render_provider_ref_ = ts_render_provider_ref;
  CallRenderDelegateSetIdMethod(ts_env_, ts_render_provider_ref_, "setInstanceId", id_);
  custom_measure_views_ = custom_measure_views;
  
  enable_ark_c_api_ = enable_ark_c_api;
  if (enable_ark_c_api) {
    c_render_provider_ = std::make_shared<NativeRenderProvider>(id_, bundle_path);
    c_render_provider_->SetTsEnv(ts_env);
    NativeRenderProviderManager::AddRenderProvider(id_, c_render_provider_);
    c_render_provider_->RegisterCustomTsRenderViews(ts_env, ts_render_provider_ref, custom_views, mapping_views);
  }
  
  NativeRenderManager::GetStyleFilter();
}

void NativeRenderManager::InitDensity(double density) {
  density_ = static_cast<float>(density);
}

void NativeRenderManager::CreateRenderNode(std::weak_ptr<RootNode> root_node,
                                           std::vector<std::shared_ptr<hippy::dom::DomNode>>&& nodes) {
  if (enable_ark_c_api_) {
    CreateRenderNode_C(root_node, std::move(nodes));
  } else {
    CreateRenderNode_TS(root_node, std::move(nodes));
  }
}

void NativeRenderManager::CreateRenderNode_TS(std::weak_ptr<RootNode> root_node, std::vector<std::shared_ptr<DomNode>> &&nodes) {
  auto root = root_node.lock();
  if (!root) {
    return;
  }
  
  uint32_t root_id = root->GetId();

  serializer_->Release();
  serializer_->WriteHeader();

  auto len = nodes.size();
  footstone::value::HippyValue::HippyValueArrayType dom_node_array;
  dom_node_array.resize(len);
  for (uint32_t i = 0; i < len; i++) {
    const auto& render_info = nodes[i]->GetRenderInfo();
    footstone::value::HippyValue::HippyValueObjectType dom_node;
    dom_node[kId] = footstone::value::HippyValue(render_info.id);
    dom_node[kPid] = footstone::value::HippyValue(render_info.pid);
    dom_node[kIndex] = footstone::value::HippyValue(render_info.index);
    dom_node[kName] = footstone::value::HippyValue(nodes[i]->GetViewName());

    if (IsMeasureNode(nodes[i]->GetViewName())) {
#if USE_C_MEASURE
      auto weak_node = nodes[i]->weak_from_this();
      MeasureFunction measure_function = [WEAK_THIS, root_node, weak_node](float width, LayoutMeasureMode width_measure_mode,
                                                                           float height, LayoutMeasureMode height_measure_mode,
                                                                           void *layoutContext) -> LayoutSize {
        DEFINE_SELF(NativeRenderManager)
        if (!self) {
          return LayoutSize{0, 0};
        }
        int64_t result;
        self->DoMeasureText(root_node, weak_node, self->DpToPx(width), static_cast<int32_t>(width_measure_mode),
                            self->DpToPx(height), static_cast<int32_t>(height_measure_mode), result);
        LayoutSize layout_result;
        layout_result.width = self->PxToDp(static_cast<float>((int32_t)(0xFFFFFFFF & (result >> 32))));
        layout_result.height = self->PxToDp(static_cast<float>((int32_t)(0xFFFFFFFF & result)));
        return layout_result;
      };
#else
      int32_t id =  footstone::check::checked_numeric_cast<uint32_t, int32_t>(nodes[i]->GetId());
      MeasureFunction measure_function = [WEAK_THIS, root_id, id](float width, LayoutMeasureMode width_measure_mode,
                                                                  float height, LayoutMeasureMode height_measure_mode,
                                                                  void *layoutContext) -> LayoutSize {
        DEFINE_SELF(NativeRenderManager)
        if (!self) {
          return LayoutSize{0, 0};
        }
        int64_t result;
        self->CallNativeMeasureMethod(root_id, id, self->DpToPx(width), static_cast<int32_t>(width_measure_mode),
                                      self->DpToPx(height), static_cast<int32_t>(height_measure_mode), result);
        LayoutSize layout_result;
        layout_result.width = self->PxToDp(static_cast<float>((int32_t)(0xFFFFFFFF & (result >> 32))));
        layout_result.height = self->PxToDp(static_cast<float>((int32_t)(0xFFFFFFFF & result)));
        return layout_result;
      };
#endif
      nodes[i]->GetLayoutNode()->SetMeasureFunction(measure_function);
    } else if (IsCustomMeasureNode(nodes[i]->GetViewName())) {
      int32_t id =  footstone::check::checked_numeric_cast<uint32_t, int32_t>(nodes[i]->GetId());
      MeasureFunction measure_function = [WEAK_THIS, root_id, id](float width, LayoutMeasureMode width_measure_mode,
                                                                  float height, LayoutMeasureMode height_measure_mode,
                                                                  void *layoutContext) -> LayoutSize {
        DEFINE_SELF(NativeRenderManager)
        if (!self) {
          return LayoutSize{0, 0};
        }
        int64_t result;
        self->CallNativeCustomMeasureMethod(root_id, id, self->DpToPx(width), static_cast<int32_t>(width_measure_mode),
                                            self->DpToPx(height), static_cast<int32_t>(height_measure_mode), result);
        LayoutSize layout_result;
        layout_result.width = static_cast<float>((int32_t)(0xFFFFFFFF & (result >> 32)));
        layout_result.height = static_cast<float>((int32_t)(0xFFFFFFFF & result));
        return layout_result;
      };
      nodes[i]->GetLayoutNode()->SetMeasureFunction(measure_function);
    }

    footstone::value::HippyValue::HippyValueObjectType props;
    // 样式属性
    auto style = nodes[i]->GetStyleMap();
    auto iter = style->begin();
    auto style_filter = NativeRenderManager::GetStyleFilter();
    while (iter != style->end()) {
      if (style_filter->Enable(iter->first)) {
        props[iter->first] = *(iter->second);
      }
      iter++;
    }
    // 用户自定义属性
    auto dom_ext = *nodes[i]->GetExtStyle();
    iter = dom_ext.begin();
    while (iter != dom_ext.end()) {
      props[iter->first] = *(iter->second);
      iter++;
    }
  
    dom_node[kProps] = props;
    dom_node_array[i] = dom_node;
  }
  serializer_->WriteValue(HippyValue(dom_node_array));
  std::pair<uint8_t *, size_t> buffer_pair = serializer_->Release();
  
  CallNativeMethod("createNode", root->GetId(), buffer_pair);
}

void NativeRenderManager::CreateRenderNode_C(std::weak_ptr<RootNode> root_node, std::vector<std::shared_ptr<DomNode>> &&nodes) {
  auto root = root_node.lock();
  if (!root) {
    return;
  }
  
  uint32_t root_id = root->GetId();
  auto len = nodes.size();
  std::vector<std::shared_ptr<HRCreateMutation>> mutations;
  mutations.resize(len);
  for (uint32_t i = 0; i < len; i++) {
    const auto& render_info = nodes[i]->GetRenderInfo();
    auto m = std::make_shared<HRCreateMutation>();
    m->tag_ = render_info.id;
    m->parent_tag_ = render_info.pid;
    m->index_ = render_info.index;
    m->view_name_ = nodes[i]->GetViewName();

    if (IsMeasureNode(nodes[i]->GetViewName())) {
      auto weak_node = nodes[i]->weak_from_this();
      MeasureFunction measure_function = [WEAK_THIS, root_node, weak_node](float width, LayoutMeasureMode width_measure_mode,
                                                                           float height, LayoutMeasureMode height_measure_mode,
                                                                           void *layoutContext) -> LayoutSize {
        DEFINE_SELF(NativeRenderManager)
        if (!self) {
          return LayoutSize{0, 0};
        }
        int64_t result;
        self->DoMeasureText(root_node, weak_node, self->DpToPx(width), static_cast<int32_t>(width_measure_mode),
                            self->DpToPx(height), static_cast<int32_t>(height_measure_mode), result);
        LayoutSize layout_result;
        layout_result.width = self->PxToDp(static_cast<float>((int32_t)(0xFFFFFFFF & (result >> 32))));
        layout_result.height = self->PxToDp(static_cast<float>((int32_t)(0xFFFFFFFF & result)));
        return layout_result;
      };
      nodes[i]->GetLayoutNode()->SetMeasureFunction(measure_function);
    } else if (IsCustomMeasureNode(nodes[i]->GetViewName())) {
      int32_t id =  footstone::check::checked_numeric_cast<uint32_t, int32_t>(nodes[i]->GetId());
      MeasureFunction measure_function = [WEAK_THIS, root_id, id](float width, LayoutMeasureMode width_measure_mode,
                                                                  float height, LayoutMeasureMode height_measure_mode,
                                                                  void *layoutContext) -> LayoutSize {
        DEFINE_SELF(NativeRenderManager)
        if (!self) {
          return LayoutSize{0, 0};
        }
        int64_t result;
        self->CallNativeCustomMeasureMethod(root_id, id, self->DpToPx(width), static_cast<int32_t>(width_measure_mode),
                                            self->DpToPx(height), static_cast<int32_t>(height_measure_mode), result);
        LayoutSize layout_result;
        layout_result.width = static_cast<float>((int32_t)(0xFFFFFFFF & (result >> 32)));
        layout_result.height = static_cast<float>((int32_t)(0xFFFFFFFF & result));
        return layout_result;
      };
      nodes[i]->GetLayoutNode()->SetMeasureFunction(measure_function);
    } else if (IsCustomMeasureCNode(nodes[i]->GetViewName())) {
      int32_t id =  footstone::check::checked_numeric_cast<uint32_t, int32_t>(nodes[i]->GetId());
      MeasureFunction measure_function = [WEAK_THIS, root_id, id](float width, LayoutMeasureMode width_measure_mode,
                                                                  float height, LayoutMeasureMode height_measure_mode,
                                                                  void *layoutContext) -> LayoutSize {
        DEFINE_SELF(NativeRenderManager)
        if (!self) {
          return LayoutSize{0, 0};
        }
        LayoutSize layout_result = self->CallNativeCustomMeasureMethod_C(root_id, static_cast<uint32_t>(id), width, width_measure_mode, height, height_measure_mode);
        return layout_result;
      };
      nodes[i]->GetLayoutNode()->SetMeasureFunction(measure_function);
    }

    footstone::value::HippyValue::HippyValueObjectType props;
    // 样式属性
    auto style = nodes[i]->GetStyleMap();
    auto iter = style->begin();
    auto style_filter = NativeRenderManager::GetStyleFilter();
    while (iter != style->end()) {
      if (style_filter->Enable(iter->first)) {
        props[iter->first] = *(iter->second);
      }
      iter++;
    }
    // 用户自定义属性
    auto dom_ext = *nodes[i]->GetExtStyle();
    iter = dom_ext.begin();
    while (iter != dom_ext.end()) {
      props[iter->first] = *(iter->second);
      iter++;
    }
  
    m->props_ = props;
    auto parentNode = nodes[i]->GetParent();
    if (parentNode && parentNode->GetViewName() == "Text") {
      m->is_parent_text_ = true;
    }
    mutations[i] = m;
  }
  
  c_render_provider_->CreateNode(root_id, mutations);
}

void NativeRenderManager::UpdateRenderNode(std::weak_ptr<RootNode> root_node,
                                           std::vector<std::shared_ptr<DomNode>>&& nodes) {
  if (enable_ark_c_api_) {
    UpdateRenderNode_C(root_node, std::move(nodes));
  } else {
    UpdateRenderNode_TS(root_node, std::move(nodes));
  }
}

void NativeRenderManager::UpdateRenderNode_TS(std::weak_ptr<RootNode> root_node, std::vector<std::shared_ptr<DomNode>> &&nodes) {
  auto root = root_node.lock();
  if (!root) {
    return;
  }

  for (const auto &n : nodes) {
    auto node = root->GetNode(n->GetId());
    if (node == nullptr)
      continue;
    if (n->GetViewName() == "Text") {
      MarkTextDirty(root_node, n->GetId());
    }
  }

  serializer_->Release();
  serializer_->WriteHeader();

  auto len = nodes.size();
  footstone::value::HippyValue::HippyValueArrayType dom_node_array;
  dom_node_array.resize(len);
  for (uint32_t i = 0; i < len; i++) {
    const auto &render_info = nodes[i]->GetRenderInfo();
    footstone::value::HippyValue::HippyValueObjectType dom_node;
    dom_node[kId] = footstone::value::HippyValue(render_info.id);
    dom_node[kPid] = footstone::value::HippyValue(render_info.pid);
    dom_node[kIndex] = footstone::value::HippyValue(render_info.index);
    dom_node[kName] = footstone::value::HippyValue(nodes[i]->GetViewName());

    footstone::value::HippyValue::HippyValueObjectType diff_props;
    footstone::value::HippyValue::HippyValueArrayType del_props;
    auto diff = nodes[i]->GetDiffStyle();
    if (diff) {
      auto iter = diff->begin();
      while (iter != diff->end()) {
        FOOTSTONE_DCHECK(iter->second != nullptr);
        if (iter->second) {
          diff_props[iter->first] = *(iter->second);
        }
        iter++;
      }
    }
    auto del = nodes[i]->GetDeleteProps();
    if (del) {
      auto iter = del->begin();
      while (iter != del->end()) {
        del_props.emplace_back(footstone::value::HippyValue(*iter));
        iter++;
      }
    }
    dom_node[kProps] = diff_props;
    dom_node[kDeleteProps] = del_props;
    dom_node_array[i] = dom_node;
  }
  serializer_->WriteValue(HippyValue(dom_node_array));
  std::pair<uint8_t *, size_t> buffer_pair = serializer_->Release();

  CallNativeMethod("updateNode", root->GetId(), buffer_pair);
}

void NativeRenderManager::UpdateRenderNode_C(std::weak_ptr<RootNode> root_node, std::vector<std::shared_ptr<DomNode>> &&nodes) {
  auto root = root_node.lock();
  if (!root) {
    return;
  }

  for (const auto &n : nodes) {
    auto node = root->GetNode(n->GetId());
    if (node == nullptr)
      continue;
    if (n->GetViewName() == "Text") {
      MarkTextDirty(root_node, n->GetId());
    }
  }

  uint32_t root_id = root->GetId();
  auto len = nodes.size();
  std::vector<std::shared_ptr<HRUpdateMutation>> mutations;
  mutations.resize(len);
  for (uint32_t i = 0; i < len; i++) {
    const auto &render_info = nodes[i]->GetRenderInfo();
    auto m = std::make_shared<HRUpdateMutation>();
    m->tag_ = render_info.id;
    m->parent_tag_ = render_info.pid;
    m->index_ = render_info.index;
    m->view_name_ = nodes[i]->GetViewName();

    footstone::value::HippyValue::HippyValueObjectType diff_props;
    std::vector<std::string> del_props;
    auto diff = nodes[i]->GetDiffStyle();
    if (diff) {
      auto iter = diff->begin();
      while (iter != diff->end()) {
        FOOTSTONE_DCHECK(iter->second != nullptr);
        if (iter->second) {
          diff_props[iter->first] = *(iter->second);
        }
        iter++;
      }
    }
    auto del = nodes[i]->GetDeleteProps();
    if (del) {
      auto iter = del->begin();
      while (iter != del->end()) {
        del_props.emplace_back(*iter);
        iter++;
      }
    }
    m->props_ = diff_props;
    m->delete_props_ = del_props;
    mutations[i] = m;
  }
  c_render_provider_->UpdateNode(root_id, mutations);
}

void NativeRenderManager::MoveRenderNode(std::weak_ptr<RootNode> root_node,
                                         std::vector<std::shared_ptr<DomNode>> &&nodes) {
  if (enable_ark_c_api_) {
    MoveRenderNode_C(root_node, std::move(nodes));
  } else {
    MoveRenderNode_TS(root_node, std::move(nodes));
  }
}

void NativeRenderManager::MoveRenderNode_TS(std::weak_ptr<RootNode> root_node, std::vector<std::shared_ptr<DomNode>> &&nodes) {
  auto root = root_node.lock();
  if (!root) {
    return;
  }

  serializer_->Release();
  serializer_->WriteHeader();

  auto len = nodes.size();
  footstone::value::HippyValue::HippyValueArrayType dom_node_array;
  dom_node_array.resize(len);
  uint32_t pid;
  for (uint32_t i = 0; i < len; i++) {
    const auto &render_info = nodes[i]->GetRenderInfo();
    footstone::value::HippyValue::HippyValueObjectType dom_node;
    dom_node[kId] = footstone::value::HippyValue(render_info.id);
    dom_node[kPid] = footstone::value::HippyValue(render_info.pid);
    dom_node[kIndex] = footstone::value::HippyValue(render_info.index);
    dom_node_array[i] = dom_node;
    pid = render_info.pid;
  }
  serializer_->WriteValue(HippyValue(dom_node_array));
  std::pair<uint8_t *, size_t> buffer_pair = serializer_->Release();

  CallRenderDelegateMoveNodeMethod(ts_env_, ts_render_provider_ref_, "moveNode", root->GetId(), pid, buffer_pair);
}

void NativeRenderManager::MoveRenderNode_C(std::weak_ptr<RootNode> root_node, std::vector<std::shared_ptr<DomNode>> &&nodes) {
  auto root = root_node.lock();
  if (!root) {
    return;
  }

  uint32_t root_id = root->GetId();
  auto len = nodes.size();
  auto m = std::make_shared<HRMoveMutation>();
  std::vector<HRMoveNodeInfo> node_infos;
  for (uint32_t i = 0; i < len; i++) {
    const auto &render_info = nodes[i]->GetRenderInfo();
    m->parent_tag_ = render_info.pid;
    node_infos.push_back(HRMoveNodeInfo(render_info.id, render_info.index));
  }
  m->node_infos_ = node_infos;
  c_render_provider_->MoveNode(root_id, m);
}

void NativeRenderManager::DeleteRenderNode(std::weak_ptr<RootNode> root_node,
                                           std::vector<std::shared_ptr<DomNode>>&& nodes) {
  if (enable_ark_c_api_) {
    DeleteRenderNode_C(root_node, std::move(nodes));
  } else {
    DeleteRenderNode_TS(root_node, std::move(nodes));
  }
}

void NativeRenderManager::DeleteRenderNode_TS(std::weak_ptr<RootNode> root_node, std::vector<std::shared_ptr<DomNode>> &&nodes) {
  auto root = root_node.lock();
  if (!root) {
    return;
  }

  std::vector<uint32_t> ids;
  ids.resize(nodes.size());
  for (size_t i = 0; i < nodes.size(); i++) {
    ids[i] = nodes[i]->GetRenderInfo().id;
  }

  CallRenderDelegateDeleteNodeMethod(ts_env_, ts_render_provider_ref_, "deleteNode", root->GetId(), ids);
}

void NativeRenderManager::DeleteRenderNode_C(std::weak_ptr<RootNode> root_node, std::vector<std::shared_ptr<DomNode>> &&nodes) {
  auto root = root_node.lock();
  if (!root) {
    return;
  }

  uint32_t root_id = root->GetId();
  auto len = nodes.size();
  std::vector<std::shared_ptr<HRDeleteMutation>> mutations;
  mutations.resize(len);
  for (uint32_t i = 0; i < len; i++) {
    const auto &render_info = nodes[i]->GetRenderInfo();
    auto m = std::make_shared<HRDeleteMutation>();
    m->tag_ = render_info.id;
    mutations[i] = m;
  }
  c_render_provider_->DeleteNode(root_id, mutations);
}

void NativeRenderManager::UpdateLayout(std::weak_ptr<RootNode> root_node,
                                       const std::vector<std::shared_ptr<DomNode>>& nodes) {
  if (enable_ark_c_api_) {
    UpdateLayout_C(root_node, std::move(nodes));
  } else {
    UpdateLayout_TS(root_node, std::move(nodes));
  }
}

void NativeRenderManager::UpdateLayout_TS(std::weak_ptr<RootNode> root_node, const std::vector<std::shared_ptr<DomNode>> &nodes) {
  auto root = root_node.lock();
  if (!root) {
    return;
  }

  serializer_->Release();
  serializer_->WriteHeader();

  auto len = nodes.size();
  footstone::value::HippyValue::HippyValueArrayType dom_node_array;
  dom_node_array.resize(len);
  for (uint32_t i = 0; i < len; i++) {
    footstone::value::HippyValue::HippyValueObjectType dom_node;
    dom_node[kId] = footstone::value::HippyValue(nodes[i]->GetId());
    const auto &result = nodes[i]->GetRenderLayoutResult();
    dom_node[kWidth] = footstone::value::HippyValue(DpToPx(result.width));
    dom_node[kHeight] = footstone::value::HippyValue(DpToPx(result.height));
    dom_node[kLeft] = footstone::value::HippyValue(DpToPx(result.left));
    dom_node[kTop] = footstone::value::HippyValue(DpToPx(result.top));
    if (IsMeasureNode(nodes[i]->GetViewName())) {
      dom_node["paddingLeft"] = footstone::value::HippyValue(DpToPx(result.paddingLeft));
      dom_node["paddingTop"] = footstone::value::HippyValue(DpToPx(result.paddingTop));
      dom_node["paddingRight"] = footstone::value::HippyValue(DpToPx(result.paddingRight));
      dom_node["paddingBottom"] = footstone::value::HippyValue(DpToPx(result.paddingBottom));
    }
    dom_node_array[i] = dom_node;
  }
  serializer_->WriteValue(HippyValue(dom_node_array));
  std::pair<uint8_t *, size_t> buffer_pair = serializer_->Release();

  CallNativeMethod("updateLayout", root->GetId(), buffer_pair);
}

void NativeRenderManager::UpdateLayout_C(std::weak_ptr<RootNode> root_node, const std::vector<std::shared_ptr<DomNode>> &nodes) {
  auto root = root_node.lock();
  if (!root) {
    return;
  }

  uint32_t root_id = root->GetId();
  auto len = nodes.size();
  std::vector<std::shared_ptr<HRUpdateLayoutMutation>> mutations;
  mutations.resize(len);
  for (uint32_t i = 0; i < len; i++) {
    const auto &result = nodes[i]->GetRenderLayoutResult();
    auto m = std::make_shared<HRUpdateLayoutMutation>();
    m->tag_ = nodes[i]->GetId();
    m->left_ = result.left;
    m->top_ = result.top;
    m->width_ = result.width;
    m->height_ = result.height;
    if (IsMeasureNode(nodes[i]->GetViewName())) {
      m->padding_left_ = result.paddingLeft;
      m->padding_top_ = result.paddingTop;
      m->padding_right_ = result.paddingRight;
      m->padding_bottom_ = result.paddingBottom;
    }
    mutations[i] = m;
  }
  c_render_provider_->UpdateLayout(root_id, mutations);
}

void NativeRenderManager::MoveRenderNode(std::weak_ptr<RootNode> root_node,
                                         std::vector<int32_t>&& moved_ids,
                                         int32_t from_pid,
                                         int32_t to_pid,
                                         int32_t index) {
  if (enable_ark_c_api_) {
    MoveRenderNode_C(root_node, std::move(moved_ids), from_pid, to_pid, index);
  } else {
    MoveRenderNode_TS(root_node, std::move(moved_ids), from_pid, to_pid, index);
  }
}

void NativeRenderManager::MoveRenderNode_TS(std::weak_ptr<RootNode> root_node, std::vector<int32_t> &&moved_ids, int32_t from_pid,
                       int32_t to_pid, int32_t index) {
  auto root = root_node.lock();
  if (!root) {
    return;
  }

  CallRenderDelegateMoveNodeMethod(ts_env_, ts_render_provider_ref_, "moveNode2", root->GetId(), moved_ids, to_pid,
                                   from_pid, index);
}

void NativeRenderManager::MoveRenderNode_C(std::weak_ptr<RootNode> root_node, std::vector<int32_t> &&moved_ids, int32_t from_pid,
                      int32_t to_pid, int32_t index) {
  auto root = root_node.lock();
  if (!root) {
    return;
  }

  uint32_t root_id = root->GetId();
  auto m = std::make_shared<HRMove2Mutation>();
  std::vector<uint32_t> tags;
  for (uint32_t i = 0; i < moved_ids.size(); i++) {
    tags.push_back((uint32_t)moved_ids[i]);
  }
  m->tags_ = tags;
  m->to_parent_tag_ = (uint32_t)to_pid;
  m->from_parent_tag_ = (uint32_t)from_pid;
  m->index_ = index;
  c_render_provider_->MoveNode2(root_id, m);
}

void NativeRenderManager::EndBatch(std::weak_ptr<RootNode> root_node) {
  if (enable_ark_c_api_) {
    EndBatch_C(root_node);
  } else {
    EndBatch_TS(root_node);
  }
}

void NativeRenderManager::EndBatch_TS(std::weak_ptr<RootNode> root_node) {
  auto root = root_node.lock();
  if (root) {
    CallNativeMethod("endBatch", root->GetId());
  }
}

void NativeRenderManager::EndBatch_C(std::weak_ptr<RootNode> root_node) {
  auto root = root_node.lock();
  if (root) {
    uint32_t root_id = root->GetId();
    c_render_provider_->EndBatch(root_id);
  }
}

void NativeRenderManager::BeforeLayout(std::weak_ptr<RootNode> root_node){}

void NativeRenderManager::AfterLayout(std::weak_ptr<RootNode> root_node) {
  // 更新布局信息前处理事件监听
  HandleListenerOps(root_node, event_listener_ops_, "updateEventListener");
}

void NativeRenderManager::AddEventListener(std::weak_ptr<RootNode> root_node,
                                           std::weak_ptr<DomNode> dom_node, const std::string& name) {
  auto node = dom_node.lock();
  if (node) {
    event_listener_ops_[node->GetId()].emplace_back(ListenerOp(true, dom_node, name));
  }
}

void NativeRenderManager::RemoveEventListener(std::weak_ptr<RootNode> root_node,
                                              std::weak_ptr<DomNode> dom_node, const std::string& name) {
  auto node = dom_node.lock();
  if (node) {
    event_listener_ops_[node->GetId()].emplace_back(ListenerOp(false, dom_node, name));
  }
}

void NativeRenderManager::CallFunction(std::weak_ptr<RootNode> root_node,
                                       std::weak_ptr<DomNode> domNode, const std::string& name, const DomArgument& param,
                                        uint32_t cb_id) {
  if (enable_ark_c_api_) {
    CallFunction_C(root_node, domNode, name, param, cb_id);
  } else {
    CallFunction_TS(root_node, domNode, name, param, cb_id);
  }
}

void NativeRenderManager::CallFunction_TS(std::weak_ptr<RootNode> root_node, std::weak_ptr<DomNode> domNode,
                                          const std::string &name, const DomArgument &param, uint32_t cb_id) {
  auto root = root_node.lock();
  if (!root) {
    return;
  }

  std::shared_ptr<DomNode> node = domNode.lock();
  if (node == nullptr) {
    FOOTSTONE_LOG(ERROR) << "CallJs bad node";
    return;
  }

  std::vector<uint8_t> param_bson;
  param.ToBson(param_bson);

  void *new_buffer = malloc(param_bson.size());
  FOOTSTONE_DCHECK(new_buffer != nullptr);
  if (!new_buffer) {
    FOOTSTONE_LOG(ERROR) << "NativeRenderManager::CallFunction, malloc fail, size = " << param_bson.size();
    return;
  }
  memcpy(new_buffer, param_bson.data(), param_bson.size());
  auto buffer_pair = std::make_pair(reinterpret_cast<uint8_t *>(new_buffer), param_bson.size());

  CallRenderDelegateCallFunctionMethod(ts_env_, ts_render_provider_ref_, "callUIFunction", root->GetId(), node->GetId(),
                                       cb_id, name, buffer_pair);
}

void NativeRenderManager::CallFunction_C(std::weak_ptr<RootNode> root_node, std::weak_ptr<DomNode> domNode,
                                         const std::string &name, const DomArgument &param, uint32_t cb_id) {
  auto root = root_node.lock();
  if (!root) {
    return;
  }

  std::shared_ptr<DomNode> node = domNode.lock();
  if (node == nullptr) {
    FOOTSTONE_LOG(ERROR) << "CallJs bad node";
    return;
  }

  HippyValue hippy_value;
  param.ToObject(hippy_value);
  
  HippyValueArrayType params;
  if (hippy_value.IsArray()) {
    hippy_value.ToArray(params);
  }

  if (name == "getScreenShot") {
    HippyValueObjectType hiType;
    hiType["screenShot"] = HippyValue(
        "/9j/4AAQSkZJRgABAQAAAQABAAD/"
        "4gIoSUNDX1BST0ZJTEUAAQEAAAIYAAAAAAQwAABtbnRyUkdCIFhZWiAAAAAAAAAAAAAAAABhY3NwAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAQAA9tYAAQAAAADTLQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAlkZXNjAAAA8AAAAHRyWFlaAAABZAAAABRnWFlaAAABeAAAABRiWFlaAAABjAAAABRyVFJDAA"
        "ABoAAAAChnVFJDAAABoAAAAChiVFJDAAABoAAAACh3dHB0AAAByAAAABRjcHJ0AAAB3AAAADxtbHVjAAAAAAAAAAEA"
        "AAAMZW5VUwAAAFgAAAAcAHMAUgBHAEIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAFhZWiAAAAAAAABvogAAOPUAAAOQWFlaIAAAAAAAAGKZ"
        "AAC3hQAAGNpYWVogAAAAAAAAJKAAAA+"
        "EAAC2z3BhcmEAAAAAAAQAAAACZmYAAPKnAAANWQAAE9AAAApbAAAAAAAAAABYWVogAAAAAAAA9tYAAQAAAADTLW1sd"
        "WMAAAAAAAAAAQAAAAxlblVTAAAAIAAAABwARwBvAG8AZwBsAGUAIABJAG4AYwAuACAAMgAwADEANv/"
        "bAEMAAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBA"
        "f/"
        "bAEMBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBA"
        "f/AABEICRgE7AMBIgACEQEDEQH/xAAfAAEAAwEBAQEAAwEAAAAAAAAABwgJCgYFBAECAwv/"
        "xABmEAEAAAYBAQUGAwMDCw8GCQ0AAQIDBAUGBwgJERIX1RMUVVaUpQoVIRYiMUaFxSMkMjg5QXd4trfXGBkaJTY3Ql"
        "FhdHaBl7W4MzpYkZa0NUNXWXN1mLPWJjRFUlNxcpKTsbLB0f/EAB0BAQABBAMBAAAAAAAAAAAAAAAFAwQGBwECCAn/"
        "xABQEQEAAgEDAgEGDAIFCwMCBQUAAQIDBAURBhIhBxMXVpTTFBYYIjFRVFVhkrHVQZMVIzJGhQg2N3GBhpG1tsXGM1"
        "KhNEI1U2SC8HJzdMHh/"
        "9oADAMBAAIRAxEAPwDsoAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAZRdIfa18PdYnU7yJ0t6XxnyVq218cYfe8zk9j2ifV5tev6Gg7hhdOyFKxlxObvslCre3uboXdl7xZ0pIWtGr"
        "CvNTq+CSbV1xY9hv/dfeqT/"
        "AKC9SH+fPQU1tmi0+"
        "p0O8Z81bWyaPTY8uCYtasVva2SJmYiYi0cVjwtzCN1upy4NTt2PHMRTUZ7Y8sTWJmaxFOOJnxrPjPjDtOAQqSZRY7t"
        "bOHsj1+1Oz4pcZ8l0+Rqe4ZTTY7xUn1f9io32L0m73etdQllzcc77pUsbOezp9+M9t73NLGanLR754aB8783ce9N/"
        "EO+84cq5WthtA45wc+d2G9tbSpf3s9Oa5t8fj8djbKlGWa8yuZy17YYfFWsalGnXyN9a0qtehSnnrU+QnV//ADma6/"
        "w6bf8A+H7NuvbnfiDjTnzh/kDh7mHGUctxrvmAr4fa7Stfz4uNOzkrUb62yNtk6dSlPjb/"
        "AA+Rs7LL43ISzw9yyFja3MYTwpRkmnt00Wj0WbaYiuWuHU6DRanVRS3dkmctreenF38xW00r8ys/"
        "Ni3Hhwi9DqdRqcevmbUnJh1epwYO6vbSIx1r5uL9vjMRafnT9Mwrn0L9oTwD2g2mbft/"
        "B8u54mvoObscJt+o8g4bGYXasLNmLe7ucDkq1HBZ3Z8Jc4rO0sdk4Y65ss5c1fa4u/"
        "oXlvaVaMJJ7zKMdCfQz0z9Dei7ZqnTjUy+Zobnn7XLbnuGy7Pjts2XN3mLtq1HC42+"
        "yGGsMRiLawwNpkLz8sx9jibL2Ucne3dzG5ur2rcT3TvM5hcfd21hf5fF2N9eRpwtLK8yFpbXd1GrU9jShbW9atJWrx"
        "qVv6lThSkm8dT+py9836IzWxpfheaNDXNGl7o8zGaP62KxWvd3cfw7+7t58ezt7vHle6bz/"
        "mMfwmcc5+"
        "P6ycf9jnmeOPx44548O7njw4fUHk9y33RuOsPPsPIO6anomAp1IUZ85uWxYfWMPJVmlmmlpT5PN3ljZS1JpZZpoSRr"
        "wmjLLNGEO6EXzdC5W4u5UsrnI8Yck6DyPj7OeSnd3+hbjru4WVrPU8Xs5Lm617I5GhQnqeCfwS1aksZ/"
        "DN4YR8Me627L9k5Oy3ZE8Tftnsifqm3HHP4c8qvdXu7e6vdMcxXmO7j6+Pp4e+"
        "B8uvnMLa39HFXOXxdvlLmEkbfG18haUr+vCrNNLSjRs6laW4qwqTSTyyRkpzeOaWaEvfGWPdxETP0RM/"
        "6vF2fUHit55K454xxlPN8lb/pXHmGrVZqNLL7ztWC1PGVa0sITTUqd/n7/"
        "AB9rPVllmlmmpy1YzQhGEYw7owNH5J465OxdTOca79pXIWFpVYUauX0facFtmLp1poRmlo1MhgL/"
        "ACFpJVjLLNGFOatCeMJYxhDuhFz2X7e/st2c8d/bPbz9Xdxxz+HLr3V7u3ur3cc9vMd3H18fTw9qPyX1/"
        "Y4y2qXuSvbTH2dKMkKt3fXNG0tqcak8tOnCpXrz06UkZ6k8lOSE08PFPNLLL3zTQhH/"
        "ADx2VxeXoTXWJyVhlLaSrNQnuMdeW97QlrSSyTzUZqttUq05asslSnPNTjNCeEtSSaMIQmljHjieOeJ4+jnjw5+"
        "rl2fvB8jI7BgcPUpUcvm8Riq1aSNSjSyOSsrGpVpwm8MZ6UlzWpTVJITQjLGaWEZYTQ7u/"
        "v8A0IiZ8IiZn6ojkfXAcDIfr97Yfhfs+OYNZ4b5G4t5Q3jN7RxriOTbTK6TPqkuKt8VmNo3DVaOPuIZ3O4y7/"
        "MKV3pt9c1Y07ee292urXwVpqvtpKdHJfxOnSt3w8XT51Awl74eKMtTjmaMIf34wljt8sIxhD+EIzSwj/Dvh/"
        "FQP8Qxb2952mfTDaXdCjdWt1wJw5b3NtcUpK9vcW9fn/"
        "mWlWoV6NWWanWo1qc01OrSqSzSVJJppJ5YyxjCPWRP0cdIlSSanU6VunCpTnhGWeSfg7jGeSaWP6Rlmlm1eMs0Iw/"
        "SMIwjCLLMmn2XQ7dteo1Wi1Gpy67DkyWtj1VsUVnHakT83xjx744iOOOJ+"
        "uOIKuXctTq9diwanDhx6XJSkRfBF5mL1mfp8J8O2fp+nmGdPTp29fQB1AbFjtQyW0bnwRsuWr0bLGU+bsDi8Drd/"
        "f1ppZZLenuutbBter4qnPGMYUrvbMjrVvVnhClLNCvUo0am0VOpTrU6dajUkq0qsklSlVpzyz06lOeWE0lSnPLGMs8"
        "k8sYTSTyxjLNLGEYRjCMIudrtZ+x36X9r6beVueOn/"
        "jLV+EuXuHdO2DkqrZ8eY231jSt41jUMdcZ7adfyul4yShrtjlPyGzyd9gMrgMbjMhWzNG1sMlUvrG5hLa/"
        "x+"
        "HL6rt15u6bOSeDN9y95sGQ6aM7qNjqGYyNepcX1LjXkCy2Cpres1bmvNPWu6OrZbTtktcZPUqze4YG8wuEoU6NjirK"
        "Raavb9Bn26+"
        "67XbPTHgy0xazSama2vhnJNa4748leO6k2tWvFu6Z7pnmvbNVbT6vV4tZXQ66MVr5cdsmnz4YtWuSKRM2retvotEVt"
        "PMcRHERxPMWdFIIk3jn7gjjHJU8NyTzXxJx7mK0tOalit45H07U8lVlqwlmpTU7HPZnH3U8tWWaWanGWlGE8JpYyxj"
        "CMO+ArS957aVte3HPFazaePr4iJnhLWtWsc2tFY+u0xEf8ZS2Pi69smu7diLPYNUz+"
        "F2fA5CSNSwzevZSxzWIvqcJoyxqWeSxte5s7mSE0Iyxmo1p5YTQjCMe+"
        "EYPtOJiYmYmJiYniYnwmJj6YmP4TDmJiY5ieYnxiY+iY+sHxdh2TXdSxN3n9rz+"
        "F1nBWEsJ77NbDlLHC4mykmmhLLPd5LJV7azt5ZpowlhNWrSQjNGEIR749yPtH5+4I5NyU+"
        "G425r4k5BzFOWpPUxWj8kadtmSpyUoRjVnnscDmb+6klpQhGNSaalCEkIRjNGEIRcxS9qzeKWmtf7VorM1r/rtEcR/"
        "tl1m1YmKzasWn6ImYiZ/1RM8z/"
        "sS2A6uwI537mLiPimS2q8o8p8c8bU72WM9nU37d9Z06S7klmmlmntp9hyeOlryyzSzSxmpRnhCaWaEY98Ivr6XyDoX"
        "JGJ/PuO931DfcH7T2P51pey4bacT7bwwm9l+Y4O9vrP2nhjCb2ftvF4Ywj3d0e9383kikZJpeKTPEX7bdkz9UW445/"
        "wBrr3V7u3ur3fT28x3cfXxzz/"
        "8AD14hPqC2nXsJxByzZ5PYsLiMlW4r3yvZ2t9l7GwvasJ9YzNKhWtqFxcUq9SE9enPTpVKUsYTVqc0kkYzyRhDmL/"
        "DHbzaWtLrZ/bHcbe28dTpx/Lv2l2CnR8fhl53979y/NLuXxeHxWvvPsO/"
        "u76Htf400hp9ttqNu1+4RkmvwK2mrGLzc2nN8Iyxj5i/dHb2c90/"
        "Mvzxx4c8xaZtZGLV6XSzSJ+ExmnznfEeb8zj7+Jr2z3d/"
        "wBEfOrx9Pj9DrgHyMTsGAz0tebBZzEZqW1jTluZsTkrLIy281WE8aUK8bOvWhSjVhTqRpwqeGM8JJ4y9/"
        "hm7vro2YmJ4mJifqnwlexMT4xPMfXAA4AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAHzM1msPreHyuw7DlcbgsBgsde5jN5vMX1tjMTh8TjbapeZHJ5TI3tWjZ2GPsLSjWury8uq1K3trelUrVqklOSa"
        "aHO/1C/iSulbjLZ8jq/"
        "CfGO+dQkMTc1bS526nlbHjXRcjVpTRknn13JZbF7Ds2VtpKks8nvl1qGKtLiEstfHV7+"
        "0q07ma33bTcGdW3UX0f3nGHSnZWGaq3+02GX5X1ajn6eD27c9Ewdtc31vq+rwvpKGIyctfYZcZmcxi7rMYu+"
        "yNLCWeNxlPKz31zjLj9XZs9lfwR0ecI6ZV3HjXTd16idhwWOzXJ+/"
        "bXgcVsmTw+"
        "wZO1pXd7pmnXOUtryTAa3rU9WGH8eHls62yXFjNmstGpUr2trYTuixbRg0Xw3cJtrM2TNbFh27Dm81ataRE2zai9Zj"
        "JSs88U7fCfDjv5t5qL1N9wy6n4NpO3T4q44vk1eXH5yJtaeIx4qz8y0xEc25/HxrxHdTfp0/"
        "Ej9LXKW14zU+bOMd26ePzm7o2VpuFfOY/"
        "kfQsdXr1JaVKfZcrjcTrWw4aznqTySTZCjqmTsLSEZrjJ3OPsaVa7p9E+OyOPy+Psctib6zymKylna5HGZPHXVC9x+"
        "Rx97QkubK+sb22nq213Z3dtVp3FrdW9SpQuKFSSrSnnpzyzR5wfxAXRD0+T9I+e6odT4+1Dj/"
        "lvi7adIo3ef1DCYzW629a5uOz43Tb3A7Na4q3tLXO3VjdZzH5zGZW9o18vjqOIurK2u5Mde3dvPZT8PzyptfJnZ06p"
        "jtrvLrITcUckb1xXrt7ezz1rifVMVSwO04Wz9vUjGepa4Snt9XAY2TvjJZ4vFWWPowkoWlOSWtr9Ft+"
        "ba8e77bjy6akan4JqdJlyTlil5p3xfFktM3tHE157pnmLxMRTst3U9JqdXj1t9v1l8ea04fhGHPjpGObV7u2a3pHFY"
        "8YtxxETHbPM2i0cbbAxh7dXq4zvS10SZjEaLl6+F5I5+2CnxHr+UsK81vlcFrV9jL/K8gbBjqskZKtC4k16y/"
        "Zi3vrapTvcXkNtsMpZVKdzZ06tOF0elya3VYNJi4i+fJXHEz9FYmfnXn+"
        "PFKxNpiPHiJ48UlqM9NNgy58nPZipNpiPpnj6Kx+NpmKx+Mo16vvxA/"
        "Sd027pmeNeNtb2XqR3PW7yvjtivdNy+K1zjfF5S0qzUbzE0t8vqGYq5zIWdWWMlzW1zW8xgpJ4T0Jc5Pd0bi2oxN0/"
        "fiUemLkfa8drHN3Eu9dP9rlrmlaW+"
        "50s9Zcn6di6tWaWWStstfGYPV9lxljGaMZZrzG6vnZaEYy1LuS2tIVrqh8HsHuzR4fwXTvrXVxzRx9rm/"
        "cp8u1sjluO7fdMLZZ7HcdcfY/I3WJw9/hsPlaF1j6Oz7dXsLrYJ9lhSq31vrd5gsdiqmM9tnoZS63al9l9wl1W9PG/"
        "5/ROMtR1HqK0PWcvtnG+4ahr2LwGX2a/"
        "wNnWylTQNnnxNvZS7DitqoW9fE4ybLRuKmu5q9tMvj61GhDKWeSyS9OmsGq/"
        "oy+n1eSaZPg+Xc41E1mubnsteuCOcU4qZOYm01mYiJmK5IiJtC0tvWXBGtrmwVi1PPU0U4YmLYuO6sTl/"
        "wDU77V8YiLRzzHM1nwjXXCZvDbLhsTsWuZbG57X89jbLMYPOYa+tsniMxiMlbU7zHZTF5Kyq1rO/"
        "wAff2lajdWd5a1qtvc29WnWo1J6c8s0fqOXr8Nh1c53e+OOU+kXc8vXydfiGnZch8VTXlxNXurXQdjyU+"
        "O23W6Hjj4qeJ1nbLjE5PHyx9pGSpu95ayzUrW0sqEvUKgdz0F9t1ufR5J7/"
        "NWjsvxxGTHesXx345niZraO6Ime20WrzPCW0WqrrNNi1FY7e+J7q/"
        "8AtvWZrev4xFonifDmvE8RyMm+vDtjOlXoTztXjvP1M/yxzPStbe6veMuO446erq9K8oyXNhPvey5K5oYrWKl/"
        "azy3VtiqEma2WFrWsr+"
        "4wFvi8jZZCvbjrh6jKHSX0m859QU1O2r5Hj3SLu41e0vIQmtL7eM9dWmr6JY3knfCapZXW4ZvCUb+"
        "Wn31PcpriaSHilg5auwn6EtY6yt/"
        "5i61+qvFycv43Xt8uMRruI3ilLnMPvPL2Yo0du3bb9zsL2Saz2GXX7LOYOeyxWQo3mIv8pstzd3lp7TB2Uk9/"
        "tW36S+l1e6bjOSdFpLVxVw4p7cmp1F+"
        "3jFFvprWsWpNpiYni0W7oitubXXavPXPg0Wj7I1Ooi17ZMkc1w4a883mv8ZmYtFYnmOazHHMwt5x3+"
        "J74UzOyW2P5Q6YORdC1i4uJKNTY9T37Aci31lSqTQkhd3evX2u6BGa3oxjCpdS2WVvbuSjLPG1tryvCS3qdGXC3NnF"
        "fURxvrfLnDG6YffuPtrtZrnD7Bh6lX2c09KeNK7x+Qsrqlb5HD5nG3Es9plcLlrSyyuLu6dS1v7O3ryTSQr/ANTfZ/"
        "dK/"
        "VRxTnOLt94j0XGRu8Rc2WqbprGpYHCblx9lI0Iy4zN6nmsbZWd5ZzY65lt7ith5q8cJmaFCOMzNje4+tVt5uYbsP+"
        "Y+Q+jXtCOWOz75Myc37Pbxsu+6VVxs1epDD43mfimnlbjHbPhvbTT0re03TV8BmMPPNby06mwTVtKmq160MXZUprm+"
        "j2vdNDq9TteDNotVoKeey6TJmtqMebTR/"
        "byUvfm8XpEWtMfR4RXtmb1tWhXUa3RarT4ddlx6nBq7ebx56Y4xXx5vCK0tWvFe20zERPjM893MRWaz2mAMYTYAAPK"
        "bjvmjcd4mbP8AIG56nouClqeymzW47Fh9YxMtWMs08Kc2Szd5Y2cKkZJZp/BGtCbwyzTd3dCMYfA0Lmbh/lX3mHF/"
        "K/"
        "GvJEbKSFS8hoW9avuHulOM0JIT3P7PZTI+wkjPGEsJqvgljNGEvf3xhB383eazeKXmkTxN+2e2J+"
        "qbccRPjH8XXurForNqxaY5ivMd0x9cRzzx/sSUwZ7RjtWeaujjri6f+l/"
        "QuPuLtm03lrUOKthzme2+22yrs2Out85e3jj3J0cVVw20YjFSW9pitXs72xheYy7qS5C4uZripXto0relvM41e3T/"
        "ALrx0Vf4MenH/"
        "wATnLqa6e0uDV7hOLU4q5ccaXUX7Lc8d9Kc1t4TE8xPj9PCO3XNlwaWL4rzS858Fe6OOe214i0eMT4THhLsqH9alSS"
        "lJPVqzyU6dOSapUqVJoSSU5JIRmnnnnmjCWWSWWEZpppowhLCEYxjCEH4MbmMRmadStiMpjsrSozwp1auNvra+"
        "p0qkZYTQp1J7WrVlknjLGE0JJowmjLGEe7uj3oPieJnieI+mePCP9cpN9EQxm+o/"
        "p51nZZtM2TnnhjX9wkr+6z6pm+UdHxOyyXUZ/BC2mwV/nLfKS14zxhJ7GNrCp44+Hw9/"
        "wCiZKVWnWp061GpJVo1ZJKtKrSnlqU6tOpLCaSpTnljGWeSeWMJpJ5YxlmljCMIxhGEXNqXpETalqxaOazas1i0fXE"
        "zEcx4x9H1usWrbmK2raazxaImJ4n6p4nwn8JQp1McnZjhPpv6geZtesMblM/"
        "xHwjytydg8ZmpbqfD5HMaFome2rGWGWksbmyvZ8beXuJoW9/"
        "LZ3lpdTWtSrC3uaFWMlWTPXsgu0L5S7Q7izlreuU9N0DTMlx/v+M1LF2nH9DYqFjeWN9rtvmKlxfy7Hn8/"
        "cTXclxWmpSRt69Cj7GEsJqUZ++eNwev7+0Q61/8UjqQ/wAzm5sQ/wAML/a7dS3+GnX/"
        "APIaxTem0uC+w7lqrYq21GHVaWmLLPPdSl7Ui9Y8eOLRM88xMo7Nmy13TR4a3mMWTBntenhxa1f7Mz4c8x/"
        "DxdOYhWHUn06x2f8AYqHPnCsdy969y/ZKHKejR2f32M/s4Wn5D+e/mvvUan9T93909r4/"
        "3PB4v0TUhLUvTjvpandHMd1ZrzH1xzEcx+MJGLVtz22rbieJ4mJ4n6p4+iQEeb7y7xPxVQtrnlDk/"
        "jzje2vYTRs7jfd11vT6F3CSPdPG2q7Dk8dTrwkj+k0aU0/hj+ke6JWtrzFaVta0/RWsTaZ/"
        "1RETMuZmKxM2mIiPpmZiIj/XM+CQx43SeRuPeS8ZPm+Od703f8NTqQo1MtpOz4TasZTrRhNGFKe/"
        "wV9f2slSMJZowpzVYTRhLNGEP0i9k4tW1Zmtomto8Ji0TExP1TE+METExExMTE/RMTzE/"
        "wCqYH+"
        "F1dW1jbXF7e3FCzs7OhWuru7uq1O3trW2t6c1WvcXFerNJSoUKFKSerWrVZ5adOnLNPPNLLLGMP8Adk52z2h9WvKXR"
        "RtPHXSTrN7tOa2nOY+25Wxev5WhY7nkeJLSzyN7nsHqVhVq2lbOXWcytDCWWZw+"
        "OvJspl9bhl8FaYrMy5mtay19JgrqdTg09suPBXLlpS2bLMVpjrafnXtMzWPCOeIm0d08V5jnlTz5Zw4cuWuO+"
        "W2OlrRjpE2veYjwrEREz4zxzMRPEczxPCqfVB+Is6S+"
        "Fdpyel8NadtfUtlsLdVrPJbJruYx2l8Z1Lq3qexr0MJuGSsc7lNhhRqyVZY5PF6lW168pwpXOIzeUta0taXwPAH4lj"
        "pr5D2rH63znw1vHAVllbqlZ0dysNks+VtTxNStPLJC52Waw13T9mx+Ol7/"
        "AOqXWI1nYalKaMs1a2p28KtxStL2VXZRcMdKPBukbvynxrre5dTW84DGbRu+wbvgLHN3vHVbM2lK/"
        "tuPdSscxbXNDWp9Ztq8mN2TJ2NKTL53PyZSpc5CfDU8Pi8b4Htv+"
        "gjgbkzo85U57wPH2qabzVwjhrfeMVu2sYPHYLIbHruPydlR2fVdunxlvbQ2DHVcHc31/"
        "hamRlr3uGzdlaT466trG9zVnkskpHTNtXXba6TV5K3y108bn8KtF5y2tGOMlcERGLzU3nwtNZntnu83P0IW071Gntr"
        "Jz6es1pOadF5mJrGOI75pOWecnf2xPNYtxz4d/"
        "wDGN0MDnsJtODw+za1l8bsGu7DjLHNYLO4a9tsliMzh8nbUr3G5TF5GzqVrS+x9/"
        "aVqN1aXdtVqULihVp1aU88k8s0YQ6mOqjgrpC41vOVefd7x+"
        "k6vRre44yjUkrZDYNozU1Ketb4DVNfsZK2Tz2YuJKc1SNCzoRoWNrJWyOVucfi7a6vqGNf4bzmnaeQ+"
        "jHeONNmv7rJ2/"
        "CHLF7g9Pr3VWerNj9N27CY7Z7bA056kZp40MbslTaLq2hGfw0LXKW9lRkp29pRlV77Qzs+"
        "esbrz7UjjXA8m4TPYPots8dZ4/"
        "U+QdXyNtmMHqujYHB22w8h0L2jJCpDV+"
        "TuQNpp3GExd3nMNLSuaFXWZaNbYsXqNxLRsce06fHu2q0Ou1dcGm0cZcuTLzWuTNixxF6Uw1tzE5slLRbtiL9sRfiL"
        "TERN3fX5b6DBqdNgnJm1E46Ux8WtTHkvM1m2S0ccY6WiY7pmsTM1iZrzMx/O5/"
        "iheLcfsFa14+6Sd82rV5LiaSjm9u5TwGi5utbQnmhC4m1zEadyBZUqk0kJZ5beOzTwhGaMs1aWMvfHUToM7X/"
        "pa688v+wOrVM9xdzPJY3F/"
        "JxbyDDH07zYLWxozXOSudF2HG3Vxitqp462lnurywnlxGyUrOheZH9no4qxur+nc3jDpF6YuG9As+"
        "MOOuCOLtf0q1x8mNr4n9jsJk6mZoy0oUqtxs2Sy1nfZTacjdyw77/J7DeZLIX08Yz3VzVjFxo9s/"
        "wBOes9nh1ycO809LFtR4us9zx9jy9rWua/JGzw2kclaPtXssxLrdhRmkpY/"
        "VcrCOCyMNcpd2MtK99m8XZW9tgJ7HF2chpNPsW8ZMmg0mj1Gg1M48ltLqb6m+"
        "eMtsde7tz4rTNad1Ym0xj58ItxeJ7Ymz1GXdNupXVajUYdVgi9K58NcMY5x1vMRM4rxEWtxM8RN+"
        "PGY5rMTPHdyPLaNs9LdtJ07cqNvNaUdt1bXtnpWs03imtqWexNpladvNNGEsZpqMl3CnGaMId8ZYx7od/"
        "c9SxKYmszWY4mJmJj6pieJj/in4mJiJjxiYiYn8J+gAcOQAAAAAAAAAB/"
        "z6Ohrrc4x6Bu0b6kea+Wdb3zadYys3O3H1DHcd2GvZHPU8vnOW8HmrW7rW+"
        "y7Jq2PhjaVtrN7TuKkmSnupa9a1lp2lWnNWqUf+gu4auyH4d4n5v7Vrqd0/"
        "mTjTROVdUtda6hs7ba1yHqmE3HBUM1Zc1abZ2mWo4rP2V/ZU8la2mRv7a3vZKELijQvbqlTqSyV6ss2V9OWwV0W/"
        "W1NL5MEaTBOXHjt2XtTvy8xW38J/FA7xGSdRtcYbVplnUZIpa8d1a24x8Tav8Yj6msX+yaeiL/5G+qn/wBlOI/9M5/"
        "smnoi/wDkb6qf/ZTiP/TO1r/1u3oH/wDQs6Wf+wfjL/8ADR/rdvQP/wChZ0s/9g/GX/4aWvwnpr7t3D2uv4f/APf/"
        "AOfRX8zvX2zSezy5B+kXnvU+qDt89B590XF7FhdS5O5X3LYMHi9stsbabFZWknB+"
        "0Y2NLK22HymbxlK59tY1Z4S2eVvaXspqcfawnjNJL1t9pp/c9+sv/F25Q/yYvnLZxHpOnccfiPcfo/"
        "H2q69pGma5zVuFjr+qaph7DAa7hLKPAmwXEbTFYfF0LXH2FtGvWrVo0bW3pU41atSp4fFPNGPUn2mn9z36y/"
        "8AF25Q/wAmL5I73OOdz6enFW1MU6HbZx1tPdauOdReaVtb+NorxEz/ABmJlabbF40O7RkmLXjVa2L2rHFbXjDXumI/"
        "hE25mI/hDKD8Ml/acc5f4zGX/"
        "wA1vGCj34hffrjintCujblG0sYZO642420XfrbGxuZrKGQuNO5u2rYqNjG8lpXE1pC7qY6Wh7zLQrTUPae1hSqRk8E"
        "14fwyX9pxzl/jMZf/ADW8YM/PxI2Os8v1u9L2JyNb3fH5ThTAY6+rwnhTjQs73lvc7a6reOPfCT2dCrPP44w7pfD3/"
        "wB5eaaK26y1lbRzWa6iLR4+NZ0tYmPDx8Y8PDxW+eZjp3TTWeJjzExP1TGbwnx+qUqcOdlF1Q9q3Ssus/"
        "rx6g9j0DFcn0J9h4s4x13Cxy2TwvH2XqQu9f8AyKyzeSk17jjU7uwjQvNfxNri9hy+fx1e22PYrynl7+5uL+"
        "Eus7sgefOzDwtHrK6NOoHdc/"
        "h+"
        "NLm0u9trULOnrHJOj4m5vLa2hm61TDXNfBb5pFW6nt7Xb8VeYmxo2uPry1sphc3r8Mzd4rtTxuOsMPjrDEYq0oWGMx"
        "Vla47HWFrTlo2tlYWNCnbWdpbUpYQlpULa3pU6NGnLCEslOSWWEO6EEV9Q2uYPcOAub9U2ajb19d2XiLkjA52jdS+"
        "K2qYjLadmbHIwrQh+vs/dK9WM0Ze6aWEPFLGE0IRhD4eptfGqxx/VRoJvXF/"
        "R8YMMaeNNNorOGI7O7mKfRabTPd9MTWZpMhk2XS+YtPz51cUm/wALnLk87OeI5jJz3cf2o/"
        "s8cRX6OLcWVA7LTrrtuvrpbwnJ2XtsfieVNQylXQOYMFjYRo2FHccZZWd7R2DD2tSpPXt8Dt2IvrHNWNGeM9PHZCpl"
        "9fpXV9HB1Lytzh9tzzJlOnjtfODOc8JirXO5XifjPhPe7DCX1xXtLLL3Gu7zvuQlxl5c20I3FG0voUY2tzUowjVlo1"
        "Z/B+93Jf8Awumcysm19Yuty1Ks+Dude4YzlWlNNNNQoZWyyXIthQqU5YzeGlVu7O/"
        "uJa00ksJq8llbwqTRhbUoSxT21Gu4Tb+"
        "2Y6ZNT2WlRr65tGE6YNdz9C5hLG3rYTN8x7LjcrSrwnhGWNGpY3NeSrCaEZYyRmhGEYd6X0WjwaHqfcNNWvOmro894"
        "x8z4Ys2HFlti+nmIiL2pXx57YieeUfqdRl1WyaTNNv66dTir3z/APmY8mSlbz/"
        "Dme2LT4cczPgnHh3sVOortBrSz6tO0J6kt01zbuVrGhs2A48wuGtsps+"
        "u6rmO7I4Ozu62bupdd48spbGvJWx3HuB1q9p4WzuraXJ3WNzMmSxFCs/"
        "WJ2ZXUz2PtzhOsjpE592nYtF1rM4vG7XlaWOk1/atNkyt/"
        "QtMVbb1hrO6v9X5A48z2TntMPkZr2ytLWTKZHGWF7rlWlcU8nJ28whCEIQhCEIQhCEIQh3QhCH6QhCEP0hCEP4QVE7"
        "QHXcJtXQx1g4bYqVvVxc/TVzVkZ57mXxUrO9wXHufzuJykIf/"
        "ALXD5bG2OVt5u6PguLOlNGEYQ7oxWl6k199Zhx5JwzoMmSmC+"
        "gjBhjT1017RScVIjHFo7KT820zPMxHdFqzNZv8ANs2lrp8lqd8aqlLZK6ucuTzs5qxNvOWnu7fnWj50RERETM14mIm"
        "Kw8JcoaD2xfZp7Nb5qyx2ByXK2l7JxZyRhqEJ7y04/wCZNftrO7tctYU6s9etNaYvPR1TknVre5q3NzSxV5haF/"
        "UrXlO578P/AMO7zjsfBfU51BdB3KEK2Dyez3Ody+GwF/V8M2I5h4fuLzB79r9rRhGEJr/"
        "M6pa3d7f1YyzS+78cUJZIyeLuqTx+F4zmWuOM+rvWq1SrHB4neuKM5jqM0ZvYU8tsOv7nYZmpTl7/"
        "AAwq1bPWMDLWjCEJppKNCE0YwllhCq/"
        "bM8fbJ0F9plwj138YY+e3w3I+"
        "bwPJVeha99rY3XI3HFzisPyXrFzNRljLQst81C5wl3k6k80lbKXOz7VPJLGNCtUSWHTYq63fOmomIxams59DFp8Mep"
        "pjpqMdIn6f7E1i8zPM0wcTM9082WTNktptr3meZyYJjFqpiPG+"
        "G17Yb2mPr7otMREcRbLzEeEO0pxmbXaf66f29FPXYy/n/"
        "BnTVmZMbkpe73vFVOPun3KTV89SreHvt7vEchc05O4wstzRjJG4wG02lSnWqy2tOpN0b9ZXWbqvC3Z/"
        "ch9XOk5qhd2mc4ixWd4ayPfJGOW2blXHY+w4vu6dt4p5riSjktjxWbyVrTjNPSxWPyVSeanTt6tWnl1+"
        "HA6YK2g9OPIPVJtNnVm23qI2mriNYvr6WareQ424+vr/AB899Tr1/"
        "wCuKc+zb5X2afIQj3y39vreAvvaVYTU4yRO1RO37fum6XiaZu3+jNJ3RMWjUZvHPaOfovhxRFon/"
        "wDqrP1Tf66Y1er0OirMWx8/DdRx4xOHFPGKJ+uuTJM1n/8AbP8Aq6QQGNplxT/"
        "iGrq2su0y6Yr29uKFpZ2fAfDt1d3d1Wp29ta21vz/"
        "AMy1a9xcV6s0lKhQoUpJ6tatVnlp06cs0880sssYw6sp+"
        "ubompyTVKnWH0sySSQjNPPP1B8SyySywh3xmmmm26EJYQh+sYxjCEIfxcnf4jLC2+y9o30767eVa1C0z/"
        "TjxXhbqtbRpwuKNvledearGvVt41ZKtKFanSrzT0o1KdSnCpCWM8k8vfLHRu//AAx/"
        "SNUsb2TF889Rtrkp7S5kx11f3nGd/"
        "Y219NRnhaV72xt+PcbcXtpRuI06lzaUMjYVrmjLPRpXtrPPLXp5vq8W25do2D+kNXn03Gmz+a8zg893xN8Xf3f+"
        "3t4rx4TzzP1cMawZNbTX7r8E0+LPE5sXf5zL5uazFLdsV+vnm3P1cfi+"
        "12sPbKdLuv8ATdytwN078la9zby1zDp2f42r5LQbqGe0bR9W2/"
        "H3GC2jP5Dc7SE2AyuTqYC8ydlr2N1vI5W6o5avbZLJxsbK2pwvfo/"
        "hz+k7duDOmvkfnDkDD3uu5PqYzmoZHUsLkqFW2v5+NNAsM/"
        "T1jZa9rXlp17SntWU3LZb3GU6lKWF9r9rhM3RqV7LK2c0uDfQvx7wl0S9pVd9PnaK8N6jsdPHbBR0vXNu3GfI3elaR"
        "uN1d0LvQuSamGu7m31fZ+PN2sbizhLf7bhb6jr9DLYjY6sMN+T5ulH/"
        "oDSSySSyySSyySSSyyySSQhLLLJLCEJZZZYQhCWWWEIQlhCEIQhCEIQ7lvvPmdp0Fdp0VMt8Ov81rMuvy2pauqpXsv"
        "jrg7PmxSlq0meeLV8PC0ZO+am2+d1+"
        "qtr9Talcml79Pj0tItE4LWia3tk7vHm0TaI4mYnx8YmnbHNr24XaYcq8M7FqHRJ0oZLK43m/"
        "kyww91vG36rGepuGuY3b7+"
        "bEadoWjVLbxXWO3fcq0Jry5ytt7HL4XDXGEjgqkuQ2CnkMRA3CH4aW22rTLfbuq7qM3e25c2q3hm8/"
        "gOPLPC5O11vL5KE11c2Wc3Pafz253bL061WEcvkbKyxFnNf8AvVCyvctbS0cxd5p9W2W6iNi7c7k/"
        "KcBaxjN66gsDzjY1eJ9a2ObCRxl3kOOePcXNrk1WGzZjA4SEMRr+uUsrj5L3LWck11Y23sI1bqajQraxedX4mL/"
        "0buMv/wCnwP8A6ZEnODUbft+34Ns1u27fk1Glx6vWZtVqMOHVZ8mWItWK+dpefM4/"
        "nVrMRHPHEccX7rOMmHWavV5dbptZq6Ys98Gnx4MWXJgxUxzxMz2WrHnL+E2iZn6eZjxrxnZyvxx1Z/"
        "h9+pjj7b+O+TrzlHp75RvLi6nx1Wld4LV+R8bga+Pk2rSN91GN9l8bgN9w+"
        "OyNpX1ncsbXuq01vd0sli60lGXZ9Vte1rj7k7A8r8S6dzDx3JV2LXeQtBwnIOm28attYXWVx2yYG3z2Fsa9S4qxtcd"
        "fXFK7oWd1LdVYU8fdxq07maX2FTu4/usbg3t7OunQda44566XdUvdf1Lb6O7YWrq2a4L13K0MzSwuXwUac9/"
        "T5bupqmPr2WauJrizjThCrc0LKtGeEbaEs3SD2fWo7d0qdndwtrXUtJbcdbBwzxntF9yVDNZjEX9npuCwef2nPe1yO"
        "Ywl9lMRUtMNqXuVavVsb67o0aFCNOE8Z6c0kIvfox59FoNRl1Gg1W6xltp9TbQ5sWWdRjmLTiyZK4YrPdWKVpzFI+"
        "deYr4dta3u1zfFqdVix4dVg0E44zYY1WO+OMV4msZKUtk5+bPdNoibTxFYmfHm08/"
        "9j2dnaK9q11Kcn7f187DufTZxZxrs9fD4HR6lpHMYq1nuact3aa5wlhZMhJpuUw9jhq9pLn+YPe8zDN31W2owm2W+"
        "lylHAfX6u/w7uL4P4d2rm7pN505Oy3IXEmEvt/"
        "jqm608JJk89Z6ta1Mxk5tM2nTLDWb3A7TY2dncX+BtqmOyccpf0KGLp3uOr3FO+kknb+"
        "286zurPlfYeKey96W6e4YXAzzQq77uuByGxZ66xsa89vZ7Lf2X57rGi8YYnJ16VWljaO7ZbN176lLJGpNjchPXxdn/"
        "AEzvOv4knQcNlMrvfTJx5v2t3GOvqOSwdLC8RbdNPYXFrUpXdKOJ4e5Zt9sqd1vNU/doxnnjGMYfv/"
        "2MJKMu+4smnidTtO146VxRTab6jT4ecfbXurbFeMl/62OZmL5e+s245raOVnOPa71y/"
        "wBTr9de1rzbX0w5snF+Z4mL1mlf6vw47ads8fxieF4Owm69986y+njctL5jzVbZ+Xun3Ma/"
        "gsruF7NLNltz0bbLLJ19KzmwVu+E19s1rc69smCzGS9lCpkrfGYnK5GtdZrI5K6r/"
        "c7aftKsz0JcPa7pXEdzYydQ3NUmXoank7uhbZCjx3p+"
        "JhQt89vtXG3Ela2u8xPd3tth9Ns8lQnxlfJfmmWuqeQoa1cYfI5PfhdqlSHInWBRhPNClPpfEFSenCP7s1SlnN8lpz"
        "xh/CM0ktarLLH+9CpPCH8Yov7ZCjS5J7ajp5483iEtzpU8/"
        "Szoc9pefvWM2pbRyTXvdgkmp1IxpRoXFzsmbkuZowhCeWWaSfvlkh3W87Xo56q1WG2KvwTTYp11tPERFLTGDFlnHFf"
        "o7JzZO7siO3sjs47VaNdqI2LBkrkt8Iy3jSxmmfnV5y3pF5t9PdGOnHd427p7uefFI/St2BnK/VrqFh1J9b/"
        "UHv8Are38s2Vrt1nrFCh+13J1bFZi3p3eKzHIG37je3lPF5a+sqlG5k1WlicndYuxr2tDJZLGZOneYPH/"
        "AMbr2PHXz2efO/F3LPZ0cm57l632DZrPBZKzq08fpt5hacsta/"
        "nxXMGKus7Jp+2cXZK0s7mS9zdxXx0LHIVLe0p4jHZmfAZS97GYQhCEIQhCEIQhCEIQ7oQhD9IQhCH6QhCH6QhD+D+"
        "UVPVG52y3m84MmmvFqfAcmDHbSximOK4+2K1vMUjjie+Jnj50zEzC+jY9FFKRWMtM1Jrb4VTJeM83iYm1+"
        "Zmac2mJ8O3iOfmxE+LCPtL+yew/XLUxHUzyfyll+L+QeMOmunr+Z0XScVYbbqlbNapNue/"
        "5KlZbJmo4LKXVhHN7Lf4e1vamHsq1xjbK1v57K2uLira0ubnskey803tI5Of5tt5X2bjHyam4slx8Nd17FZ7878woc"
        "jRuo3n5ne2fuv5b+w9t7v7D2nt/f6/tPB7Gn4u8/nP/AHkuYv8ABZyD/kll3L1+Fp/8l1zf/SdNH/"
        "8Ajz+kds3TXY+n92yY9RNLaK2hppOK0mMFMupit61i1bd0WrMx8/"
        "vnj6J58VprdDpb7roK3xRaNTGqtqObX5y2x4eaTPFo4mJiJ+Z2xM/THDZ/s2uzM0/s4MXy7jNS5T2Xk2Tlu/0u/"
        "vquxYDF4GbDzaZb7Nb29O0lxt5eQuZb6GzVpq01aNONKNrShJCaFSeMNOQYlqdTn1ea+o1OScubJ29+"
        "SYrE27KVpXmKxWvhWtY8IjnjmfHmU9hw4tPirhw0imOnPbWJmYjutNp8bTMzzaZnxmfpAFBVAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAGdXaDdpPxJ2dOH4xy/"
        "KmkcibrLyrf7Zj8Bb8f0NarT2VfULbA3N/"
        "Pl59j2HAy0aNxLsNpLazWcL6eM9Kv7WnShCnGpoqiblTgTg3nS2xFpzZw1xVy/"
        "ba9Pf1dfocn8fanvlPA1cpLayZOthJdoxOU/Ka2QksbGS9q4/"
        "wB3qXUtnawrzTwt6PguNLbTU1GO2rxXzaeJt5zHjv5u9o7bRWK3/hxeazP1xEx/"
        "FRz1zWxXrp71x5p47L3r31r86Jnmv8ea8xH1TMT/AAcS/XV2onJva0bTxv0r8YYHS+nvh/"
        "J7pYZCavytybr2u0to2S3pXNviszyFuuW/Jtf1/AYKhd3VbF6diYZq/wAnm6lGtQrbJm/2cxWP6/"
        "8AoN6Ttb6KOlzjTgDXstS2S512yvMxtu3UaPu9Lbd12W7qZjY85bUYzzzUsZG6uJMbgaNSepXoa/"
        "jsTQua1e5pVq9XJztRexd6UNl6cOWeZunvjbC8I8v8T6VsnI9rbaLCvitL3XD6di7nP5zVsrpkK8+v4+"
        "7vcNY5GXAZTXrPCXVLNzWMmUrX2L8dtJ5f8N71W71y9wXy309b/"
        "mr7Yf8AU85LS7zjzKZW5q3mQtdA32hsdCnqEtzVmnq1cXqGX1K5qYiS4nmnscdsVvhrOaTFYqwtLPKtxnT67YaZNpi"
        "dPodv1FfhWiyU/rZy5ZilNRbN5y/"
        "nY5ydsRP8LTM8ebisQejjNpt0tTX8ZtTqsVowamlv6vzeP51sMYuykUnivdM8fTERET32tPSa4/"
        "8A8UVst3W3Do806FWeWwx2tcx7LNRlmjCSrd5rKcfYuSrVkh+"
        "k89vRwNSShNN3xpwubiEnhhVqeLsAchH4orVLynsfR3vElKeewvcJzJqlzXll/qdveYy+48y9lSqz/wB6e8o5a/"
        "noS/8AClsLiMP7GKO6V7f6d0fPH0ant5/93wXN/wDPHMLvfef6L1PH14ef9Xn8f/"
        "8Avh049Ius2el9KXTNqNhSkpWmtcAcO4OhJJL4YRlxnHuvWkak36QjNUrT0pqtapN3z1Ks89SpGM800Y2GVu6ONtst"
        "86SOmDcsfWp17bZen7h/"
        "LwmpzQmhTrXfH+AqXdtP3Rj4K9ndxr2lzSjHx0bijVpT908k0IWQmmllljNNGEsssIzTTTRhCWWWEO+MYxj+kIQh+"
        "sYx/SEP1ihNR3fCM/d/"
        "a89l7ufp7u+3PP8At5SeHicOKa8dvm6dvH0cdsccfhw4d+"
        "xl7uLe2d5S45w3faYqvN1QcaTW1OPspI4nV9luM5Z2s1OTuljTpXGk4+pLS7vDJNQkmh/"
        "5ODuJcO3YoQ83u2K5U5Sw0I3WItsf1KcpxuqcYVaUMXuG108DY141Yfuxlr1t7spac0v61I1IRhDw+KMO4lkPVn/"
        "4lh5/txt+l859ff8A1nPP48dv+zhD7B/9FkmP7M6vNNPq7eKccfhzz/"
        "8ALBH8R1st3guzyssXbVZ6dHdOoDjTWshJLPGWWvaWuD3rcZKVSEP0qSQv9Tsq0JI98IT0ZJ/"
        "4yQilXsCNZs8D2YnC2VtqUlOvuu18xbNkZ5ZYSzV7y05U2vTZKtSPdDxzwx+pWNGE0e+"
        "MKdGnJ390sIQjT8Rlqd5sXZ2xzFtSnqUNC524y2zIzyy+KW3s7yx27RpKtSP/AAJJshudjQhN/fqVqcn/"
        "AAkg9gBttlsfZk8SYe1rSVa+gbry/"
        "qWSkkmhGa3vb3kfP73To1YQjGMk82O3WwuISzQljGnXpzd3hmhGPNvHpHH2fw3mfO8f/wCPk45/2zj+n8HNf84Mnd/"
        "Hbo7Of/7tOeP+F/8A5bRuHbrThDiv8RNpWfwvfZ0sr1I9IOduadKPsYT0NqwnE2K2qSE8vd/"
        "8Kwr5mevPNCMJp76rCeWeXvhN3EuHbq7jDmP8RXqGEwP9fW2G6l+"
        "lLC3M9vNLWjLbce4Hi3KbpHuh3yyTYufE7DSrSxjN7ONlUmqQhGE8krpX/wCq3Gbf+nG0avznP0dvfg+n+H0c/T/"
        "Dn8TfP/R0fH9r+kNP2/Xz25Po/wDj8OePwdxIoh2iPB/VR1AcCY/Rej/"
        "mq34G5Woch69sF5u1zuW6aNTr6fj8Tsdrl8DDNaJgthzc81/kMjhrqFjPYS2NeGPjUr3FOpQoSVMPv9a87eL/"
        "AOcwxP8A9pHqV/0WovR7fptTh85l3TR6S3favmc0ZZvxHbxb5lJrxbmePHnw/wCF7qNXmw5Oymh1GorxE+cxzjivM/"
        "THzrRPMfx8PqdWCOOYM5yHrPFnIWwcSaZbcicoYjUM7fcf6NfZexwFjtO30MfXmwGHv8zkruwssfY3eS93lvbive2v"
        "htYVZadelVmknhzLf61528X/AM5hif8A7SPUr/"
        "otdJfMPM2hdNPCm08yczbHJhNL421ijlNqzXhq3lxc16cLXH29ljLeMJLjJ5nP5m4tMVhbGWElfI5XIWltD2c9aM0v"
        "Gp0WLTZNNGn1em3K2XJ2+a09c0/OicfZS8TFL289N5rEUmLfNmImLTHHOHU3zUzTm0+bRRSnPnMs4/"
        "omLd1qzzaseb45mbxx4xMxMcw5TOBeyI6xe0o3DcOoztJOVuUOK4zbLm8BgdHyOFlk324kxmQq22Sk1jAZzu1bjDjq"
        "yvaU9rrVPHYTKU9l93ucta2FLGVrDPZ3y3Xr2KOx9n9xnW6xOjzn7k67ueG7/"
        "F5vZrLMVbLDcg63ibnI2uMhuGqbfpdHA0rqliLy9tI53C3GDtYwwdS/ycclWoWNfH3E/"
        "wBl2wPag9cm27Njezm6SMPZ8e6/fzWE217TjbbZMxb1ZpfHZSbHu20bNqPEeuZa/spqWRjqns8tfWUk/"
        "dSzWVtKfvlaMuqTmzt+sB06c3a11NdOWi5vhvbuLt2wXImx4/"
        "CccbHdahp2SwN9Qz2z291w5ydczYi417HT18nRyWaxmQxlhUs5Lu/srq2pVZJ8xx5d8rrMFc2r2nR4e/"
        "FSdmnUaWs109prE4YwxW0zaaTPbPnJt3THbxHFYx2+PbLafLOPBr9Rk7b2jcYw57ROWPGMk5OYiKxaI5+ZxxE/"
        "xnund/"
        "slutDN9cvRzqnKG7RtJuUNQz2X4s5SuLG3o2dpltt1izxOQobHQsbfwUbObZtYz2vZvIW1tQtbC2zd5lrTGW1DH29t"
        "SkwF7dP+68dFX+DHpx/"
        "8TnLq6P4YypUj0p9QVGM80aUnUHCpJTjH92WpV4402WpPCH8ITTy0aUs0f78KcsP70FLu3T/uvHRV/"
        "gx6cf8AxOcurPQafHpeqdfgw1imKmHWTSkeEUrfDXJFKx/Cte/"
        "trH8IiIXWpy3z7JpMuSZte2TS91p8ZtNcvb3TP8Zt28zP1zLrO6gP94fmz/BHyR/kbmnAP2dG69ZfLWubX2e/R/"
        "lamj33P2522+8o8nWuVyOGudW451bAyYTNU7/OYyhVyGuatcVL6z/aC7w/izuyXUcFp2P76OavcdmO/jqA/wB4fmz/"
        "AAR8kf5G5pyx/hcddwdzsnWfttejQn2TD4PgvXcZcTSy+822D2S/5Wyeco05ow8fsL6/"
        "1TXp68skfD7TH28akIx9l3Wmx566XZd71NsWPNOHJob46Za92OM05LUxXtWfC0Y8l65O2fCZr/"
        "D6Vbc8Vs+5bbhjJfHGWmpre1J7bebikWyVi38O+lZrzHjHKWKH4X7jWbTalHIdWW+"
        "VuR6tpGeOeocc4CXTZMnPJNNNNU1ivsFbPXVpCtGEsZv2xtrirJCarH2U0/spKr9FPU31N9kL1uY/"
        "oO6rtmr7DwDtWcweDx1xdZO9ymr6jYbndRs9J5c4yyGVlp3WJ0m8yUY2m665LLa2NhCTYa9fG09o1+tJc9pDjx/"
        "FEa7hLTkPo92+2pUJNlzWm8v67lLiSEYXc+E1XOaDk9epVZ4fxoUL/"
        "cNmqW0IfrJVr3Uf+"
        "HBW2jc9VvWpttW6XjVafWYs3bNsWKt9Pmx47ZaZcVqUp2zWKTER4xzMccRNotT3DRYNtwRrtDWcGXTXx90VyXmubHa"
        "9aWx5Ita3dEzaJmfp45558JjpX6/v7RDrX/xSOpD/ADObm4fOzR496y+rvWN76G+m/c/"
        "KjiTa9ntuUeo3k6nUydpJba1LirPV8NrOVusVVt8hk7TJz2uSnx2i2F3ZQ3S+"
        "jWjnLy017X8hfWfZp1IZvK7L2VHPWx5yepVzewdnxyjm8xVrRjGtUyuV6cc7f5CerGMYxjUnu69aaeMYxjGaMe+"
        "MYsg/wwOMsKXBPVFmadtSkyd/y1puMvLyEsIVq9hiNOr3eOtqk/"
        "d4pqVpcZvKVaMsYxhJPe14ywhGebv6bVqLaDY93zVpjyZcOs00YvOVi+"
        "OuXurSuXtnwtOOZ85j5iY761mY8HfXYY1W57fjm16UyafNbJ2WmtrY+"
        "O6cfdExMRfwraYnntmeEZcofhh8PYcZZW74h6mc/"
        "sHLeMxNe7xmL3bSsPh9K2zK21vNPTw8LjF5i6ymoSZKvL7K3ydxcbRSsZp6ctzbVqXtLmn6j8PZ1vcuZ7buS+g/"
        "nbM5zNX3HOt5PauL6+2XNe52PU6Oo5/HaxvHF13eXtWre3VljLjLWGU1vHV4zza/b4zZcfSr/lkuKsMd1TuM3s/"
        "LelivxDvUxY2EsLa0q8n9ZMJqFKEJKXguNj2HIT0pZJYQllpS3XhqSU5YQlk8EkJYQhLA0u4aneNs3jBuN41E6XSxr"
        "NNlnHjpkxZMdp5is46V8L+ET4cxWbV5mtuI659Lh27W7dl0dZwxnz/"
        "Bs1Ive1clLxERNova3jXmZ+n6e230xzPTb167t1PaB0uclbB0fceT8j87wsaNjrGOozYmvd6/"
        "Z3c80ub3DGYHMV6FDbc1r2Nkr18Fq1CW8ucrmath/tTmbahc4m950Ol/sDeTOqzWaPUb2gnPHLeC5H5RpU9j/"
        "Yq1hQyHJ+Ox19ThXsLjkTaN9t83+UZy4oVJZv2Ioa7NU1mz92sr+/tMl75gsP0d9a/WfxF0KcI5TmrlyvfXdt7/"
        "AENe0/"
        "T8JChPsm97hfW91c2Gu4aS6qUra3hC1srzJZXKXlSS0xOJsru7mhc3ULSwvefDTu0m7bzrfoXe79GvShpWo8Tz315b"
        "4HOXmMwdWhfU7S4qULm38xeZtv1LUtvu7CvTqWV9canruNo291RqW1e0o3VOpJChs/"
        "8ASlNDmtofgWgxWzxGXddVfHhtbiscaauXN3x2RMd39Xj8LTMTbmZVtx+A21WONT8J1V645mmhwUtlrHM/"
        "+tamPtnun+z8+88xxxXiIU864OgDnHsV9r4z6tOkznrb8ro1/t1tqN/"
        "d5S0t8dncHnatpe5nHaxvuOxNSTXN+0bbcfh8tTrRu8TjLS3vbSSwucfLd3WJv6vX/"
        "wBI3UFi+qrpo4Y6g8TZU8ZT5Q0fG57JYmjVjXoYXZKE1bE7dg7evNNPPcW2F2nHZjFW9xVjLWr0bSnVrU6VWeelJx/"
        "9pJzJ2y+Q6Yc9xv12cAafh+IchtWn5K75Kw2B1W+u8HnsXlJamCoU9k4x5DzmpY+"
        "GWuqk2PqSZXC1ql1Jcz21pVtq88k0OhjsJqlSp2WPTJ455p/Z1uaqcnijGPhpy9QPKkZZIRj+vhl74+GH8IQ/"
        "SHdCEIQut8xZcuzaTWavNpNVrcetnS/"
        "DNJkx5aZtPfFkyVi98Va1telqRERNYmscz9N5m1DbL0puWo0+nx59Ppr6aM/"
        "wfUUtjtjy1yUxzNa3mZitotMzMTMTPh9FYiNdAGGMjHPD+IO649S4e6a8j0ma3lLa/"
        "wCYOoK0xsudx1rWknuNL4mx+at8hlc1lJZZpvd7ncr7EQ1TCWden/XuMqbTkJKlCfFWvvN/"
        "e0s7Rfjns9uF6uz5P3DZ+ZNzt8hjeHeMp7mMtXPZihTlkr7HsElvUku7LR9ZqV7e4zl7Tmo1shXqWmAxtejf5KW6s+"
        "fHso+"
        "z25O7QHnHKdor1wVMjtmiXm31to1bE7TbwhLzZumMuYUbW6qYupTltaHEGi1rOhjLDDW1vSweYuMXa6nZW82t4XNY+"
        "vkmy6LFgr/Te4TOPRaS8W09PDzms1VJ5x48VZ/"
        "tUpevNrf2ZtWazPbXLNIfcdTfLb+jNJ87U6is1zX8e3TYLRxe95j6LTWeKx9MRMTHzppFtj+"
        "wk6Udk6Yeh3D5PfMbcYffOfNnuuY8phr6jNQyOA1rKYfEYbRcNf0ZoS1KF1W17E0tpr2leWS7x9xtNfG3tOldWdWhS"
        "2gIQhCHdCHdCH6QhD+EIf8AEITWarJrdVn1eXiL58lskxH0ViZ+"
        "bSP48UrEViZ8eIjnxSWmwV02DFgpMzXFStImfptxHjaf4c2nm08eHM+Hg/"
        "yr16FrQrXV1WpW1tbUqle4uK9SSjQoUKMk1SrWrVak0tOlSpU5Zp6lSeaWSSSWaaaaEsIxcLnXVyHfdsj2pPHXB/T/"
        "AFK+W4u1Ca04pwe52FONfHVNTw+avc/"
        "y5zLPPNLPSpYOlTr3lLAVJ6kKeexmA1j3alJldgp2M90O2a7UfZ+Xttuuzs6KrjKbXk9mzlLj/l/"
        "a9H9rkMrumxZC7lxk3CeiVsfGapcWM15P7hv2TtJ+7K3EK2o06tPEW+xy5bWDskuzG1/s/"
        "wDiStnNxpY3O9SvJ2NsavJuy20aV5a6pi4TU72z4w1S+"
        "lhNCOGxN1LTudhydtNCXadioSXk89bFYnXaFjkmgx12DR23TVeG4avDfHtmln+1Sl4iLarLX6axETHbWePmz2z87J/"
        "Vw2qvO66iNDg5nSYMlb63PH9m9qTzGnxz9Ezz9NufCY7o8KcX1nw+"
        "JsMDicXg8Vby2mLwuOscTjbWTv8ABbWGOtqVnZ28nf3x8NG3o06cvfGMe6WHe+iDEpmZmZmeZnxmZ+mZ+uU/"
        "9H0AAAAAAAAAAAADix7Df+6+9Un/"
        "AEF6kP8APnoLtOZtdMPZYdNHSV1Cbz1L8W5LlG65E5Bxe5YjO2+"
        "2bThstrVO13naMTt2ajj8ZY6riLy3rSZXDWkljPVydxLQtI1qNSSvUnlrSTO267BpdFu+ny9/"
        "nNbpseLD215r31teZ755jtji0cTxP8UbrdLlz6jb8uPt7dNntkyd08T2zFIjtjieZ8J8PBpKAhkk4r9X/wDOZrr/"
        "AA6bf/4fs26a+00/ue/WX/i7cof5MXyP7Hsr+mfH9as/"
        "XnQyXKMebqm0ZHbprKptOGm0T81yepXOmXEsMDDVZcnC0hiLurPSo/"
        "n8Z5b2ElaarPThGhG63NfEmrc9cScjcLbvVy1HUOUNPzmk7JWwN3QsMzTw2wWNXH30+"
        "Lvbm0v7e1vZaFaeNvXrWV1Tp1O6aehUhCMscg1+5abU6rZs2PznZodJocGfupxPfp8k2ydkcz3RxPzZ8OZ/"
        "hCJ0ujzYcG4479vdqdRqsuLi3MduakRTunjwnn6Y8eGBX4ZL+045y/xmMv8A5reMGc/"
        "4mSzu8j1g9O+Px9vWu7++"
        "6f7GzsrW3kmqXFzd3PKG80ba3oSS9809atWnkp0pJf3pp5pZYfrF1J9FHQzwv0E8dbPxhwhebxe63tu63G+"
        "ZSpvmcxueycubucFhNeqSWl1jMFgKNGwhYYCxmloT2tarC4muKkbiMlSWnT5nfxEH90C6Pf8ABhpX+"
        "eva0ttWrxazqvNq8Hd5rLTUXp317bcRp4j51eZ48az/AB+hYa/BfBsWLT5eIvS2Clu2eYiZy/"
        "wn+PhLXXs9+2R6auceIdS1HqI5W1LgnqS0nE2mp8mYDl3M2mgYzYthwFKnirvaNf2PZ6uMwNapsNe2mvL/"
        "AFi5yFtsmCzU+RxVTGXNjb2GWycT9rV2vfTfxz03cl8L8Acs6fzBzZy/qmZ4+tq/GWw2G267x9ru02NfD7Ps+b2/"
        "AXN3gqGatsJdX9nr2Gx+TuM5Rz1zY5C8s7XH2VerPZ7rH7Fbot6ydvyfJmfw22cTcpZutNd7Du3EWTxWE/ay/m/"
        "jkNs1rN4TP61k8lWmjPUvszYY7D7Bla00KuUzF5NJJ4Ys6bOwA6FeAdtxu97Jb75z9sOGu6N/"
        "iLDl3JYK60fH31tUhUtrybStc1/BWGcnpzQjGez2262TD1JvBU/K5atOSpCwxZOmq5q6+3w6tq3jN/"
        "RsY8dscZYmLRSufwidPF/oi0xeacRb+NZur03mcc6WPgtqzXzfwybXrfzcx2zacXE8ZZr9MxzXu8Y/hKPvw8vSFs/"
        "T90sbZzRv2IucFt3U5mte2LDYi+oVLe/teK9QscnQ0O/vKFaWWra1tlvdk2nYrSnCWEtxrt/rl9Gaaa59nQxk/"
        "EIy7PN2mvH0uk0cncbnNwxw5LqNvhLerd5mvs8d63qGAo4i1oU6te5ydXK+"
        "6SWFvRpVKta7mpU6dOeeaWWPc7LLLJLLJJLLJJJLCWSSWEJZZZZYd0ssssIQhLLLCEIQhCEIQhDuhDucbHa3f3c/"
        "oz/5/wBH/wDn5zS72LXZNbv+r1+Wte/LpNVk7PprFaUx1x4+Z8ZiuOlaczHMxHPHiobnpaafatNpKTPbTUYKd/"
        "0TNrWvNr8fwm17Wtxz4TPHLa3ok7ZTpD6muLNXu+SeX+O+CubLPEWdnyJoHJ+zYvQLKGzWtGlb5K/0zO7Td4/"
        "B7Bgcvdy1b/"
        "E2VjlbnPY60qQtMxj7e4t5qlWjfbQ9rd092vTfvnTF05cnazy9yZzLi5tP2zYeO81a7Jpmi8f39WnDa5LrbcPWucDm"
        "c9s+MkutVt8Dh7++"
        "nsLLI5XI5ypj57XGWOYuV1adhf0R9Ve6ZfkyOP3PhPkHYbyvk9lzHEOSwuMwe0Ze6qTVLvL53UNgwWfwcmSvKk89xf"
        "3uuUtcucnfz1Mjlat/e17mvX/"
        "N0qdhJ0OdL+"
        "4YrkW5xm5c571gbujkteyHMWRwuU13XMra1JalplMRpmAwOBwdzfWk8klezuNnp7LPj76SnkcbGyvaFrXoWmHL03gz"
        "U19fh9r47Rmx7bamOaVzRPdWk6jwi2Cl+Jjni81iO6LeNLV8uPecuO2kn4LFb183fWRa/"
        "dOOY4taMXjMZLV5if8A7eZntmvhaPzdhJ0hbP0sdFttmuQ8Rc4Lkfn7ZpuVcxg8hQqWuV13VKmJsMRoWBytrUhLUtr"
        "+bEWtztNzaV5Kd7jq211MVf0qN5YVqFKUe2a6U/8AVWdCPKGLwuN9/"
        "wCQ+"
        "IqUOauO4UqPtb2vktIsr6rsuEtZZJfeLirsWj3Wy4uxx9KeEt1namEqz06s9pRlhqu89t20a7pGqbPue35O0wup6jr"
        "+Z2bZ8xfzQlscVr2Bx1zlMzkbyaMJvDa2WOtbm5uI+GbupUp/0j/CMTO5am+6Rucf/"
        "UTqa561rzxzFo7cMceM07IjFx4zavhPMzK/jR4a6H4FM/1MYJxTaeInjie7JP8ACLd3N+fCIt4+EP8AnaWnVJy/"
        "1g9LfRb2Yut21/"
        "c5vX+fMtjbLJ16k1a0zmL2e8sMbxLbXlWlP7zJZaJW3XkunmJLmn+X2Ou2OrXVKtD8tr+6f9CPhvizV+DuJ+"
        "OOHdKt/ddU4x0rW9HwMk0kklarj9cxVri6V5eRk/SrkMhG3mv8jcTRmqXV/c3FzVnnq1Z55uQ3sDOnXVObuuPn/"
        "rAwmmzavxDw9ltwjw/"
        "rV1CFenr+yct5TPUtZw1C4mhNQvp9D4wmzWPvvZy+"
        "0tr3Na9kJJ6U0aPtOztN9V6jFGpx6DT081jwzk1WopHHPwzWzGbJ38TMTalJrEcTMV77VjwhGbDhv5m+"
        "rzW775ezBit/+n00ebp28xExFrRbmJiJnti0+MyAMTT7iu/EIf3Tvpa/wFcL/wDiD5jdqLNnq37K/"
        "pn60ebdF595fyPKNpvHHur63qOCoaZtOGwuBqYrVtt2LdMZNkLDIarmrm4u5svs+"
        "Skuq1HIW0lWyltqMlKlUpT1qukya3HX4NVoNn02Lv8AOaHBnx5+"
        "6vFe7JbFNeyeZ7o4pPM8Rx4I7R6bLh1W4Zr9vZqcuK+LieZ4pW0T3Rx4TzMMKO3B7NqHWFwtDm3inBQueo3g/"
        "CXtzYWePt4T5Hk7je2mr5PN6JGSlL7e9z2HqT3mw6LTlhXq1shVzOu29vNX2ilc2UYdg52lceozjSl0nczZ+"
        "NbnTh3A04aPmsvcxjkOTuLcVLTtKElWvXmjUv8AcNApxt8ZmITx9+y+txxWcmhkL2x2nIUuiZkzddjL0jW/"
        "VBN1c6PluZOJ+V4b9JyVaUeNNzwGD1HHbRWnhVzU9lgMhpmZmkxG0V6mRqbJga19Ww+"
        "RoZrL4yWyoYa7hjqVTTbjps215ds3HznGKZzbdqKV85bBl/"
        "8Auw2jmJ8zfnx457Ym3hMxj7aebR5seux63R9vOTjHrMNrdtcuPw4yV8OPOUiP4/TMV/"
        "hN+7D3tp+IOUuirtCuJ+0m4wwlTI6jtGz6BseSvIU7iXDY3lXjzH2GFyembFVtpJ/"
        "y7FcjaPgrOrb3MZ6VfLVau6SW8ktfGxrVuhLpy7VjoW6j9Ew+3YbqD4049zl3jra4z3HfLG6a5x7uusZKaj47/"
        "GXFhtGRxdHN0sfVlqU45vW6+WwlxThTrUr6HtYU5bx8hcd6Jyzpmwcd8majgN60barGbG7Dquz4y1y+"
        "FytpNPJVkkubK7p1KcatvcUqN3ZXVOEl1Y3tC3vbOtQu7ejWp4Pcnfhtehzcdgus3ou6c5cS2d5cT1ZtUwWza7s+"
        "s2FOaeM0KGJm3LWMxtNGWWE0ZITZPacx3Sy04SwljLPGpXprts3HR6XTbrbUabU6HH5jBq9PSMtMmnjjsx5scz3d1I"
        "iIrNfD6bTaOZrNO2m1uj1GfNoa4c2HU387l0+W045pmn+1fHeI44v4zaJ/"
        "1cTxEpS6te3f6T+nff8Aj3j7i2H+qry2d2Glab9V4W2LGZfHafgK8lW1to6/"
        "naNG91ve91vstPaUrDUcVmLa3jbUryXK7BhrurjKF7bTtONY3jljs3+pzD8fYbPUNtz/"
        "AA3NnrfWqltGns0cdirvDbXs+t1cfY1rqarmrjWcbmcPWxVnWvJru9qz4+"
        "2jdxrSQqRP0g9i50R9He1YvkXWdZ2flPk/B1pLvX975hy+O2O61i/kj4oZDWNdw2F13UsXkaFWElTHZmvgr/"
        "YcVNTlmx+"
        "aoVJq1StrMstRn23T59FbbMWbJOkyVy5dRqp7Z1V6ZK5KRGGszXHjr2zXmIi1omItHNe61zhxa3Li1NddfFWNRSaUx"
        "YImYwVtSaWnzk8WvaeYn6ZiJjmJ4nivJ3+HK6r+mLjjhbljgnf980fjLmPM8t3O92Nxu2ZxWr09/"
        "wBSvtR1XB4nHYXPZmvZ2mSyWs5TCZ+pV1mS7jeULbN/"
        "m2OtLindZira659fHandL3SrwvvFfD8saJyNzTl9by+I424v0PZsPt+"
        "fvdqydlXscPkNkoYC7yEmsaxjryrDI5PJ5yex98s7G7sMHTyeZqW2PrRF1Pdgp0L9SG5ZfkPHWO98F7dsF7cZTPTcQ"
        "ZfCY/Vc1lbypUrXeSu9O2TXtixONuLqrUjVuKerR1u0r1/HdVrepc17mtW/"
        "X0n9hH0S9LW9YblCrR3rm7fNavbbK6vectZLBXut6xmrGtJcWGcw+pa7r+"
        "BxtxlbCtTkuLG52SpsUuPvZKORxtKyyFta3VCS1Wo2DV6u+55cuvnJktXLk2/"
        "zVPn5K1rHZ8J7prXDaa8TPE3inPbFZ4rFlgw7tgwV0VKaWKUicdNX32+bjmZ+"
        "d5njmckRMzHjFZtxzz4zOSX4XqnPS5L6waVWSenVp6XxNTqU6ksZKlOeTP7zLPJPJNCE0s8s0IyzSzQhGWaEYRhCME"
        "vfiNekffKuU4k68uLbLI16nG2Lxugco3mGpTz32p22I2S72TjTf5pKMk8YWNrnc3mMDmMpWhCFhc1dQpTQqW9apPab"
        "bdHfZvdPfQ7ufLW9cL3nIlxmOZo4+"
        "O2UNz2PFZvHW0uMyuZzFrJg7fH65hKtlJC6zl5JPC5ub6M1CW3khGWanPUqXtyuKxedxmRwmbxthmMNmLG7xmWxGVs"
        "7fIYzKY2/oVLW+x2RsLunWtb2xvbarVtru0uaVW3uKFSpRrU56c80saep32td/tu2krN8Vq4qXxZI7JyY/"
        "g+PDmpMfO7eZrM0t4xForaYmOaz2wbXadqjQZ7RXJFr2rek93ZfztsmO0fRz9MRaPDwm0RP0SyU6H+"
        "2Q6Rep7ibWMjyNzDxxwfzTY4eytORdB5N2rD6DaR2O1t5KWSymlZjaL3HYbY9dy9xTrZLGW+"
        "OyV1mcXZVYWmasrWvbxq1o06ve3k6SenDb9E0vi64odVOXzGwUKO+"
        "1OHNnxeRwmn6tVpV6XveG2qjRyOsbpuNxkZrSTHalispStalvSvoZnYdfuJsZTv/AD3On4dzoP5Z2W/"
        "2rSbnlHga7yVxUurrXeN8/"
        "hb3RYV680alxUsdd3HX9hvcPLPVjGahj8LncdhLGnNG3scTb20tClRmPpD7EHol6RtzxPJuOxO48xcla/"
        "dUsjrWzcv5TEZix1TK0JoT0MrreqYHBa9r1DJ2tSWncY7KZuzz2VxF7TkvsPf2F1JTqScT8V62vqYncMkWi1qbdNYx"
        "xW9ontpbU1t/6dLT4TW9rxERz5zia25j+m5rXBMaSkxNYtrItN5tWsxzaMNo/"
        "t2iJ55rFZ5niKTxaNOc7Qr8pcPZm1tcZltdueRuNcjQt8Ps9rLi85gq+"
        "36vWp0cZsNjSrXcmOy2MqZCW1y1rTr3UtpeULijJWrQpwnm43Pw9/"
        "VTxD0pcy9TPDfURuGD4dv+ULPQqGIzXIWQttX1+w2/iXK79jsxqGczeVqW2NwOWu6e73FS0/"
        "ObmytK1xgrrHS3UMlcWNnedtrGzrC7Djoz6vOQcxy3eft5w7yTs11UyO25rivJ4S0wu4ZevHvuc3sOr7Fgc9jYZq7j"
        "/Vb7I4CfAVspeTVcjmPzHIXFxdVaG067RY9PuO36/"
        "wA9j02vrhmM2GO++HJp8k5Kc1nxtWZ7eeImZ7eJji02rV1+"
        "l1N8uj1el83fPpJyROPLM1rkrlrFLcTHhW3ETxzMR87nmeOJ0k4r6lunznPPbfrPC/"
        "NHGnLGZ0K2wd3t9vx3uGF3G3wVDY58rSw8bvJYC7v8ZNVuauEydKpb295WuLKpbQp39K2nr20K03svOgXsm+"
        "nns9dk2reOLtr5X3Pd9z1v9ks3mN92TFz4qng/zSwzEbXHaxrOC17ES1Y32Ms6tPIZiTN5Wzl97oY6/"
        "srXIX1vX1DRWsrpKZ7V0WXLm08RXtyZqRjva3bHf82Jnivdz288TxxExMxzN9p7ai2KJ1OPHjyzNuaY7TesRzPb86f"
        "49vHPHMc+"
        "Mcc8QAWyuAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAw77dzkjrV4f6ZNa5G6U"
        "d0zWl6bhtlu7LnvKaXjqcd8xevZKnj4ajn8fscKda/"
        "1rV7DNW15jNpvMNC0yU9xm9flq39HDS5ilW+50L9tt0kdSnGutW/MvKOk9P/"
        "ADnjsVZ2W96zyVmrLStRy2at6ElG72DSdyz1az1i6wubuJJ7u0wV5l7fY8PUq1Mbc2N5b0LPMZPZzIY+"
        "wy9hfYrK2Nnk8Xk7O5x+SxuQtqN7YZCwvaM9teWN9Z3MlW3u7O7t6lShc21enUo16NSelVknkmmljh3z3+Ht6B+"
        "ZNgvtp1G05G4CymRr1Lq7xPE+fxEuk1bmvNGetVo6nt+A2ejhqMYx/qGN1e/1/"
        "D2kP3LbHU6UIU4Tuj1G0ZtFXQ7liyafJiyXyYdfpcdLZJjJxNseprPzskRxEUt86YrFax2RWZvFajDuGPU21WjyUzU"
        "yUpTJpc9rVrHZzEWw2jwpM8zNo8ImZtM90zER4/"
        "tXO126WNB6YuWOIOEOXNI5s5k5i0nYuNsXa8Y7Djtz1zT8PuOMuNf2Xadh2/X6+Q1y0usVhMhf/"
        "k2GoZG6zN1n5sfCtj6GMo5C+tYk/"
        "DX9MO48ZcFcw9RW5Yq7w1t1AZnUMRx5Z5ChUtrq+"
        "0jjmTZo19st6U8ss35Vsuf2q8scdVqwhNdUtWjkLaWOPvrS5u7A8C/"
        "h5ugzh7Y7Da9yhyVz7kMbcU7q0wfKOew1LRZbmhPCpb1brVtQ1/"
        "XJ8zTlnh33GN2PLZrCXskIUbzFVqMalOpufj8fYYmwssXi7K0xuMxtpbY/"
        "HY7H21GzsMfYWdGS2s7Kys7aSnb2tpa29OnQtrahTp0aFGnJSpSSSSyywravcNv0225Nq2rz+"
        "Wupy0y6vV6itaWvGOa2pjxUrxMVi1KzM2iOI7o+"
        "f3816afSazNrKa7XeaxzhpamDT4Zm0Vm8TW172nwmZi0xxEzz4T83t4nP7/XR+jX/Vb/AOop8xr7zp/P/wBjO/"
        "8AZ3JfsV+33svafsD+1vg9y/aj2v8Atf7H2X5Z+ef/AJO/mn5//tYhztqOjjO9YnRXsmM0HE1c1yvw9m7blzj/"
        "ABNnRjVyWyRwmOyOO2zULCWnJUuLi8zmq5PJXWIxlvJPVy2z4fXrCWEsasKkn7Y9jh0ox64IddPvfIUN5hvUOVo8fQ"
        "zWJ8u48qQuPzD9s40fyP8Aabx/n3dtUcRDYvy+Oy/"
        "1z4fyiMcI1iWNtRpNFqdBqtrvntkw48OXPGoisR8Jif6yle2I5xWjmsx4+"
        "EzxaefC5ri1Gpw6vBrq4opkvkx4vNTMz5mY+Ze3Mz8+J+dH0eMeNY/"
        "jyjdh52snCejcIYzpB6ouQMPxTnONMjlqXFe971eyYXTs3p+Yyd1mKmp53Z7+"
        "MmO1vOarmL7KU7GtsV3jcVd69cYvF2dzTvMRNb3NxO1G7ZLps4j6e9+486c+"
        "YtL5i535L1rLabrVzxZseM3LXuPrPYLOti8vu2b2/AXGQ1yhk8Jj7i6q67hLfIXuWuNh/"
        "Lal5jqWHo311TnXq67EPoi6t9wyvJWRwu3cO8k5+5q5DY9o4dymIwdntWUrzzVK+U2TVM7gth1q4yV3Vnq3GRyuHx+"
        "CzGXvKk15lsjfXEZp5on6ffw9fQlwtteO3Pb5uR+fcnibqleY/A8p5fAw0KndW88tS2r3up6tr2B/"
        "PYSVIeKtjtkymZwN5J4aV5h61Pxy1Ji+p6azar+"
        "k8ka6ma141GXb646WxX1HMWtxl8I81fJza0Tas25meKRPYj64d6xYI0VJ0tqVr5qmsm1ovXFx21/q/"
        "p76V8ImItEcR42mO6YE/Dk9Gew8RcMb71U8gYW4xGf5/p4fCca2WQt5rfIUeJ9er3F/"
        "U2OFKrJTr0bPf9huKNzYSVpYyXuF1XB5yzmqWOYtqtXUnqW7UXo26SuZtQ4I5r5FvsDve12uKyN3+"
        "X67k8xgtJxOdu6tnhsrvWZs5I0MHaZCpQr14U6MmQvrLG05Mvk7OxxN1ZXtzoJbW1vZ29C0tKFG1tLWjStrW1tqUlC"
        "3trehJLSo0KFGlLLTo0aNOWWnSpU5ZZKcksskksJYQhDKbrB7HXpV61ef9V6heUb/AJFxOyYrH4PD7hr+oZzF4/"
        "XOSMTrdeepibXYoX2EyWXx1f3SeGEv77Xcpibm6wdKjb0ZrPI0aWVkjZ1mk3PdNRrN2nPiwZq3mtdNxa9JpSKYMfzo"
        "mJrWlYibREd2SItbtra3F7Gn1Gi0OLT6CMV8uOaxac/"
        "MVtFrTbLf5sxMTNp5iOZ4rzEczELm9X3T5ieq7pj5p6fcpcW9pT5P0bI4fD5O5ljVtMRtVpPQzelZ64pySVJ61tg9u"
        "xmDy9anSl9rWo2c9OjNJUnlnl5H+xy67bPs2uZ+aujnrGpZTjDT9g3WNS5yuXsruva8X8vYKjT17J1M/b2VK4r/"
        "ALLbxhLTDUJtosqV7Y2scHrmThH9nMnf5qw7b6VKnRp06NGnJSpUpJKVKlTllkp06dOWEslOnJLCEskkksISyyywhL"
        "LLCEIQhCDPHrS7LjpE67a9vnuYtOymE5GsrKljLPljjjJ22r79DGW8Iwt8bk7q6xuYwOy2Vr3+"
        "Gxp7RgM1VxdKNSjia1hTr3EtXttW46bDp9Vtu40y30Grmt5th487p89O3tzUifCeYrWLx4zxSI7bRNq2667R58mbBr"
        "NJaldVp4mvbk583mxW55x2mI5jjm3bPh/"
        "anxrPFo8d1Mdr90LdPfFWc3vD8+8W81bZDE3NTS+"
        "NuId81zfNg2jPT0IxxeOyNXVr3MW2n4upcTU58rm9jmsqdhY07qa0tsnlJLXE3mC/YOdPPJPU/"
        "wBZXKvaM8vWNetiNdz+/wCVw2duLWpQsdr5x5TjkIbDHB060tSjVxWlavsObjd06c0JcVkM9qdKyqTTWlzLbaHcd/"
        "hsOiHVNkts3um+88coYqzuZK0moZjZNX1vBZGlLN3xtc3d6lqeK2avRnhCEs02Gz+"
        "ArfrNGFT9YQhvVx9x5o3FGma7x1xrqmC0fRtTx1PFa5q2t463xeGxNjTmmqeytbS2kkk9pWrVKt1d3NT2l1fXte4vb"
        "ytXu7itWqXOTX7Zt2h1Wk2m2o1GfXVjFqNXqKRijHp/"
        "GJxYqR87m8TaLzMRHFue6Zita0aaXXavVYM+vjFhxaW3nMODDabzfN4TGS9vGOKzETXieeY4iIibTb2QDGU0MSvxA+"
        "k7tuXZxbncabbX17a6VyPx5u28WmPkq1q1TScdd5LF31zWt6PfPWscTmczgs3kJ4yT0rGyxlfKXHsrexq16W2r8eRx"
        "2Py+PvsTlrGzymKylndY7J4zI2tC9x+"
        "Rx97Qntr2xvrK5kq213Z3dtVqW91a3FOpQuKFSelVknpzzSxutFqZ0er02qikXnT5seXsmeO+"
        "KWiZrzxPbMxzEW4ntnieJ44UNThjU6fNgm01jLjtTuj6azaOInjw5iJ4mY5jmOY5jlz59g31fdKlt0O6DwXc8k8ecd"
        "cwcd5vfZt51Pbthwmp5naLjYt2z2yYvcsPNmrqwm2qyq67k8Nhry6sZ7u4wtfDy4q9pWtlSxE9193ti+"
        "076cNG6WuWenvi7kjUuXubub9Ry3GlDWeO85j9wtNP1zabWfGbbntwy2Br3+Mw9xQ1y4vrbD4GrdR2C/"
        "ymQxt1JjJcNRyGStP283fh1uhLlTZ8htek5Hlfgutk7mrdXOsce5/"
        "BZDR6VevNCpXnxuD3DXdgyWJpzVYzzUrDG5+2w9lJP7vYYy1tadGhSmzpm7EDoh6Z7Ha6+Mw+"
        "6clbvtun7RpNTkTkrM4jKZ/VsNueAyOs7DNomOxGv4bWdcyV3iMpeWtLN1cJlM/"
        "bUKta1pZb3K7vrW6yDJqOn7a226Tm1+"
        "XJfP8ACvgFsNK8ZpvGSa31Hdas4YvMz21ibdkdsTP8YmmLdo01dD5vS46VxeY+"
        "FRktbnHFezmuLtifOTT6JtxXu8ZiFJfwxf8AasdQn+MBS/zdaipj26f9146Kv8GPTj/"
        "4nOXXSz0S9CHCfQNou3ce8H3u9XuB3TbJdzzFTfc7jc/kJMvLh8fg4SWVxjMDgKVGz9zxtvNGjUt69T28as/"
        "tvDNLTljjqi7L7pu6ueoPjXqV5UyPJ1ryHxXhNOwGs2+pbPh8RrdWx0fdtg37DTZXG32r5e8uq8+"
        "c2XIyX1Shk7SWvYS21vTp0KtKe4qse8aOm/"
        "6zcp878Gz4stKcUjznN8FMdead3hHdWefH6PFzbQaidq02kiKeexXxWvHd83imWb24tx4+E/V9K3nUB/"
        "vD82f4I+SP8jc04Gex466ZegrnzM7/AL9htgvenfkmyxHF/Lecw2LvMlJqGXu62Qz+ibRClbU4+/"
        "ZDGRxGz+LB06k2SyOr3G2XmGscnk8Vb2c/fN1Af7w/Nn+CPkj/ACNzTk5/DqcI8UdRHFHX5xPzTo+E5C4/"
        "2W56cKeU17OUak1L21GjzjPaZHHXtrVtslhszj6sfb4zNYe8sctja8IV7G8oVf3lTY8unxbLvl9Vitm0/"
        "foKZcdLRW80y5ZxTbHM+EZMffGTHzxE3pWJmImZdNzx5cm5bbXBkjFmimqvjvaJmvdSkXit4jx7L9vZfjme208RM+"
        "DpMtu0h6BbvS5t/o9YfTv+zktl7/"
        "PCtynqlrsVOn7ONSW1q6XdZGjudLLTySxjTwdTAS5mpN3SU7CaeMJY8mHVDyRlu3G7TjjHjLg3HZ+"
        "HBel2tlqFlsd1Y3dlUseMcXnps3ypzFlrG5p+LAT5mW8kxesY/JRs7rI+5aPib6hjtgy9ewoa75/8ND0U5TaKmVw/"
        "KvUVrWt3FzGvU1Sz2HQsjC0pzT+KNlic5l+P7zIULWST+"
        "p0ZstJm72WH71a8uJv1a99JfRD02dEmm3endPvH1rrP5zNbVdq2zJXNbObzuVzZyzy2tbZdnvvFeXVC1jVr1LHD2Ut"
        "hr+"
        "Lq3V5UxWIsZ7y6jWp4NZsu0Rl1O3ZNXq9dfFfHp51GKuLHpfORxbJfjjzmSseEdnNbRzX5sW7nbLp9y3CceHWU0+"
        "n0tL1vmjFe175+yYmKV5/sUmf/"
        "AHcTE8T87iIfg677CzxfQF1lYzHW9K0x+O6PeoewsbSjL4aNrZ2fC24W9tb0pf18NKhRpyUqcvfHullhBid+GF/"
        "tdupb/DTr/"
        "wDkNYujTljjbXuZeLOS+"
        "INtqZKlqnKugblxts9XD3NGzy9LXt513JaxmqmKvLi2vaFpkpMblLmaxua9nd0aF1ClVqW1eSSalPWXoj6B+D+"
        "gTT900jg293y9w297LabVm599z+Mz97Jk7LF0sRRlsa+"
        "MwGAp0LWNrRljPSqUK881aM08KsJYwkhG6fXYcezbhob986jU6jT5cfFeaduK1Zv3W58J8J4jieZXuXTZLbjpdTWK+"
        "aw4c2O/jxMTePmxFePGP0XZcanQb/5xP1Kf4T+sL/"
        "vbYXZWzm4r7L7pu4f6v9062tUyPJ1XmPe83yHn81Z5faMPeaVJfcm3F3c7HCwwlDV7PI0KFOpe1oYynVzdxNay+"
        "CFapdRljGLbNbg0mn3XHl7+7WaG+nw9te6POWnmO6eY7a/j4/6nGu0uXUZdBfH28afVVzZO6eJ7I457fCeZ/DwY/"
        "figdK3bKcXdJ+/Yu2vq/H+n7pynr231reWrPZWWx7ridGutJuMhCnCNOl7Wz0/"
        "cbS1ua8JZKdavG2knhUvZZKmq3Z09avR/yF0f8DY/SOW+LNNuuP8AiTRNN2zjrPbdrmqbFo+d1XWcdhc3Z5HBZi/"
        "x9/"
        "HHT5OxvK+O2OW3nx2wW08MlQu6tSrcS09DuR+NtC5f0fZONeT9Twm86Ht+"
        "OnxWyatsVlTv8TlbKapTrSSVqFSHip17a5o0LyxvLeeje4+/t7a/sbi3vLahXp4K7/"
        "8AhrOiLZthuMxpnIXPnGuLu7matNqmM2TVNlwuPozTzTe6Ye82nUMjslKlJJGWSnPmc7nriHh8VSrUjGK7wazbdXte"
        "n23cMuo0l9Hmy5MGfDijPjyVzWte9cuPurbvi1pisx4RWI+"
        "d4zWaGXTazBrs2t0lMWojU48dMmLJecV6Wx1rWs0vxNZrMVibRPE8/"
        "wAPolVDt9e0k4F5Q4WtukbgTdMHy3nchuOA2/"
        "lTb9Jv6GxaRqGA1SvUqYzAU9mxs9fD5bYMts15iJ7iOIvb60wlDGV8dlKtHKZO1tqWrHYR/wByy6Z/+c82f+IDlN/"
        "bRexH6FePun/k/gDDazulzR5ix2vYff8AlnKbFjL3mDJYnWdx1ve8ZicZsM2uy69reJjseqYa8v8AG6/"
        "qeOtMpG1o1MlSu7u0sLq0vn0tdNfHnSFwXpXT3xVcbJdaHoc+y1MJX27JWmX2Ceba9tzu55P3/"
        "IWONw9rXhLl9hv5LWFLHW/"
        "srKW3oz+1qU569Rrtftk7PTbNDGo5xa+"
        "uonJnrWJz18xel80xWZjHM3vFKYvGYx0ra1u6ZiGl0utjcb63VTh4yaScUUxTMxinztLVxx3RzeO2s2tfwib2mIjiI"
        "V20DtR+jXkzqrzfRzqXI17e8w4fK7DrlGatr2StdNz+16lSva2z6rr+1VZPdL/"
        "M4eTF5WE81Wla4vJ1cbdUMHk8pWmtadxIvXH1scTdCPBua5j5OuoX1/"
        "PGtiOPdCsrujb7ByLudS3nq2Ov4r2ktWNpY0YSwvdiz1S3r2uAxFOtdz0by9nx+MyFY+L+xw6UeJus/"
        "K9a+"
        "tXfIVbc7rZdp3fB6BkM1iavHerbrudPJU89ncVaUMHQz9elLNmMnc4XEZHO3eOwt3ez1aFKrStMTQxk59cXZ3cB9oF"
        "jOOsTzrfchWNrxhf7LkdbjoGxYrX6tS42q3w1tk4ZOfKa7sEt1Tlp4Kyjay0ZLWNKaNeM81WFSWFOztXZY1uj7L6y2"
        "h81jtrZtWvnpzRFpyUxxHb8y0xSszE817rzS0xFVxE7lOm1HdXTxqvOXjTds2835uZrFb3mZt86Im9ojjx4rFoiZlx"
        "pdP2e487SbrS2XqO7SbqU484z40wt7j8jkdR2Hb6etXG0Yy2ubitrPDnG+"
        "Lnup77XeP8AE0ZasdlzdKrJkZ6Fxcxo311t+y5DZMf2IYDtKuzS1XB4fWdZ6r+nXX9c17F2GEwOCw+"
        "2YbHYnDYfF2tKyxuLxmPtJKVrZWFhZ0KNraWlvSp0LehSp0qUkskssIUQ/wBjd9nz8f6jf+0fU/8ARsf7G77Pn4/"
        "1G/"
        "8AaPqf+"
        "jZObjrtg3K2KMmr3HDg09Ix6fS4dNhrhw1iKxPbEzMza3b42mZniK1jitYhF6PTbto4v26fR5MuW83y58ma85ckzPP"
        "FrREeEczxEeHMzPjM+OuHCnVz0y9R+VzeD4I5w475Wy+t4+"
        "3yuex2l7Ba5m6xWNurn3O3vb2lQj4qNvWuv6hTqTfpNU/d/"
        "iwk7b3tdYcJ43YOj3pm2bw8x5uwnx3MPI2Du+6rxVg8jb/"
        "1XTtcv7efxUeRs1ZVofmmToVJa2kYm4hJZzybVf0brWNSOiLst+mvoD2vd9x4NyPKF9lt/wBesNZzkm+7Rhs/"
        "ZyY7HZL80t5rCjjNW1+pb3Mbn9KlWrWuJJqX7kKUs376uHNPYKdEPPHLfI/NG7Zrneht/"
        "KO457eNko4LfNasMNTzOxX9bJX8mLsrnQr+4tbGW4rzwt6Fa9uqlOn4ZZq9SMIzRjNBfYtLuc5s1tVqNFhpS+"
        "njJipNsmeO2Z89SJis46T3TWvMxaYr3cxzW17qq7pn0UY8dcGLU5LTXNNMlorTF4/"
        "+naYme60cRafprHd2+PExl72MmudnF0f6vZdQ3PnVX0+X3VBueJn/"
        "ACnDXe74i9o8H6xlreMtfCWVSE1WhPyBm7OrGluGboTTxxVnVqalh60trNsF9sO/P+ukdnd/6Y3A/wD7cY7/AP6z6/"
        "2N32fPx/qN/wC0fU/9Gx/sbvs+fj/Ub/2j6n/o2Xmuz7DuGpyarU6/db5Lz4RGnwxWlI/"
        "s48deZ7aVjmIjxmZmbWm1ptNrfS4t10mGmDDpNDWlfpmcuTutaeO695/ja3jzP4RERxERO5mhb9pfKWna/"
        "wAg8d7NiNx0narCXKa5s+Bu5L7D5rHT1KlGW8x93T/cr0JqtKrJCeX9PFJND+89eh/"
        "gDhHTem7hrj3gzj6tm7jS+"
        "NMBT1zXa2x31vks5Ux9K5uLqWbJX1pY422ubn2t1UhGpRsbaSMsJYezhGEYxmBi2SMcZMkYptbFF7xjteIi9scWnsm"
        "0R4Raa8TaI8InlO075pSckVjJNa98VmZrF+I7orM+"
        "M1ieeJnx4AHR2AAAAAAAAAAAAAAAAHKV26HS91Hc2dbnSzufEHBnKnJupazx5qWP2LZNG0fYdmwmDvrTlvZcrc2mVy"
        "WJsLq0sbi3xlzb39alcVac9O0rU7iaEKc8Jo9Wov8AbdwvtmqrqseOmS1aZMfbeZivGSs1mfm8TzET4fj9K11mlrrM"
        "E4L3tSJvS/dWImeaWi0R4+Hjx4gCwXQ5TO056X+o3kjti+lHlvj/"
        "AIN5V3Ti7V73pbn2TkHWdH2HNafgpNZ5oy2Y2KbK7BYWFfGWMuExVWlkcpG4uacLKyqSXNx7OlNCaPVmL/"
        "btwybbnvnx46ZLXwZcE1vNoiK5YiJtHbMTzHHh/"
        "D61rq9LXWY6Y73tSKZceXmsRMzOOZmI8f4Tz4gCwXSrHWvyVzbxH0vcvb705cbZjlfmrD637LQNQweLlz17HMZO+"
        "tMZNn46/"
        "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"
        "SelUl74eKnUlkqS98PFLDvg+"
        "ixfLiy4Ml8ObHkw5cVppkxZaWx5Md6zxal6XiLUtWfCa2iJifCYZ5g1GDVYcWp0ubFqdPnpXLhz4MlM2HNjvHdTJiy"
        "47WpkpaJia3paa2ieYmYAFNVAAAAAAAAAARfyztFzrWsRhYVJqOQy1x+"
        "X29eSPdPb0o056l1Xpx74RlqS0pYUac8vfNTnry1Je6aSEYUwjGM0YzTRjGaMYxjGMYxjGMY98YxjH9YxjH9Yxj+"
        "sYrKdQM8YUdVp9/wCk9XMzxh/e76cmLlhH/qhVm/"
        "8AXFWp6r8ku36fS9I6bWY8dY1G5ajWZtRl4jvvGn1WbR4qTb6ezHTBM1pz21tfJaIib2mfAP8AlC7vrNd5RddtubNe"
        "2k2TR7bptHgm0+"
        "axTrNBptx1GStOe2MubJqorkycd96YsNLTNcdIgA2a0aAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAkHjXarrWdmsfDVn/Lcnc0LHJW8Y99KenXqQpU7nwxjCWFW0qTwqy1IfvezhVp/"
        "2NSaEbvs5KM8aValUlj3Rp1ac8Iw/jCMk8JoR/"
        "8AXBo286eWrQafBuOy7hix0pn1+n1mHU3rEROWdFbSzivfiI7rxTVTj755tNKUpM9tKxHs7/"
        "Jh3fWarZuptnz5r5NLtOt23U6PHe02jTxumPXRqMeLmZ7MVsmgjL5uvFYy5MuSI7sl5kA0k9RAAAAAAAAAAK39Qf8A"
        "JH+f/wChVb1kOoP+SP8AP/8AQqt7115L/wDMbY/8T/5xuD51+Xb/AEq9Vf4H/"
        "wBObOAM+"
        "ajAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAGkDN9pA0H5cP7r/41/wBpeuP8lj+/f+7H/"
        "kIA0G9cAAAAAAAAAAK39Qf8kf5//oVW9ZDqD/kj/P8A/Qqt7115L/8AMbY/8T/5xuD51+Xb/Sr1V/gf/"
        "TmzgDPmowAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABpAzfaQNB+XD+6/+Nf9peuP8lj+/"
        "f8Aux/5CANBvXAAAAAAAAAACt/UH/JH+f8A+hVb1kOoP+SP8/8A9Cq3vXXkv/zG2P8AxP8A5xuD51+Xb/Sr1V/gf/"
        "TmzgDPmowAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABpAzfaQNB+XD+6/wDjX/aXrj/JY/v3/"
        "ux/5CANBvXAAAAAAAAAACt/UH/JH+f/AOhVb1kOoP8Akj/P/wDQqt7115L/APMbY/8AE/8AnG4PnX5dv9KvVX+B/"
        "wDTmzgDPmowAAHteP8AQ87yRtFjq2ApSxurrxVrm7reKFpjcfRjL71kLyeSE0ZaFCE8ksssIRnr3FShbUoTVq9OWND"
        "U6nBo9Pm1eqy0wabT4r5s+bJPbTFix1m172n+EVrEz/"
        "GZ+iImeIXWi0Wr3LWabQaDT5dXrdZnx6bS6bDWb5c+"
        "fNeKY8dKx9NrWmIj6Ij6ZmIiZeKGg2UsOnLp6p0MRmsN5hbtCjSq3lO7sbPL3NKapJLPTq3Fpf1ZcJg6M/"
        "f7W1oSSXGVloT0qlSNzJGncTfhxnLnTbyHc09e2rjTH6pLfTQtrbMTYzFWlvQnqRhTpwrZnCe5ZLGQmmjJ3VvBGzpd"
        "0Z7qvRpSRnYNXrnU6jFOv0HSW/"
        "67Zoib13GlNPiyZ8MfTqNJoMuWup1GG1ebY7R2TevjMV8YjatvJXodJnjad38oXSG19SWmuO2zZMus1GHS6m/"
        "EV0mv3bBgtotJqaWmMeak+"
        "crjvzEWvHEzQcT7z3wvNxNnLKrjLqrkNU2CFxVwt1XjJPd2tW39lNc429qU5ZadaelJXo1ba6lkpy3VCeMPZwq29aM"
        "YCZhte6aLedBpty2/L57SaqnfivxNbRNbTS+PJSfGmTFkrbHkpP8AZvWY5mOJnW+/"
        "bFufTW763ZN4086XcNBljHnxd0XrMWpXLiy4slea5MOfDfHmw5K+"
        "F8d624iZmIAJBEAAAAAAAAAAAAAAAAAAAAAAAJn4a4cu+Yb/"
        "ADlha523wc2Fs7S7nqXFjUvoXELqtVowkllp3NtGnGSNOM0ZoxmhNCPdCEO7vWW47jo9p0efcNwzxptHporbPmtTJe"
        "McXyUxVmaYqZMk85L1r82k8c8zxETMSezbNuXUG56XZ9n0s63ctba9NLpq5MOGctseLJnvEZNRkxYa9uLFe/"
        "N8lYnt4iZtMRMMD9F3bxtbq5tYzQnjbXFa3jPCHhhPGjUmpxmhCMYxhCaMvfCHfHu7+7vi/"
        "OvK2i1YtWeYtEWifriY5if9sI61bUtalo4tS01tHhPFqzMTHhzHhMTHhPAA5dQAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAfvxX5X+aY3879/wDyX3+z/N/"
        "yr3f80/K/eKf5h+W+9/1p7/7p7X3P3r+t/ePZ+3/"
        "qfiWF5f4Wx+GxVvyXxheQ2HjDMyUrinPQqVrm71yerP7GahexrQ96nsZbqE1tCteS07/"
        "G3nixOYpSXdGS5vIvWbxotBrtBoNXbJgvuc5Mek1F8cxo76mk07dFbUc9mPV562tfTYr8efjFkrS05IrS89t3Te57t"
        "tW77tt1cOqx7JGHLuOjx5onccWiyxki+500fHnMu36W9KY9bqMc2+"
        "DWz4b5KRhnJkx1rASiBAAAAAAGkDN9pA0H5cP7r/41/wBpeuP8lj+/f+7H/kIA0G9cAAAAAAAAAAK39Qf8kf5//"
        "oVW9ZDqD/kj/P8A/Qqt7115L/8AMbY/8T/5xuD51+Xb/Sr1V/gf/TmzgDPmowABoH0x2lno3EHIPKdxbyVb+MmWnt/"
        "HCPfUx2tY6Nxb2sk36TU/fctWuaVaEkYQqRo20Z5oxpS+DPxoTxVLHO9JW7Yuxh7S7sLTdKFWlT/"
        "WpNXo0Zc3LShLD9Y1KttcUpZJf4zeOWWHf3wa+8pUzbYtFpbWmul1+/"
        "7Po9fMTNY+B5M9smTvmOOK+cxYuZ5jx4bi8iNa06s3PXUrW+v2rpHqPctorasWn+k8Wlpiw9lZiYteMOfUTEcT4cz/"
        "AAUFyuUv83kr/MZS5q3mSyd3Xvr66rTeKpXurmpNVrVJo/"
        "wh4p5o90sIQlll7pZYQlhCEPwAz+"
        "lKY6Vx461pSla0pSsRWtKViK1rWscRWtYiIiIiIiIiI8GoMmTJlyXy5b3yZct7ZMmTJab3yZL2m173vaZta9rTNrWt"
        "MzaZmZmZl9PJ5vMZqa1ny+UyGTmsbO3x9lG/"
        "u6917pY2lKWjbWdtCtPPChb0acksslGlCWSHdGbujNGaMbE8PdPE+8Yaru+"
        "65n9ktEt5a9WS7mmt7e8yVC1jNLdXVK5voRssbjaE8k9ObIXNO4hUq0qlOlbRkhGvLWyztpry8tLSWaEk11c0LaWeP"
        "6wlmr1ZaUJow74d8JYzd8f1h+kP4r69XuRn1fUeO+O8J4rLA1aV1NXtqUYyS1bXW6GLs8Ta1PBCWFWlJNd1bipTm/"
        "djcULatNLGeSSaXCup9frsGr2DpnZMuPbtVv2bWRbXVw47/ANBt+GNRq7afDaIxTqctb9uGbRNazF/"
        "Ct7UyU2f0LtG1anb+ruuOqdPl3rQdJ6bbpx7Xk1OXF/"
        "S277vqp0e3Y9ZqaTOf4Fgvj79TWkxa9Zx8zfFTJhyfPteNekvZ7qTWtf3rI2udrTwtrS6myN7RlubuaPgpU6dbM4il"
        "h72etU8MslCzqyT3E00KdvNCeeSKtnLvEGwcR5ylj8nUkyOIyMtWrhM7QpTUaF/"
        "SoxkhWo1qM01SNpf2vtKUbm1jVqywlq0qtKtVpzwmhEn8P4NB9suKnIfSBjNkzcY3OawMllXo3tb9+"
        "tPc4fZamrVLipWmh46lW8xM9WavPGMY1bmp4qkZp5fEjNRG7dHblsd7b5uO97PvW6YNl1eDd74tRqtLrNZW/"
        "wTVaXU4sWG1cXfjvGXBas0ikTx3XvW+Kc0c9P+UnZOqsWPpbZel+pOmdi1XU23arp3HqNJoddtu23xRuGg1+"
        "iz6jUUvqJx5sdtPqqXrknLMTPZjpkx54I4C4cwXLGO5BqZW4zVHIa3ZYifB0sVdWNtSubzJ0NgmhTvYXlheRqU418Z"
        "aSyQpVLWMss9aE88fFJNTkvG8N8CaJJZ4jl7e4VN0uqNGe+"
        "xWPvbmlj8NUuZJalO3qz42yuq1GenLPLGa8yd1a0a8nhr07WlQmhNP9rojqQpUuU6sYRjCnT02pGEP4xhJLt00YQ/"
        "5Y93co1lspe5vJ5DMZKvPc5DKXtzkL24qRjNPVubutPXrTxjNGMf3qk83dDvj4Yd0IfpCCn5re9+6t6t2mu/"
        "7hte07Z/Ql6U2+2LHq4zaza8eSuPDqMmPL5jTzkpnzaitKd+bJfHHfWsWhW8/"
        "wBMdJ+T3ye9Q36R2ffuoN9jqjFlybxXPm26dPtu/"
        "ZcN82p0eHNh+"
        "FayMOTS6bR3yZPN6fFTLbzV72rK4289JVxLntZqcc5abI6pstxCndX2QqUbyGvW8bWpkIZOe8s5aNHI4q4s6VX3KpL"
        "JRnmuvdLOe4qz31GvD7+E4k6Wby/"
        "paVJv17l9rrTws5b2jlo20lxkIxjJGjjriTGTa9UqT1v3LW0hcX9apP4KMJ7mrHvn+zitmytp0X3WRpXVWW9t8dd6/"
        "TuZZ5oVZbC83X8h9lLP3wmkhSxd7NaU4yxhGSlTk8PdGWDPWjWq29alcUKk9GvQqSVqNWnNGSpSq0poT06kk0O6Ms8"
        "k8sJpZoRhGE0IRh+sFjsWk6n6j0+66XW9U6/Rz09umv2PTanbq4cGfX6rR37/"
        "AIZuV5pe2bFGLNp8ddPjti74re2XJbLabzKdVbj0P0ZrOn9w23oLaNyjrLYNo6p1ui3m+"
        "p1Wk2nb9yxebnbNkxVy48em1Fs+m1ma+szV1Hm7ZMWPBhpp6xjiVuZOKb/"
        "iXa44SvcxyOKv7f8AMMHlI04UprqyjVnpT0bmnLGaSnfWdWSNK5kkmjLPJNQuZZaclzJSkkzjfgKw5F4iy22Yyrl6m"
        "7U89NiMRYyXllRwc8klxhpZ6t9TqY+pdyyUra+"
        "u61etTvpJaclGWpCnNCnNJUkvrInhfYDifKVZZferihnJ6k8ssJe+F3Za7c1Jf0/4MKkvfLL/AAl75u7u749/"
        "3uBs9dax0y73n7CbwX+Kutvu7Gp3Qm9leyYbGwtK0ZYwjCaWjcTUqs0sYd00skYR7u/vU8/"
        "U2+6joLYN2wZ6495z71o9BmvHFMWqyYdw1OjtXPFIrHmtXOCltRXHFI+"
        "feKdscRFfR9D9J6PytdX9O6vSXzdNaXpjcd20+K0zk1GgxanaNBudb6W+"
        "WbWnUbfGry00d805Lf1eOc05LTaZ8lLxL01aDNLhORuQq+U2qSEtPJUcfXvZbTH3UYQ8dD3bCY+/"
        "r2U1KMe6MMree1ml8NWehQlm9nDynKvTni8RqseReLM9PtOoyUJry+"
        "tqle2vrq0sZZoy1shZX1nTo0ry0s54TyX1tVt6V5jpKVSpWnuIUrmNvUyrVq16tSvXqVK1atUnq1q1WeapVq1ak0Z6"
        "lSpUnjGepUqTxjNPPNGM000YzTRjGMYrz9GGauLufetKvf67wtxj7XLyWdfvqW9OrVnnxmSkhTmhGSMuQt61pJcSzf"
        "pPC0k7ofrPGN/vmk6g6V2+3UuPqbct2y6LJp8u7bdr66aNu1mmy58eHUV0WnxYaf0fannfOYprkvNa1mOZ/"
        "s2iOltx6Q6/3enRGfojY+ncG6YdZg6f3naba2d523W4NLm1GkybprM+oyxu+PJGCcWeuTDhi98kWiK/"
        "26Ubtra4vLm3s7ShVubq6r0ra2tqFOarXuLivUlpUaFGlJCM9SrVqTy06dOSEZp55oSywjGMILr4fpy480PAWWw867"
        "hHFXV/CEaOBx93LQp0Z/BLPUtY1Le2vsnmLuhJNJ71+VUaNvbTzeCFW6pxp15/"
        "G9OeoWE3UBe4+vLCvR0ybaLyzlq908J7jF3sMNZ1Zu/vhNUoTXst3Smh+stejTqS/rJCLw/"
        "Ufs2Q2Tl3a5butUmtcDefs/"
        "jLaaaaNK0tMbJLSqwpSx7vD71exur2pHujGapcRhCaNOWnCEhumu3HfeocHTe17jn2jQ4tox7zumv0taRr81NRljFp"
        "dFpMmSt66a0xMZsmeKWnjmkcds1yQ+w7VsvSnR2r6237ZtL1FumfqHL03sW0a++Wdo0+XR6ec+v3PcMWC+K+"
        "trExbTYNLbJSkWjzk8zet8M7R4C4V5Ox19V4Z3qrJnLGjGt+"
        "W5GvcV7WeXv8MnvdjkLGyz1lQq1Iy0fzGSF1b0ppoQ92rTxhLGledweV1rMZHA5uzq2GVxV1Us720q93ipVqcf4yzS"
        "xjJUpVJIy1aFanNNSr0Z6dalPPTqSTR9NxhsuQ1Lf9TzmOrVKVW2zePpXEkk00IXWPu7mna5CyqQlhHxU7qzq1qM0P"
        "DNGWM0tSSEKkkkYWP60cHaWW66xnaFOWncZzA17e+8EIQ9vWw91LTo3FTu/WarG2vaNtGaP/"
        "AMVa0ZYf2Bt+fdOn+ptH05rdz1W87bvOi1Wp2zU7h5u+46TVaCIyanT5tRjpjjU4cmGfOVvekXrea0rFa1vN+"
        "d40mxdYdDbl1ntmxaHpne+mtz2/"
        "Rb5odo89j2bcNBu1pw6LWaXSZ8ua2i1OLVR5m+LHktS+"
        "OL5b2va1IxfxiOmCx2zjjjzZdcyWSts3st1a1dmucpc2VXB4bBxs8vXvshb2lKytbyarSuLOwt7ajPkKktSpdeCrPS"
        "pzTXNv6PHcadJlO6pavc8g3WTz1SeW1myUcxVt7We7n7qcfd7+3xcNdpyRqx7qMlW6uYQj4ZJ61aaE00329tzd/"
        "hejzV5cfXqW8+YsMNhbirSmjJU9xu726rXlGE8sYRhJdUbWa0rw/WFS3r1qU0PDPFnuiNj0e/"
        "8AVGPecuq6n3XbtNod+3bQ6DHtlsOHPacGoma31ee+"
        "O98uDFW9MOHS0nFWKY7WtebX5ZD1TuXSHQuXprBoehdh3nXbr0n07uu75d7rqM+"
        "lpGq0Va3w7fpcWbFjwavPfFl1Wp1+WNRecmelKY4pj7ZnrnXhK74hy9lPa3lbLaxm/b/"
        "lOQuJJJLy3uLbwTV8dkYUZZaMbinTqU6tG5pSUaV5SjUmp0KU9CtTk8lxXxZn+V9i/"
        "I8NNTtLW1pS3WZzFzJPPaYqzmn8Es80kkZZri7uJoTSWVnJPJPcTyVJpqlG3o3FxRtZzRdVc30s8Y5XITxuL+"
        "W41Kaa5qfvVqlWTBZaxqVKlSPfNNPXkl9pWjGP9Uqd083fGEO71nTJg/"
        "deC9lyGNyVpgs1sl9npJdhu4Swp4ue2tJMXjbmrNGelCNLG1/"
        "b3tGSepLD21ep3zQlqKXxy3XQ9D5dbqcuHLvWDec3TdddfFM4bajHqLY/"
        "h+TBjrPdOPTUvkmlaWjJmpEzjtF5xzXr5Nen918qeDbNFp9Tg6Y1fTel61ttePURGpro8+"
        "ix552nDqsuSnZXNrclMMZb5aWw6bLaK5qWx1zR4W8406UdKuptc2vespf7BRmhb31andZCtLaXcP3atOpDX8Rc2GPm"
        "p1O+We2v7itXto/"
        "uXE8YyzRR1zD09Wep67R5B48zk+"
        "1aPXlo1biaerbXd3j7e5qQpUL6le2UlK2yOOmrzS29aeWhQuLKpNThWlryRr1rf78/"
        "SnZzzTTz8zaZPPPNGaeeeWlNNNNNGMZpppo5mMZppoxjGMYxjGMYxjGPesJoWn4XQOK9z0XO8haxs2PydrnatpLRvL"
        "ShCzt8jiY291aUqVW/uJ4wmuKc13ShSjL4bitVqSSwqzxmjEZeoqbPbQ6/"
        "a+sd66i1M6zTY902vcNDqY0ur0ua8U1N9BS234K6DJi585irGa/"
        "hHEzeInHkyTT9GZepMe7bTv8A5N+mejdDG3a3NsO/"
        "bNumhncNu12nx+"
        "c0WHdclN41dt2w6iI8znvbS4pmbd0Vx2tXLhpt088Wa7yvs2bw2yXWYtLXG4KOToT4a5s7avNcQyFna+"
        "GrPeWGQkmpezuJ4+GSnJP44Sx8fhhGWMqYzhHhvj+3tvO7dZrXYclLG4t9asLqt/"
        "tdY1ak0ttNkPyqzvr6rXnll76l14rLHy1IVqNH3mFGNxH8vRX/ALvNr/6Ix/"
        "75xiuHKGXvc7yLu2Tv609a4r7PmacIzzRm9nbWt9WtLO2kjGMYwpWtnQoW1GXv7paVKSWH8GU5671vPWO/"
        "bLi3zXbXtOl0G1am/"
        "wABnFXWVy5cV4ri0ufLjyRpsea03y6m9aXyXnDix1mlb3mcC0d+mOmvJt0p1Nn6V2vfeoNfu+/"
        "6HHO6xnvt1sGDPhm+"
        "fX6XBmw21uXTY648GhxXy0w4o1OozXrkvTFEWe5B6V7S6ttf2Dh3KVM7hs9e2NtUt7u7o3tCytcjPCSlm7bJ21KSNX"
        "E20Yw9/"
        "p1ada7toR9rJPXhCpSofUx3EfTDhb2lp2y8hVsruE9SWyvLilkqlhYW+"
        "SjN7Ke3o1rWyr4mwmkrR9lG3yeSua9OrCMlaaE3fTl9BwVsuUx/"
        "TByDf29zVkutan3ajhqss0YTWU35BYZSjPRj398saOQyVe6lhDu/qk0Y/wAYxjHPOMYxjGMYxjGMYxjGMe+MYx/"
        "WMYxj+sYxj+sYx/"
        "ij9l0fUm95N62fW9U7jpsHTm45tBp9boK4cO47he8edwZdwz2pebU0+Gcceax9k57Zb+"
        "dzTOOl5lup9x6K6Ww9MdR7Z0Hsut1XWezafdtbte721Op2baMeOY0+qwbPpKZcdaZdZqa5rRqM/"
        "na6WmHF5jT1jPkpWdud+"
        "FrjiHOWMtpeV8prOckuKmHvrmWnLeUKtrGnC6x2Q9jLJRmuKEtehVp3FOnQpXdKpGanRpz0a9OS53TfgeIsTNk7njz"
        "Z8nnc5eYPDx2Wzvp4z0cfN3zVIwoQjhcZCH9eTV6X6XF1+7JD9f8AhzRh1B3FXLdN/"
        "D2ZvZo1shXr6fGvcTx8VSrUvNKytW7qTTx75ozV61tTqz98f3poQjHvjCD5HRL/ALod7/8AqbEf+/"
        "XSH3rUbjvfk01et3DcdTGs2jPqdBrI004sem3W2n3TT6WmTWY5xWtbspWmSvmr4uc3de3MW7IyPpnR7N0t5b9t23Z9"
        "m0Ntt6h0mh3Xbp1sZ8+t6fprdi1evyYttzRnrWs3yWyYbzqMeo40s0xV4tTzk/"
        "F2zSelujj9lu8ZyNsVxsdKzzNzj7CpWmjQr5unQuatpaTw/"
        "ZKlCNGrfwp0Zoe9U4eCaMPbyf8AlIR1wfwfV5VqZTL5fK/s/"
        "puvzQkymTl9j7zcV4UfeatpaVLmPutpLbWkYXN7f3UtWla06lv3W1f2s8aMIZj/AOFsp/8AWN7/"
        "AO81Vu+m3kvRrDUds4p3u/"
        "kwlhtNfJ1KGWuKsLWyrUM3iLbC5Kwr38YRpY2vJQtZK9pd3Xhtpo1Kss9anUpUKdfLt30++dP9L6zNtW57zu+"
        "sy30U2zaqun12r0Gjm1KarLoNPi0+GuS2PFM27b1ydvjl5jsmY1507relesOu9t0+/"
        "wCx9NdO7bp8e51rptBfWbVt+"
        "77lWl77fg3XV59Zqb4aZdRWtJyYr4ovzGHi05YrP3bXQekHLXUut2G9ZSnlqtSFpRyNTI5C2p1LmaPs5IyZHIYKnrl"
        "WM9Tu8EZI+"
        "xqzTQlpRj4pYQrnzLxLkOI9npYeveyZTF5K2mv8Lk4U4UKte2lqzUatC7toT1PY3lpUhLLVjJPPRrU6lGvTmkjUnoU"
        "Jm2Po/wBlhRnyfH+0YLcMVVhNVs6davJjr6tSjGMadOhc057rD3c0sO6Sa4mvrCnUm/"
        "fhSpwjGSWsW3WG2YvO3WP3Wnl6WwWktGhc083Vr3F7LRp0pZLWEtevUqxrWkKEJIWlSjVqW01CEkbeeal4YuOlMun1"
        "GvjLtPW2s33R/"
        "B8s7htW8Tjzbhjzc0jFnwxbFo9ToseO82rlpfBkxTzGPmbWrfH28oGn1ej2m2DqDyXbb0ruXwzBG0b/"
        "ANNxl02z5tNxktqNNqrUz7lodzzZscUvgyY9Xgz14nLxWlL4s3mQGxmlwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABNPDnMuW4syVe2r0Y5vS81NGnses14yVKF"
        "eSrThb1chYU6/fb0sjLbwhRrSVIQtcraSS2V93ezsruwhYWW47do910efQa/"
        "BTUaXUV7cmO3MTExMTTJjvWYvjy47RF8WXHauTHetb0tFoiUns287l0/"
        "uWl3baNVk0ev0eTvw5sfExMTE1yYsuO0TjzYM1Jtiz4Mtb4s2K1seSlqWmJtLy9wvivyaHK/"
        "ElaXN8fZOSa+"
        "vsfaeKrc6zGaPfcR9jHvrwx1tVjNTvLetLC8wk8s1O7lmtac9ehVpdPpOsNzxVXO7Xd5G3wnFNKzu5thnzksYYzK17"
        "ajUl9tjoVqlKlRq4/"
        "wxhkMn3zW8KMs2Pq07mrGWFpWzlHIaRlN5z19x5jbjF6tWue+"
        "yt639To1a0IQhd3ePsZqUlXF4y7uPHWscbWnqVLajNDup2FOenicfivTe463Fum5dMajPfeMW0Ysd8O91jm1MeSYim"
        "2bvfiuO264azzGTDN51GCvndRTDmrki2f9bbLtmfYdl650Wlx9N5+os+bFqelskzWuXLhrNsu+9O4vnZadP6m/"
        "zfM6mMUaPU2jBpMmp018VqR+Azdq0AAAAaQM32kDQflw/uv/AI1/2l64/wAlj+/f+7H/AJCANBvXAAAAAAAAAACt/"
        "UH/ACR/n/8AoVW9ZDqD/kj/AD//AEKre9deS/8AzG2P/E/+cbg+dfl2/wBKvVX+B/"
        "8ATmzgDPmowABajpd5Yx+jbFf6tstxSt9Y26NCT3q6jL7pjczShNRt611Gf+"
        "p0rHIUKk1le1p4RkpzyWNWtNTtaVxUlquIre9n0m/"
        "bXq9q1sW8xq8cV76cRkw5KWrkw58UzExGTDlpTJXmJrM17bRNbWiZ/pfqPcOkt+27qDbJp8K2/NN/"
        "NZeZw6nBkpbDqdLnisxM4dTgyZMN+Ji1Yv30muStbRabmLpq23Vc1fZTSsPe7LqF9WqXdlTxFGe/"
        "yWHkrTRqfl91YUIVLyvQtu+"
        "aW2vralcUp7WSnNdT0a8YyzRlqHCHJu45ShjrPU8zjaFSrJJc5bOY68xOLsaUZu6pWrXN7Qo+"
        "2jSl75o21pLcXdTuhLToTRjB9XReoXlDQLSjjMZmqWUxFvLLJbYnYLeOTtbaSXuhLSta8Ktvkba3llh4ZLWhfU7anC"
        "MYyUZZo972Wb6uuXMvaVLW1n1zXo1JIyTXeExNf3uEs0PDN7Opl8hl6dKaMO/"
        "w1KVKSrTjHxU55J4SzQxLHXyj6PBG3Up09uNsdIw4N81Go1WC844jtpm1uhrjvN9TFfG/"
        "mbzjtaOZ85M2m2w81vItuWrtvOXJ1jstMuSdTquldHpNv1WGuW09+"
        "TS7Zul82OMWitbmuKdRjrmpSZisYoilafH59400vinLa5itX2DIX+emx1G4ztjcewqQsa1Gnby2+"
        "ShWpRkqWVTK1oV7qnjKktxNQpy+"
        "2lr0repa06lkuTsFP1I8R6puumexvNn16FxPe4aSrTkrxuLm2tZNhw0kJ5pISXlG6tLS8x8teMnvdlCWajDxXtCE2d"
        "97fXuSvLnIZG7ub6+vK09xd3l3WqXF1c16k0ZqlavXqzT1KtSeaMYzTzzTTRj/ABi9bo/"
        "I248c5GfI6lma+"
        "NqV4SS3lrNLTucff05IzeCS9sLiWpbVoyQmnhSreCW5t4VKkbetSmnmjGrrul93yaHYNTg3euq6n6ey5s+"
        "DcNwx8afXfC6zXW6PUVwVjJj02bH24ceSsZM1MeOImZve2StvtPXnTmHderdDq+nb7f0L1lg0uk1ez7Rmi2s2r+"
        "jr1ybZuWjvqreZza3T5ovqc2K84tPlzZrTWsY8dMN/3YbiTkrOZelhLLSdkp309aFGrG/xF/"
        "jrWz75vDNVv7y9oUbazo0/1jNUr1Je/"
        "u8MkJ55pZJrb873+L4p4Q1jhWzvaV5m8hb2M2ThRjGHgsrXJRzeTyM0ke+"
        "pQpZLYIQp2NKrCWNa3heQ8U3u1SE0WX3WFy1d2U1pQoaljK80ng/MrHD3k97LHu7vaSyZHK5DHwn/AL/71jNJ3/"
        "wkhD9FacxmcrsGSu8xm8hd5XKX1WNa7vr2tPXuK0/"
        "dCWXxTzxj3SU5JZadKlJ4aVGlJJSpSSU5JZYdK7R1Hv8Aue1avqTFtu3bdsmrruGn2/"
        "QajLrMus3LFWa6bUZ898eOmLBp5tN8WOkTktab1ycxaLVqX6i6K6R2Pf8Ab+is+9bzvPU+332fWbxu+"
        "jwbdp9s2TUXrfWaPSabFmzZdRqtbFK4s+XJNMNK1x5MMxalqZLu9FP/AOacsf8AN9Q/"
        "+729Q9JnHfLW38XyZynqtTH05dhksJMj7/Yy3kYy46F/LbexjNUp+yjCGSufH/ZePvk/"
        "h4P1jNMbVs+r0XUnVm65pxfBd6nY50cUvNssf0dt9tLqPPUmkRTnLaPN8Wv3V8Z7Z8GOdQdR7fufRPk+"
        "6f00aiNf0xHVcblOXFWunt/TO8Ytdo/g2SMlrZeMFJjN3Y8fZfite+PnL22n9pDkf+dy/"
        "wCc6yUSSXJyzt1PjitxZLUx/wCylapCpPTjZSxv/FDM087+l77TvhD3+lJHu9n/"
        "AOS76f8ACPejRz0ztGr2meop1c4p/pTqfdN303mrzfjSaymlrhjLzWvZl5w376R3RXw4tPPg656j2/"
        "qKvRtdvjUROw9DbD07rvhGKuPncNuvrbaicHbkyec0/Gox+byW7LWnu5x14jm93V5/uN4f/wCa5L/"
        "urX3uOmfD2GxcAbNr+Try21jnMzs2HuLiaaSX2EMni8XZSVZI1JpZPbU6leSahCM0PFWhTlh+"
        "sYKS75yzt3I2PwGM2Spj57XW5K1PGQsrKW0qQlr0LW3n9vPCpP7WPs7Oj3R7pe6Pij/"
        "wv0tNxZGMvSTyTNLGMs0txtMZZoRjCMIwx+HjCMIw/WEYR/"
        "WEYfrCP6wa83rY9dtHQmy7Nqc2PDrfjRpYjPprTmphnW7rq82DLSb1x99sdc1LTWYiO+"
        "s15mPFuPpjqrauofKx1R1LodNm1O2T0HrrTpNbWNPk1Nds2DbdNqtPljHfNGPHmyaXLji9bWnzdov28z2xV3auGuSd"
        "RzFxiL7Us5eezrz0rXI4nF32TxmRp+OMKVeyu7ShVpzwrS+"
        "GeFvUjTu6PjhTuKFKrCMkLi8C6hX4N0HcuTOQqE2Hur6yoRtcRdxhRv6NhZe2ntbSrTm/"
        "eo5PO5G4o29GxqS+2oextY1oUqlWtSpV/"
        "wBY6ruWdaxtHGVa+E2Wnb05aNvc7JYXd1f06UkIQklnvMfksZWu5pYQ7o1r6N1cT/"
        "8ADrTR7owjXkLlzeuTq1Gbact7Wytakatnh7GlCyxFrVjLGWNaS0kmmmr3EJZp5Zbm9q3VzJJPPSp1paU0ZGQ7jtPW"
        "fUenpse8V2XRbVfNgtum4bfn1OXU7jp9Plpm8zpdNlx1+CWz3x0nJOTJaMfj2WvWJx5MN2bqDyadF6vJ1V03fqXc+"
        "oMen1Vdi2jd9JosOi2bV6zBk086nX63Dmv/"
        "AEjXSYs2SuCuHFScszE5KYrzGbD6ThXkenqHLmP23OVYU7DM3eRs9guId8JaFDPTzTVLyb+"
        "yjCjZ38ba9rwhCeeNvQqyyQjPGWKW+pXhXZZtuveQNSxV3sWvbNJb395DC0KmRr43IRtqVOvVnt7WFatVx9/"
        "CnLf0r+lLUoSVa9elWjRkltpq9NU36B1C8mcd2NLE4rJ2uUwtvDw2uI2C2qZC0s5e/"
        "v8ABaVqNxZ5C2ofx8NrSvpbWSMZppKEs8000ZTedj3XDvGl6j6bto512LQf0Xrdu11smLS6/"
        "QVyeewxjzYq3tp9Tgyf2L2rNbUitbWrWlqZYLprqrYNT05rui+"
        "ta7lG1ajdv6e2vetqph1Gv2nd7Yfg2onNps98ddXodXh/9XHXJGSmSb3pS+TJTLp/"
        "VcFcF7hs+54TL5zA5PCatgsjaZW/vMvZXGPjkJrCtLc0Mbj6N1JRrXcbuvSko3NajL7C1t/"
        "bzT1oXHsKFb+"
        "vVPv9huvIsLHD3FO7xOpWP5LJd0Z4VKF1k5ripcZWtb1JYxlno06k1CwhPL3yVKljUq0556VSnNH5m6dTnKu6Y+"
        "tiqt/"
        "jtdx91Tmo3lDWLS4sKt3RnhGWejVvru9yGQp0qksYy1adtd28laSaanVhPSmmkjXt12rZt51m+U6k6j+BYM+"
        "k0mXRbVtegyZM+LR01ExOp1Oo1OStPO6rNWJxcY6earimP/uiIr23/"
        "qbpnbulcnRXRf8ASer0m47jg3PqDfd2w4dJqNxyaSvGh0Ok0WHJljBodPefPzbNec9tRE8fMmZtfDkP+1A0D/nGv/"
        "8A3mWUPSbleWtvzOhYvje9qY6OtYie1ns5KVjCnewms4140faXcKkY1IQjcVPFCMkPF3w/h3IyX/"
        "Smz6vZtLumHWThm+s37dNyxeZva8fB9ZlpfDF5tSnbkiKz31iJis/RaUR191Ft/"
        "Uuv2PU7dGojHt3SewbLqPhGKuK3wzbdNbFqZxxXJki2GbzHm7zNZtHjNK/Qvhyj/aj8a/8AONX/APcMw/"
        "npgzOH3LjveeGMtdy2t5kKOVucbGaMPa1cdmbGnaXVS0pxjCFWviL6lLfT0++"
        "EZpbuSaEs1OjWmlq1muWtvz2iYfjrIVcfNreDnsp7GSjYy0r2E1hSuKNv7W6hUjGpCElzV8f7kPFHwxj3d36+"
        "BxuSyGHv7XKYq9usdkbGtLcWd9ZV6lvdW1aT+"
        "xqUa1KaWeSaEIxhGMs0PFLGaWbvlmjCOP4ui9Vn6d3XadVqMWl1ufqHW75tmrwWnNXTZp1NdRosl4tWkzPEWx5qRE9"
        "tb2mlptFZZhn8pug0nWewdQ6DR59dtml6P2vpbfNv1Va6a2u01NBbR7nhxWrfLxXma5tNkma9+"
        "TFSMla0m0PebVxDyLqGXuMRk9UzVaalWnp29/"
        "jcbe5DGZCnCbup17G9taFSjVlqyxln9lGaS5o+OFO5oUa0JqcPSXnAW+"
        "4vju95Fzdva4Gysa0njwubqVMdm6tlUno0aV5Tta1OEtOerdVoUKNhdT219W8MalKhPLUoQre4xPV/"
        "y1jbOS1uqerZypJJCT37LYi7kvJvDDuhNP8AlGUxVrNNGH6xm91hGaP6zd8Yx74o5C5h33k6alJtGX8ePtqvtrXDY+"
        "jJY4mhW8M0sK3u1OM09zXllmnlp3F9Wuq9KSpUp0qkkk80sZLS36+"
        "z59Hg1uDYNBgwZ8dtfr9Pn1GstrsGOY85i0mjvjxTp7aj6JvmyxOKJm1I5rFLQuvxeSTS6TctVtur6u3bV6rTZ6bRt"
        "Os0uj27Hteqyx/U59x3HFnzxrKaTmZjFpsHGotWKZJrW05Kz30V/wC7za/+iMf++cYq5u3+7Pbv+k+f/"
        "wC9bt9njvkzZ+MMnfZfVqljTvMhYRx1xG/tIXlONtG4o3PdJJGen4Z/a0Kf7/fH93xQ7v174eLyN/"
        "cZTIX+Tu4yRusjeXV/cxpy+CSNxeV57itGSTvj4JI1Kk3hl74+GHdDvj3L7Q7Pq9P1Xvu85Jw/"
        "A9x0O16fTxW8zmjJo6ZK5vOUmkVrWZtHZMXtNvHmI/"
        "jFbr1Ht+s6B6U6awxqP6R2Xdd+1msm+KtdPOHcsmC2n8zljJNr3iMdvOVnHSKzxxNuV4eGP7VTl/"
        "8A53uX+SWvqJJL17lnbtY0jPcf4upj5de2SpkKmSkr2Uta8mmydja4669hdRqSxpQjbWdGEkPBN4J/"
        "FP8Ar4u6EaOdg2jV7buHU2p1M4px7tu/w7S+bvNrRh+DYsX9bE0rFL99J+bE2jjie46v6j2/"
        "fNo6H0OijURm6e6cjatfOfFXHSdV8MzZ+dPauS85MXZkr8+1cc93MdvEcze/nH+1a4Z/+n0b/IfOvhdFV/"
        "b0tw3DHT1JZbm9121ureSaMIRqSWGRkkr+CEf7KMnv1KaMsO+Ph8U3d3SzRhXnZOWdu2rStf0HLVMfNr+"
        "szY2fFyW9jLRu5Y4rG3OKtPb3MKk0a0IWl3WhU75JfHUjLPHujL3R8drmyZvUszZbBruRr4vL4+"
        "pGpa3lDwRml8Us1OpTqU6ks9GvQrU5pqVe3r06lGtSmmp1ac0k0YIHF0hrsnR+99O6jNp8Wq3LXbjqtPlpe+"
        "TDXz2trrNLGW3m63iJtjpXN20tNIm0175iInK9R5Rdrw+"
        "UfpbrHR6fV6jQ7LteyaDWYMmPHh1N50u1223cJwVnLbHaa1y5L6ab5Mdctq0i84otNq+32/"
        "izkDEbTseOqajsVxCyvspc++WuHv7mxr46jVr3EMlRvKNCe2ns57WX2/tYVe6SHip1PBVknpy+q0ngPOb/"
        "AMc5jeNcy1jkcpjr2e0o6lbSTRyFaFtGSe7kuLmtPQo0Lya1q07vH2tOS4lvaMYSQuKdzNC3h62v1hcr18dPY+"
        "6afSrT0Y0pslJhr2e9j4pYyxqRo3GXr4uM8e+MYyzY2ahH+"
        "EaPh74Rg3SORtw47ylXK6nmKuMrXMJZb22hTpVsdf05Zpppad5j6sk1rVhT8c8KNSFOSvbQqVPdatGM80Y3+"
        "KOutRteTHemy7VuekyaG2kvTNk1ul3KmDu+GYNVWcHnNHg1MRjil8E31FOb1iaeF0Tnt5KtHv2HLjydS7/"
        "sW4Yt0puGPLp8O2a/Zcmq83/"
        "Ruq2+1NVOPctTorWzTlxaquLSZe3Haa5ebY3ruP8AF8163tNnb6Zh9zxWZ9+"
        "oy1rKOOytpj6s0tSEs0mdt7ilSsZrCWEIwuZsjCFGlTljPGeSaSWeWwfW1UxUcnx/Sk9h+fSY/"
        "Oz33s4yxry4uevjYY2FePdCeNGN5Jlo2nfCEITQvIwhCM0XhLnrF5Yr2kbalZ6dZ1oyeGF/"
        "bYfIT3cs3d3e0lkvM1dWPj7/ANe6aymp9/8A8X3forXsGw5vastd53YcldZbLX08J7m9u54T1J/"
        "DLCSnTkllhLTo0KUkstOhb0JKdChSllp0aclOWWWFlo9l37cupts6h3nRbVtH9E6fW4q49u1F9VqtyyazD5iPhefzO"
        "GsaXT1m2TBS03vTJa0cTF5tST3HqfpPZeh986O6b3PqDqL4w6zbNRbNvGjxbfoNkw7bqo1czoNLGp1N7a7WXrXDqst"
        "IxYcmGtJ5i2KK5PjANhtOAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAACxXDPC1Hbba533frv9nOL8H7Sve5G5qxs6meqW1SFOpZY+rHuqSWMlx3Wt7kKMs1WrdR/"
        "KMVCrkprirjYExVxY2mUxt3k8f+"
        "b422v7O4yOJ97rWH5nY0binUu8f79byzV7L323lqW3vdCWatbe19tSljPJLCMy8xc1X/"
        "JNa1wmGtP2b49wctChr+"
        "sW9OhbSeztaMKFC7yNK0jG2jXp0oRpWVlbxjY4q18Nvawq1o3N7dY7vsb3qrafbNo50WLWVyW1++"
        "TbHNtBpqTStsOhw905L7jqYvMYM2SlcGnpW+"
        "WLXy1rWmZdK26X0FNbvvUMxuefbrYa7R0tFM1abvrctct6andNVFK4sezaKcUW1WnxZbarWZL4tPNcWC97Zf38zc21"
        "97hQ1LVLb9neNsJ7G2xOGtqcLSOSp2XhktbrI0aUYSyUKXglnsMbDvpWvdJWre2vJZKlGvgJLa9r0WzaLFoNvwxhwY"
        "+bTMzNsubLfxyajUZZ+fm1Ga3zsuW8za0+HhWKxELv2/"
        "bp1Luefdt31M6nV5u2lYiIx4NNp8fMYNJo8FeMem0mnpPZg0+KK0pXmeJva1rAEghwAAABpAzfaQNB+XD+6/8AjX/"
        "aXrj/ACWP79/7sf8AkIA0G9cAAAAAAAAAAK39Qf8AJH+f/wChVb1kOoP+SP8AP/8AQqt7115L/wDMbY/8T/"
        "5xuD51+Xb/AEq9Vf4H/wBObOAM+ajAAAAAAAAAAAAAAFmtL5i1bXuCtw40vrXNz5/"
        "PVc3PZXFtaWdTFyQyNrYUKHvFxUyFK5kjLPa1I1fZ2dXwyxljL44xjCFZRGbrtGj3nDp8GtjJOPTa3S6/H5q/"
        "m5+EaS/"
        "nMPdPE807v7VfDujw5hO7B1FuPTWp1er2y2GubW7ZrtozznxRmrOj3HFGLUxSs2r25JpHzL8z2z48SAJNBAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAADSBm+0gaD8uH91/8AGv8AtL1x/ksf37/3Y/8AIQBoN64AAAAAAAAAAVv6g/5I/wA//"
        "wBCq3rIdQf8kf5//oVW9668l/8AmNsf+J/843B86/Lt/pV6q/wP/"
        "pzZwBnzUYAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA0gZvtIGg/Lh/df/Gv+0vXH+Sx/"
        "fv8A3Y/8hAGg3rgAAAAAAAAABW/qD/kj/P8A/Qqt6yHUH/JH+f8A+hVb3rryX/5jbH/if/ONwfOvy7f6Veqv8D/"
        "6c2cAZ81GAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAANIGb7SBoPy4f3X/xr/tL1x/ksf37/"
        "wB2P/IQBoN64AAAAAAAAAAVv6g/5I/z/wD0Kresv1A0oxttWrd37tOvl6UY/"
        "wDFGtTx08sP+uFCaP8A1K0PXHkutE9D7LEfTS25Vn8Jndtdb9LRP+187fLxS1fKp1PMxxGSuyXr+"
        "NY6e2qnMf8A7qWj/"
        "XEgDYDUIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA0gZzW1KNa5t6MsO+"
        "arXpUpYf8calSWSEP8ArjHuaMtBeXC0d3TNf4xXebTH4Wna4j/5rP8Aweuv8lmlop1zk4nttfpukT/CbUjfrWj/"
        "AFxGSsz/AK4AGhHrYAAAAAAAAABH/JOp1Nu1uraWnh/"
        "MrKtLf4+E0fDLVrUpJ5KltGaMYSy+"
        "8Uak8kk037ktaFKaeMJYRjCk1zbXFncVrW7oVba5oTzUq1CtJNTq0qkse6aSeSeEJpZof8UYf8v8ItGXwcvq2u56MJ"
        "8vh7C/qyw8MtetQlhcSy/p+7C5p+CvCX9Ifuwqd0O7+DaPQvlGv0rp8m2a/"
        "SZNbtlsts+"
        "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"
        "OMPwnURSMeOmLD35JxabT0ia4cdsl7Ta+"
        "XLa3dlmtQDE2wgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAH/9k=");
    hiType["width"] = HippyValue(50);
    hiType["height"] = HippyValue(50);
    hiType["screenScale"] = HippyValue(1.0);
    auto resultMap = HippyValue(hiType);
    CallGetComponentSnapshotMethod(ts_env_, ts_render_provider_ref_, "getComponentSnapshot",
                                   root->GetId(), resultMap);
    params.emplace_back(resultMap);        
  }

  if (name == "getLocationOnScreen") {
    HippyValue resultMap = CallGetLocationOnScreenMethod(ts_env_, ts_render_provider_ref_,
                                                         "getLocationOnScreen", root->GetId());
    params.emplace_back(resultMap);    
  }
  c_render_provider_->CallUIFunction(root->GetId(), node->GetId(), cb_id, name, params);
}

void NativeRenderManager::ReceivedEvent(std::weak_ptr<RootNode> root_node, uint32_t dom_id,
                                        const std::string& event_name, const std::shared_ptr<HippyValue>& params,
                                        bool capture, bool bubble) {
  auto manager = dom_manager_.lock();
  FOOTSTONE_DCHECK(manager != nullptr);
  if (manager == nullptr) return;

  auto root = root_node.lock();
  FOOTSTONE_DCHECK(root != nullptr);
  if (root == nullptr) return;

  std::vector<std::function<void()>> ops = {[weak_dom_manager = dom_manager_, weak_root_node = root_node, dom_id,
                                             params = std::move(params), use_capture = capture, use_bubble = bubble,
                                             event_name = std::move(event_name)] {
    auto manager = weak_dom_manager.lock();
    if (manager == nullptr) return;

    auto root = weak_root_node.lock();
    if (root == nullptr) return;

    auto node = manager->GetNode(root, dom_id);
    if (node == nullptr) return;

    auto event = std::make_shared<DomEvent>(event_name, node, use_capture, use_bubble, params);
    node->HandleEvent(event);
  }};
  manager->PostTask(Scene(std::move(ops)));
}

float NativeRenderManager::DpToPx(float dp) const { return dp * density_; }

float NativeRenderManager::PxToDp(float px) const { return px / density_; }

void NativeRenderManager::CallNativeMethod(const std::string& method, uint32_t root_id, const std::pair<uint8_t*, size_t>& buffer) {
  hippy::CallRenderDelegateMethod(ts_env_, ts_render_provider_ref_, method, root_id, buffer);
}

void NativeRenderManager::CallNativeMethod(const std::string& method, uint32_t root_id) {
  hippy::CallRenderDelegateMethod(ts_env_, ts_render_provider_ref_, method, root_id);
}

void NativeRenderManager::CallNativeMeasureMethod(const uint32_t root_id, const int32_t id, const float width, const int32_t width_mode,
                                                 const float height, const int32_t height_mode, int64_t& result) {
  CallRenderDelegateMeasureMethod(ts_env_, ts_render_provider_ref_, "measure", root_id, static_cast<uint32_t>(id), width, width_mode, height, height_mode, result);
}

void NativeRenderManager::CallNativeCustomMeasureMethod(const uint32_t root_id, const int32_t id, const float width,
                                                        const int32_t width_mode, const float height,
                                                        const int32_t height_mode, int64_t &result) {
  CallRenderDelegateMeasureMethod(ts_env_, ts_render_provider_ref_, "customMeasure", root_id, static_cast<uint32_t>(id),
                                  width, width_mode, height, height_mode, result);
}

LayoutSize NativeRenderManager::CallNativeCustomMeasureMethod_C(uint32_t root_id, uint32_t node_id,
    float width, LayoutMeasureMode width_measure_mode,
    float height, LayoutMeasureMode height_measure_mode) {
  return c_render_provider_->CustomMeasure(root_id, node_id, width, width_measure_mode, height, height_measure_mode);
}

std::string HippyValueToString(const HippyValue &value) {
  std::string sv;
  if (value.IsString()) {
    value.ToString(sv);
  } else if(value.IsDouble()) {
    double d;
    value.ToDouble(d);
    sv = std::to_string(d);
  } else if(value.IsInt32()) {
    int32_t i;
    value.ToInt32(i);
    sv = std::to_string(i);
  } else if(value.IsUInt32()) {
    uint32_t ui;
    value.ToUint32(ui);
    sv = std::to_string(ui);
  } else {
    FOOTSTONE_LOG(ERROR) << "Measure Text : unknow value type";
  }
  return sv;
}

void CollectAllProps(std::map<std::string, std::string> &propMap, std::shared_ptr<DomNode> node) {
  propMap.clear();
  // 样式属性
  auto style = node->GetStyleMap();
  auto iter = style->begin();
  while (iter != style->end()) {
    propMap[iter->first] = HippyValueToString(*(iter->second));
    iter++;
  }
  // 用户自定义属性
  auto dom_ext = *node->GetExtStyle();
  iter = dom_ext.begin();
  while (iter != dom_ext.end()) {
    propMap[iter->first] = HippyValueToString(*(iter->second));
    iter++;
  }
}

void NativeRenderManager::DoMeasureText(const std::weak_ptr<RootNode> root_node, const std::weak_ptr<hippy::dom::DomNode> dom_node,
                   const float width, const int32_t width_mode,
                   const float height, const int32_t height_mode, int64_t &result) {
  auto dom_manager = dom_manager_.lock();
  FOOTSTONE_DCHECK(dom_manager != nullptr);
  if (dom_manager == nullptr) {
    return;
  }

  auto root = root_node.lock();
  FOOTSTONE_DCHECK(root != nullptr);
  if (root == nullptr) {
    return;
  }
  
  auto node = dom_node.lock();
  if (node == nullptr) {
    return;
  }
    
  std::vector<std::shared_ptr<DomNode>> imageSpanNode;
  std::map<std::string, std::string> textPropMap;
  std::map<std::string, std::string> spanPropMap;
  CollectAllProps(textPropMap, node);

  float density = GetDensity();
  OhMeasureText measureInst;
  OhMeasureResult measureResult;

  measureInst.StartMeasure(textPropMap);

  if (node->GetChildCount() == 0) {
    measureInst.AddText(textPropMap);
  } else {
    for(uint32_t i = 0; i < node->GetChildCount(); i++) {
      auto child = node->GetChildAt(i);
      CollectAllProps(spanPropMap, child);
      if (child->GetViewName() == "Text") {
        measureInst.AddText(spanPropMap);
      } else if (child->GetViewName() == "Image") {
        if (spanPropMap.find("width") != spanPropMap.end() && spanPropMap.find("height") != spanPropMap.end()) {
          measureInst.AddImage(spanPropMap);
          imageSpanNode.push_back(child);
        } else {
          FOOTSTONE_LOG(ERROR) << "Measure Text : ImageSpan without size";
        }
      }
    }
  }
  measureResult = measureInst.EndMeasure(static_cast<int>(width), static_cast<int>(width_mode),
                                         static_cast<int>(height), static_cast<int>(height_mode), density);

  if(measureResult.spanPos.size() > 0 && measureResult.spanPos.size() == imageSpanNode.size()) {
    for(uint32_t i = 0; i < imageSpanNode.size(); i++) {
      double x = measureResult.spanPos[i].x;
      double y = measureResult.spanPos[i].y;
      // 把 c 测量到的imageSpan的位置，通知给ArkTS组件
      if (enable_ark_c_api_) {
        c_render_provider_->SpanPosition(root->GetId(), imageSpanNode[i]->GetId(), float(x), float(y));
      } else {
        CallRenderDelegateSpanPositionMethod(ts_env_, ts_render_provider_ref_, "spanPosition", root->GetId(), imageSpanNode[i]->GetId(), float(x), float(y));
      }
    }
  }
  result = static_cast<int64_t>(ceil(measureResult.width)) << 32 | static_cast<int64_t>(ceil(measureResult.height));
}

void NativeRenderManager::HandleListenerOps(std::weak_ptr<RootNode> root_node,
                                            std::map<uint32_t, std::vector<ListenerOp>>& ops,
                                            const std::string& method_name) {
  if (enable_ark_c_api_) {
    HandleListenerOps_C(root_node, ops, method_name);
  } else {
    HandleListenerOps_TS(root_node, ops, method_name);
  }
}

void NativeRenderManager::HandleListenerOps_TS(std::weak_ptr<RootNode> root_node, std::map<uint32_t, std::vector<ListenerOp>> &ops,
                          const std::string &method_name) {
  auto root = root_node.lock();
  if (!root) {
    return;
  }

  if (ops.empty()) {
    return;
  }

  footstone::value::HippyValue::HippyValueArrayType event_listener_ops;
  for (auto iter = ops.begin(); iter != ops.end(); ++iter) {
    footstone::value::HippyValue::HippyValueObjectType op;
    footstone::value::HippyValue::HippyValueObjectType events;

    const std::vector<ListenerOp> &listener_ops = iter->second;
    const auto len = listener_ops.size();
    std::vector<ListenerOp>::size_type index = 0;
    for (; index < len; index++) {
      const ListenerOp &listener_op = listener_ops[index];
      std::shared_ptr<DomNode> dom_node = listener_op.dom_node.lock();
      if (dom_node == nullptr) {
        break;
      }
      events[listener_op.name] = footstone::value::HippyValue(listener_op.add);
    }
    if (index == len) {
      op[kId] = footstone::value::HippyValue(iter->first);
      op[kProps] = events;
      event_listener_ops.emplace_back(op);
    }
  }

  ops.clear();
  if (event_listener_ops.empty()) {
    return;
  }

  serializer_->Release();
  serializer_->WriteHeader();
  serializer_->WriteValue(HippyValue(event_listener_ops));
  std::pair<uint8_t *, size_t> buffer_pair = serializer_->Release();
  CallNativeMethod(method_name, root->GetId(), buffer_pair);
}

void NativeRenderManager::HandleListenerOps_C(std::weak_ptr<RootNode> root_node, std::map<uint32_t, std::vector<ListenerOp>> &ops,
                         const std::string &method_name) {
  auto root = root_node.lock();
  if (!root) {
    return;
  }

  if (ops.empty()) {
    return;
  }

  uint32_t root_id = root->GetId();
  std::vector<std::shared_ptr<HRUpdateEventListenerMutation>> mutations;
  for (auto iter = ops.begin(); iter != ops.end(); ++iter) {
    auto m = std::make_shared<HRUpdateEventListenerMutation>();
    footstone::value::HippyValue::HippyValueObjectType events;

    const std::vector<ListenerOp> &listener_ops = iter->second;
    const auto len = listener_ops.size();
    std::vector<ListenerOp>::size_type index = 0;
    for (; index < len; index++) {
      const ListenerOp &listener_op = listener_ops[index];
      std::shared_ptr<DomNode> dom_node = listener_op.dom_node.lock();
      if (dom_node == nullptr) {
        break;
      }
      events[listener_op.name] = footstone::value::HippyValue(listener_op.add);
    }
    if (index == len) {
      m->tag_ = iter->first;
      m->props_ = events;
      mutations.push_back(m);
    }
  }
  ops.clear();
  if (mutations.empty()) {
    return;
  }
  c_render_provider_->UpdateEventListener(root_id, mutations);
}

void NativeRenderManager::MarkTextDirty(std::weak_ptr<RootNode> weak_root_node, uint32_t node_id) {
  auto root_node = weak_root_node.lock();
  FOOTSTONE_DCHECK(root_node);
  if (root_node) {
    auto node = root_node->GetNode(node_id);
    FOOTSTONE_DCHECK(node);
    if (node) {
      auto diff_style = node->GetDiffStyle();
      if (diff_style) {
        MARK_DIRTY_PROPERTY(diff_style, kFontStyle, node->GetLayoutNode());
        MARK_DIRTY_PROPERTY(diff_style, kLetterSpacing, node->GetLayoutNode());
        MARK_DIRTY_PROPERTY(diff_style, kColor, node->GetLayoutNode());
        MARK_DIRTY_PROPERTY(diff_style, kFontSize, node->GetLayoutNode());
        MARK_DIRTY_PROPERTY(diff_style, kFontFamily, node->GetLayoutNode());
        MARK_DIRTY_PROPERTY(diff_style, kFontWeight, node->GetLayoutNode());
        MARK_DIRTY_PROPERTY(diff_style, kTextDecorationLine, node->GetLayoutNode());
        MARK_DIRTY_PROPERTY(diff_style, kTextShadowOffset, node->GetLayoutNode());
        MARK_DIRTY_PROPERTY(diff_style, kTextShadowRadius, node->GetLayoutNode());
        MARK_DIRTY_PROPERTY(diff_style, kTextShadowColor, node->GetLayoutNode());
        MARK_DIRTY_PROPERTY(diff_style, kLineHeight, node->GetLayoutNode());
        MARK_DIRTY_PROPERTY(diff_style, kTextAlign, node->GetLayoutNode());
        MARK_DIRTY_PROPERTY(diff_style, kText, node->GetLayoutNode());
        MARK_DIRTY_PROPERTY(diff_style, kEnableScale, node->GetLayoutNode());
        MARK_DIRTY_PROPERTY(diff_style, kNumberOfLines, node->GetLayoutNode());
      }
    }
  }
}

bool NativeRenderManager::IsCustomMeasureNode(const std::string &name) {
  if (custom_measure_views_.find(name) != custom_measure_views_.end()) {
    return true;
  }
  return false;
}

bool NativeRenderManager::IsCustomMeasureCNode(const std::string &name) {
  auto custom_measure_c_views = HippyViewProvider::GetCustomMeasureViews();
  if (custom_measure_c_views.find(name) != custom_measure_c_views.end()) {
    return true;
  }
  return false;
}

void NativeRenderManager::RegisterNativeXComponentHandle(OH_NativeXComponent *nativeXComponent, uint32_t root_id, uint32_t node_id) {
  if (enable_ark_c_api_) {
    c_render_provider_->RegisterNativeXComponentHandle(nativeXComponent, root_id, node_id);
  }
}

void NativeRenderManager::DestroyRoot(uint32_t root_id) {
  if (enable_ark_c_api_) {
    c_render_provider_->DestroyRoot(root_id);
  }
}

bool NativeRenderManager::GetViewParent(uint32_t root_id, uint32_t node_id, uint32_t &parent_id, std::string &parent_view_type) {
  if (enable_ark_c_api_) {
    return c_render_provider_->GetViewParent(root_id, node_id, parent_id, parent_view_type);
  }
  return false;
}

bool NativeRenderManager::GetViewChildren(uint32_t root_id, uint32_t node_id, std::vector<uint32_t> &children_ids, std::vector<std::string> &children_view_types) {
  if (enable_ark_c_api_) {
    return c_render_provider_->GetViewChildren(root_id, node_id, children_ids, children_view_types);
  }
  return false;
}

void NativeRenderManager::CallViewMethod(uint32_t root_id, uint32_t node_id, const std::string &method, const std::vector<HippyValue> params, std::function<void(const HippyValue &result)> callback) {
  if (enable_ark_c_api_) {
    c_render_provider_->CallViewMethod(root_id, node_id, method, params, callback);
  }
}

}  // namespace native
}  // namespace render
}  // namespace hippy
