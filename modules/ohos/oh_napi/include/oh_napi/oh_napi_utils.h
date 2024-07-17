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

#include "footstone/hippy_value.h"
#include "oh_napi/ark_ts.h"

using HippyValue = footstone::HippyValue;
using NapiCallback = napi_value (*)(napi_env, napi_callback_info);
// using NapiCallback = std::function<napi_value(napi_env env, napi_callback_info info)>;

class OhNapiUtils {
public:
    static void CreateCB(napi_env env, napi_value &value, NapiCallback callbackC,
                         std::function<void()> scopeCallback);
    static HippyValue CallThen(napi_env env, napi_value value);
    static HippyValue NapiValue2HippyValue(napi_env env, napi_value value);
    static napi_value HippyValue2NapiValue(napi_env env, const HippyValue &value);
};
