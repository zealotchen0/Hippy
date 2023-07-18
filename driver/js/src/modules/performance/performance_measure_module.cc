/*
 *
 * Tencent is pleased to support the open source community by making
 * Hippy available.
 *
 * Copyright (C) 2023 THL A29 Limited, a Tencent company.
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

#include "driver/modules/performance/performance_measure_module.h"

#include "driver/performance/performance_entry.h"
#include "footstone/time_point.h"
#include "footstone/string_view.h"

using string_view = footstone::string_view;

namespace hippy {
inline namespace driver {
inline namespace module {

std::shared_ptr<ClassTemplate<PerformanceMeasure>> RegisterPerformanceMeasure(const std::weak_ptr<Scope>& weak_scope) {
  ClassTemplate<PerformanceMeasure> class_template;
  class_template.name = "PerformanceMeasure";
  class_template.constructor = [weak_scope](
      const std::shared_ptr<CtxValue>& receiver,
      size_t argument_count,
      const std::shared_ptr<CtxValue> arguments[],
      void* external,
      std::shared_ptr<CtxValue>& exception) -> std::shared_ptr<PerformanceMeasure> {
    auto scope = weak_scope.lock();
    if (!scope) {
      return nullptr;
    }
    auto context = scope->GetContext();
    if (!external) {
      exception = context->CreateException("illegal constructor");
      return nullptr;
    }
    string_view name;
    auto flag = context->GetValueString(arguments[0], &name);
    if (!flag) {
      exception = context->CreateException("name error");
      return nullptr;
    }
    int32_t type;
    flag = context->GetValueNumber(arguments[1], &type);
    if (!flag || type < 0) {
      exception = context->CreateException("type error");
      return nullptr;
    }

    auto entries = scope->GetPerformance()->GetEntriesByName(name, static_cast<PerformanceEntry::Type>(type));
    if (entries.empty()) {
      exception = context->CreateException("entry not found");
      return nullptr;
    }
    return std::static_pointer_cast<PerformanceMeasure>(entries.back());
  };

  PropertyDefine<PerformanceMeasure> name_property_define;
  name_property_define.name = "detail";
  name_property_define.getter = [weak_scope](
      PerformanceMeasure* thiz,
      std::shared_ptr<CtxValue>& exception) -> std::shared_ptr<CtxValue> {
    auto scope = weak_scope.lock();
    if (!scope) {
      return nullptr;
    }
    auto context = scope->GetContext();
    auto detail = thiz->GetDetail();
    if (!detail.has_value()) {
      return context->CreateNull();
    }
    return std::any_cast<std::shared_ptr<CtxValue>>(detail);
  };
  class_template.properties.push_back(std::move(name_property_define));

  return std::make_shared<ClassTemplate<PerformanceMeasure>>(std::move(class_template));
}

}
}
}
