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

#pragma once

#include <map>
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <sys/types.h>
#include "renderer/components/root_view.h"
#include "renderer/uimanager/hr_mutation.h"

namespace hippy {
inline namespace render {
inline namespace native {

using HippyValueObjectType = footstone::value::HippyValue::HippyValueObjectType;

class HRViewManager {
public:
  HRViewManager(uint32_t instance_id, uint32_t root_id);
  ~HRViewManager() = default;
  
  void AttachToNativeXComponent(OH_NativeXComponent* nativeXComponent);

  int GetRootTag() {
    return (int)root_id_;
  }

  void AddMutations(std::shared_ptr<HRMutation> &m);

  void ApplyMutations();
  void ApplyMutation(std::shared_ptr<HRMutation> &m);
  
  std::shared_ptr<BaseView> CreateRenderView(uint32_t tag, std::string view_name);
  void RemoveRenderView(uint32_t tag);
  void RemoveFromRegistry(std::shared_ptr<BaseView> &renderView);
  void InsertSubRenderView(uint32_t parentTag, std::shared_ptr<BaseView> &childView, int32_t index);
  void MoveRenderView(std::vector<HRMoveNodeInfo> nodeInfos, uint32_t parentTag);
  void Move2RenderView(std::vector<uint32_t> tags, uint32_t newParentTag, uint32_t oldParentTag, int index);
  void UpdateProps(std::shared_ptr<BaseView> &view, HippyValueObjectType &props);
  void UpdateProps(uint32_t tag, HippyValueObjectType &props);
  void UpdateEventListener(uint32_t tag, HippyValueObjectType &props);
  bool CheckRegisteredEvent(uint32_t tag, std::string &eventName);
  void SetRenderViewFrame(uint32_t tag, const HRRect &frame);

  void NotifyEndBatchCallbacks();

private:
  void MaybeAttachRootNode(OH_NativeXComponent *nativeXComponent, std::shared_ptr<RootView> &rootView);
  void MaybeDetachRootNode(OH_NativeXComponent *nativeXComponent, std::shared_ptr<RootView> &rootView);
  
  std::shared_ptr<NativeRenderContext> ctx_;
  uint32_t root_id_;
  OH_NativeXComponent *nativeXComponent_;
  std::shared_ptr<RootView> root_view_;
  std::map<uint32_t, std::shared_ptr<BaseView>> view_registry_;
  std::vector<std::shared_ptr<HRMutation>> mutations_;
  std::vector<std::function<void()>> end_batch_callbacks_;
};

} // namespace native
} // namespace render
} // namespace hippy
