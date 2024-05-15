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

#include "renderer/components/base_view.h"
#include "renderer/arkui/stack_node.h"
#include "renderer/arkui/scroll_node.h"

namespace hippy {
inline namespace render {
inline namespace native {

class ScrollView : public BaseView {
public:
  ScrollView(std::shared_ptr<NativeRenderContext> &ctx);
  ~ScrollView();

  ScrollNode &GetLocalRootArkUINode() override;
  bool SetProp(const std::string &propKey, HippyValue &propValue) override;
  
  void OnChildInserted(std::shared_ptr<BaseView> const &childView, int32_t index) override;
  void OnChildRemoved(std::shared_ptr<BaseView> const &childView) override;
  
private:
  ScrollNode scrollNode_;
  StackNode stackNode_;
};

} // namespace native
} // namespace render
} // namespace hippy
