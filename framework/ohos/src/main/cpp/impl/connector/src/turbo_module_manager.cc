//
// Created on 2024/7/10.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

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

#include "connector/turbo_module_manager.h"

#include <cstdint>

#include "oh_napi/oh_napi_register.h"
#include "oh_napi/ark_ts.h"
#include "footstone/logging.h"
#include "footstone/string_view.h"
#include "footstone/string_view_utils.h"
#include "driver/napi/v8/v8_ctx.h"

using namespace hippy::napi;
using string_view = footstone::string_view;
using StringViewUtils = footstone::StringViewUtils;
using V8Ctx = hippy::V8Ctx;

// constexpr char kTurboKey[] = "getTurboModule";

namespace hippy {
inline namespace framework {
inline namespace turbo {



void GetTurboModule(CallbackInfo& info, void* data) {
  
}

void TurboModuleManager::Destroy(napi_env env, napi_callback_info info) {

}

int Install(napi_env env, napi_callback_info info) {
  FOOTSTONE_LOG(INFO) << "install TurboModuleManager";
//   ArkTS arkTs(env);
//   auto args = arkTs.GetCallbackArgs(info, 2);
//   uint32_t scope_id = static_cast<uint32_t>(arkTs.GetInteger(args[0]));

  return 0;
}

REGISTER_OH_NAPI("TurboModuleManager", "TurboModuleManager_Install", Install)
}
}
}