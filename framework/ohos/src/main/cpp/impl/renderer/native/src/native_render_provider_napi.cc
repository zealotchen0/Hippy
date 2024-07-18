/*
 * Tencent is pleased to support the open source community by making
 * Hippy available.
 *
 * Copyright (C) 2022 THL A29 Limited, a Tencent company.
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "renderer/native_render_provider_napi.h"
#include "renderer/native_render_manager.h"
#include "oh_napi/ark_ts.h"
#include "oh_napi/oh_napi_object.h"
#include "oh_napi/oh_napi_task_runner.h"
#include "oh_napi/oh_napi_invocation.h"
#include "oh_napi/oh_napi_register.h"
#include "oh_napi/oh_napi_utils.h"
#include "oh_napi/oh_measure_text.h"
#include "footstone/deserializer.h"
#include "footstone/hippy_value.h"
#include "dom/render_manager.h"
#include "dom/root_node.h"
#include "dom/scene.h"

using DomArgument = hippy::dom::DomArgument;
using DomEvent = hippy::dom::DomEvent;
using DomManager = hippy::dom::DomManager;
using HippyValue = footstone::value::HippyValue;
using NativeRenderManager = hippy::render::native::NativeRenderManager;
using RenderManager = hippy::dom::RenderManager;
using RootNode = hippy::dom::RootNode;
using Scene = hippy::dom::Scene;

namespace hippy {
inline namespace framework {
inline namespace renderer {
inline namespace native {

void CallRenderDelegateSetIdMethod(napi_env env, napi_ref render_provider_ref,
  const std::string& method, uint32_t id) {
  ArkTS arkTs(env);
  std::vector<napi_value> args = {
    arkTs.CreateUint32(id),
  };
  auto delegateObject = arkTs.GetObject(render_provider_ref);
  delegateObject.Call(method.c_str(), args);
}

void CallRenderDelegateMethod(napi_env env, napi_ref render_provider_ref,
  const std::string& method, uint32_t root_id, const std::pair<uint8_t*, size_t>& buffer) {
  OhNapiTaskRunner *taskRunner = OhNapiTaskRunner::Instance(env);
  taskRunner->RunAsyncTask([env = env, render_provider_ref = render_provider_ref, method, root_id, buffer]() {
    ArkTS arkTs(env);
    std::vector<napi_value> args = {
      arkTs.CreateUint32(root_id),
      arkTs.CreateExternalArrayBuffer(buffer.first, buffer.second)
    };
    auto delegateObject = arkTs.GetObject(render_provider_ref);
    delegateObject.Call(method.c_str(), args);
  });
}

void CallRenderDelegateMethod(napi_env env, napi_ref render_provider_ref,
  const std::string& method, uint32_t root_id) {
  OhNapiTaskRunner *taskRunner = OhNapiTaskRunner::Instance(env);
  taskRunner->RunAsyncTask([env = env, render_provider_ref = render_provider_ref, method, root_id]() {
    ArkTS arkTs(env);
    std::vector<napi_value> args = {
      arkTs.CreateUint32(root_id)
    };
    auto delegateObject = arkTs.GetObject(render_provider_ref);
    delegateObject.Call(method.c_str(), args);
  });
}

void CallRenderDelegateMoveNodeMethod(napi_env env, napi_ref render_provider_ref,
  const std::string& method, uint32_t root_id, uint32_t pid, const std::pair<uint8_t*, size_t>& buffer) {
  OhNapiTaskRunner *taskRunner = OhNapiTaskRunner::Instance(env);
  taskRunner->RunAsyncTask([env = env, render_provider_ref = render_provider_ref, method, root_id, pid, buffer]() {
    ArkTS arkTs(env);
    std::vector<napi_value> args = {
      arkTs.CreateUint32(root_id),
      arkTs.CreateUint32(pid),
      arkTs.CreateExternalArrayBuffer(buffer.first, buffer.second)
    };
    auto delegateObject = arkTs.GetObject(render_provider_ref);
    delegateObject.Call(method.c_str(), args);
  });
}

void CallRenderDelegateMoveNodeMethod(napi_env env, napi_ref render_provider_ref,
  const std::string& method, uint32_t root_id, std::vector<int32_t>& moved_ids, int32_t to_pid, int32_t from_pid, int32_t index) {
  OhNapiTaskRunner *taskRunner = OhNapiTaskRunner::Instance(env);
  taskRunner->RunAsyncTask([env = env, render_provider_ref = render_provider_ref, method, root_id, moved_ids, to_pid, from_pid, index]() {
    ArkTS arkTs(env);

    auto idsArray = std::vector<napi_value>();
    for (size_t i = 0; i < moved_ids.size(); i++) {
      idsArray.push_back(arkTs.CreateInt(moved_ids[i]));
    }

    std::vector<napi_value> args = {
      arkTs.CreateUint32(root_id),
      arkTs.CreateArray(idsArray),
      arkTs.CreateInt(to_pid),
      arkTs.CreateInt(from_pid),
      arkTs.CreateInt(index)
    };
    auto delegateObject = arkTs.GetObject(render_provider_ref);
    delegateObject.Call(method.c_str(), args);
  });
}

void CallRenderDelegateDeleteNodeMethod(napi_env env, napi_ref render_provider_ref,
  const std::string& method, uint32_t root_id, std::vector<uint32_t>& ids) {
  OhNapiTaskRunner *taskRunner = OhNapiTaskRunner::Instance(env);
  taskRunner->RunAsyncTask([env = env, render_provider_ref = render_provider_ref, method, root_id, ids]() {
    ArkTS arkTs(env);

    auto idsArray = std::vector<napi_value>();
    for (size_t i = 0; i < ids.size(); i++) {
      idsArray.push_back(arkTs.CreateUint32(ids[i]));
    }

    std::vector<napi_value> args = {
      arkTs.CreateUint32(root_id),
      arkTs.CreateArray(idsArray)
    };
    auto delegateObject = arkTs.GetObject(render_provider_ref);
    delegateObject.Call(method.c_str(), args);
  });
}

void CallRenderDelegateCallFunctionMethod(napi_env env, napi_ref render_provider_ref,
  const std::string& method, uint32_t root_id,
  uint32_t node_id, uint32_t cb_id, const std::string& functionName, const std::pair<uint8_t*, size_t>& buffer) {
  OhNapiTaskRunner *taskRunner = OhNapiTaskRunner::Instance(env);
  taskRunner->RunAsyncTask([env = env, render_provider_ref = render_provider_ref, method, root_id, node_id, cb_id, functionName, buffer]() {
    ArkTS arkTs(env);
    std::vector<napi_value> args = {
      arkTs.CreateUint32(root_id),
      arkTs.CreateUint32(node_id),
      arkTs.CreateUint32(cb_id),
      arkTs.CreateString(functionName),
      arkTs.CreateExternalArrayBuffer(buffer.first, buffer.second)
    };
    auto delegateObject = arkTs.GetObject(render_provider_ref);
    delegateObject.Call(method.c_str(), args);
  });
}

void CallRenderDelegateMeasureMethod(napi_env env, napi_ref render_provider_ref,
  const std::string& method, uint32_t root_id, uint32_t node_id,
  const float width, const int32_t width_mode, const float height, const int32_t height_mode, int64_t& result) {
  OhNapiTaskRunner *taskRunner = OhNapiTaskRunner::Instance(env);
  taskRunner->RunSyncTask([env = env, render_provider_ref = render_provider_ref, method, root_id, node_id, width, width_mode, height, height_mode, &result]() {
    ArkTS arkTs(env);
    std::vector<napi_value> args = {
      arkTs.CreateUint32(root_id),
      arkTs.CreateUint32(node_id),
      arkTs.CreateDouble(width),
      arkTs.CreateInt(width_mode),
      arkTs.CreateDouble(height),
      arkTs.CreateInt(height_mode),
    };
    auto delegateObject = arkTs.GetObject(render_provider_ref);
    auto resultNapiValue = delegateObject.Call(method.c_str(), args);
    result = arkTs.GetInt64(resultNapiValue);
  });
}

void CallRenderDelegateSpanPositionMethod(napi_env env, napi_ref render_provider_ref,
  const std::string& method, uint32_t root_id, uint32_t node_id, const float x, const float y) {
  OhNapiTaskRunner *taskRunner = OhNapiTaskRunner::Instance(env);
  taskRunner->RunAsyncTask([env = env, render_provider_ref = render_provider_ref, method, root_id, node_id, x, y]() {
    ArkTS arkTs(env);
    std::vector<napi_value> args = {
      arkTs.CreateUint32(root_id),
      arkTs.CreateUint32(node_id),
      arkTs.CreateDouble(x),
      arkTs.CreateDouble(y),
    };
    auto delegateObject = arkTs.GetObject(render_provider_ref);
    delegateObject.Call(method.c_str(), args);
  });
}

static napi_value UpdateRootSize(napi_env env, napi_callback_info info) {
  ArkTS arkTs(env);
  auto args = arkTs.GetCallbackArgs(info);
  uint32_t render_manager_id = static_cast<uint32_t>(arkTs.GetInteger(args[0]));
  uint32_t root_id = static_cast<uint32_t>(arkTs.GetInteger(args[1]));
  float width = static_cast<float>(arkTs.GetDouble(args[2]));
  float height = static_cast<float>(arkTs.GetDouble(args[3]));

  auto& map = NativeRenderManager::PersistentMap();
  std::shared_ptr<NativeRenderManager> render_manager;
  bool ret = map.Find(render_manager_id, render_manager);
  if (!ret) {
    FOOTSTONE_DLOG(WARNING) << "UpdateRootSize render_manager_id invalid";
    return arkTs.GetUndefined();
  }

  std::shared_ptr<DomManager> dom_manager = render_manager->GetDomManager();
  if (dom_manager == nullptr) {
    FOOTSTONE_DLOG(WARNING) << "UpdateRootSize dom_manager is nullptr";
    return arkTs.GetUndefined();
  }

  auto& root_map = RootNode::PersistentMap();
  std::shared_ptr<RootNode> root_node;
  ret = root_map.Find(root_id, root_node);
  if (!ret) {
    FOOTSTONE_DLOG(WARNING) << "UpdateRootSize root_node is nullptr";
    return arkTs.GetUndefined();
  }

  std::vector<std::function<void()>> ops;
  ops.emplace_back([dom_manager, root_node, width, height]{
    FOOTSTONE_LOG(INFO) << "update root size width = " << width << ", height = " << height << std::endl;
    dom_manager->SetRootSize(root_node, width, height);
    dom_manager->DoLayout(root_node);
    dom_manager->EndBatch(root_node);
  });
  dom_manager->PostTask(Scene(std::move(ops)));

  return arkTs.GetUndefined();
}

static napi_value UpdateNodeSize(napi_env env, napi_callback_info info) {
  ArkTS arkTs(env);
  auto args = arkTs.GetCallbackArgs(info);
  uint32_t render_manager_id = static_cast<uint32_t>(arkTs.GetInteger(args[0]));
  uint32_t root_id = static_cast<uint32_t>(arkTs.GetInteger(args[1]));
  uint32_t node_id = static_cast<uint32_t>(arkTs.GetInteger(args[2]));
  double width = arkTs.GetDouble(args[3]);
  double height = arkTs.GetDouble(args[4]);

  auto& map = NativeRenderManager::PersistentMap();
  std::shared_ptr<NativeRenderManager> render_manager;
  bool ret = map.Find(render_manager_id, render_manager);
  if (!ret) {
    FOOTSTONE_DLOG(WARNING) << "UpdateNodeSize render_manager_id invalid";
    return arkTs.GetUndefined();
  }

  std::shared_ptr<DomManager> dom_manager = render_manager->GetDomManager();
  if (dom_manager == nullptr) {
    FOOTSTONE_DLOG(WARNING) << "UpdateNodeSize dom_manager is nullptr";
    return arkTs.GetUndefined();
  }

  auto& root_map = RootNode::PersistentMap();
  std::shared_ptr<RootNode> root_node;
  ret = root_map.Find(root_id, root_node);
  if (!ret) {
    FOOTSTONE_DLOG(WARNING) << "UpdateNodeSize root_node is nullptr";
    return arkTs.GetUndefined();
  }

  auto node = dom_manager->GetNode(root_node, node_id);
  if (node == nullptr) {
    FOOTSTONE_DLOG(WARNING) << "UpdateNodeSize DomNode not found for id: " << node_id;
    return arkTs.GetUndefined();
  }

  std::unordered_map<std::string, std::shared_ptr<HippyValue>> update_style;
  std::shared_ptr<HippyValue> width_value = std::make_shared<HippyValue>(width);
  std::shared_ptr<HippyValue> height_value = std::make_shared<HippyValue>(height);
  update_style.insert({"width", width_value});
  update_style.insert({"height", height_value});

  std::vector<std::function<void()>> ops = {[dom_manager, root_node, node, update_style]{
    node->UpdateDomNodeStyleAndParseLayoutInfo(update_style);
    dom_manager->EndBatch(root_node);
  }};
  dom_manager->PostTask(Scene(std::move(ops)));

  return arkTs.GetUndefined();
}

static napi_value OnReceivedEvent(napi_env env, napi_callback_info info) {
  ArkTS arkTs(env);
  auto args = arkTs.GetCallbackArgs(info);
  uint32_t render_manager_id = static_cast<uint32_t>(arkTs.GetInteger(args[0]));
  uint32_t root_id = static_cast<uint32_t>(arkTs.GetInteger(args[1]));
  uint32_t node_id = static_cast<uint32_t>(arkTs.GetInteger(args[2]));
  std::string event_name = arkTs.GetString(args[3]);

  void *buffer_data = NULL;
  size_t byte_length = 0;
  if (arkTs.IsArrayBuffer(args[4])) {
    arkTs.GetArrayBufferInfo(args[4], &buffer_data, &byte_length);
  }

  bool capture = arkTs.GetBoolean(args[5]);
  bool bubble = arkTs.GetBoolean(args[6]);

  auto& map = NativeRenderManager::PersistentMap();
  std::shared_ptr<NativeRenderManager> render_manager;
  bool ret = map.Find(render_manager_id, render_manager);
  if (!ret) {
    FOOTSTONE_DLOG(WARNING) << "OnReceivedEvent render_manager_id invalid";
    return arkTs.GetUndefined();
  }

  auto& root_map = RootNode::PersistentMap();
  std::shared_ptr<RootNode> root_node;
  ret = root_map.Find(root_id, root_node);
  if (!ret) {
    FOOTSTONE_DLOG(WARNING) << "OnReceivedEvent root_node is nullptr";
    return arkTs.GetUndefined();
  }

  std::shared_ptr<HippyValue> params = nullptr;
  if (buffer_data != nullptr && byte_length > 0) {
    params = std::make_shared<HippyValue>();
    footstone::value::Deserializer deserializer(static_cast<const uint8_t*>(buffer_data), byte_length);
    deserializer.ReadHeader();
    deserializer.ReadValue(*params);
  }

  render_manager->ReceivedEvent(root_node, node_id, event_name, params, capture, bubble);

  return arkTs.GetUndefined();
}

static napi_value DoCallBack(napi_env env, napi_callback_info info) {
  ArkTS arkTs(env);
  auto args = arkTs.GetCallbackArgs(info);
  uint32_t render_manager_id = static_cast<uint32_t>(arkTs.GetInteger(args[0]));
  //int32_t result = arkTs.GetInteger(args[1]);
  std::string func_name = arkTs.GetString(args[2]);
  uint32_t root_id = static_cast<uint32_t>(arkTs.GetInteger(args[3]));
  uint32_t node_id = static_cast<uint32_t>(arkTs.GetInteger(args[4]));
  uint32_t cb_id = static_cast<uint32_t>(arkTs.GetInteger(args[5]));

  void *buffer_data = NULL;
  size_t byte_length = 0;
  arkTs.GetArrayBufferInfo(args[6], &buffer_data, &byte_length);

  auto& map = NativeRenderManager::PersistentMap();
  std::shared_ptr<NativeRenderManager> render_manager;
  bool ret = map.Find(render_manager_id, render_manager);
  if (!ret) {
    FOOTSTONE_DLOG(WARNING) << "DoCallBack render_manager_id invalid";
    return arkTs.GetUndefined();
  }

  std::shared_ptr<DomManager> dom_manager = render_manager->GetDomManager();
  if (dom_manager == nullptr) {
    FOOTSTONE_DLOG(WARNING) << "DoCallBack dom_manager is nullptr";
    return arkTs.GetUndefined();
  }

  std::shared_ptr<HippyValue> params = std::make_shared<HippyValue>();
  if (buffer_data != nullptr && byte_length > 0) {
    footstone::value::Deserializer deserializer(static_cast<const uint8_t*>(buffer_data), byte_length);
    deserializer.ReadHeader();
    deserializer.ReadValue(*params);
  }

  std::vector<std::function<void()>> ops = {[root_id, node_id, cb_id, func_name, dom_manager, params]{
    auto& root_map = RootNode::PersistentMap();
    std::shared_ptr<RootNode> root_node;
    bool ret = root_map.Find(root_id, root_node);
    if (!ret) {
      FOOTSTONE_DLOG(WARNING) << "DoCallBack root_node is nullptr";
      return;
    }

    auto node = dom_manager->GetNode(root_node, node_id);
    if (node == nullptr) {
      FOOTSTONE_DLOG(WARNING) << "DoCallBack DomNode not found for id: " << node_id;
      return;
    }

    auto callback = node->GetCallback(func_name, cb_id);
    if (callback == nullptr) {
      FOOTSTONE_DLOG(WARNING) << "DoCallBack Callback not found for func_name: " << func_name;
      return;
    }

    callback(std::make_shared<DomArgument>(*params));
  }};
  dom_manager->PostTask(Scene(std::move(ops)));

  return arkTs.GetUndefined();
}

static napi_value DoMeasureText(napi_env env, napi_callback_info info) {
  ArkTS arkTs(env);
  auto args = arkTs.GetCallbackArgs(info);

  uint32_t render_manager_id = static_cast<uint32_t>(arkTs.GetInteger(args[0]));
  auto &map = NativeRenderManager::PersistentMap();
  std::shared_ptr<NativeRenderManager> render_manager;
  bool ret = map.Find(render_manager_id, render_manager);
  if (!ret) {
    FOOTSTONE_DLOG(WARNING) << "UpdateNodeSize render_manager_id invalid";
    return arkTs.GetUndefined();
  }

//   auto al = arkTs.GetArrayLength(args[1]);//把属性解析到map

  int width=arkTs.GetInteger(args[2]);
  int widthMode = arkTs.GetInteger(args[3]);
  int height = arkTs.GetInteger(args[4]);
  int heightMode = arkTs.GetInteger(args[5]);
  float density = render_manager->GetDensity();

  uint32_t p = 0;
  OhMeasureText measureInst;
  OhMeasureResult result;
  while (true) {
    auto measureFlag = arkTs.GetString(arkTs.GetArrayElement(args[1], p++));
    int propCount=std::stoi(arkTs.GetString(arkTs.GetArrayElement(args[1], p++)));
    std::map<std::string, std::string> propMap;
    for (int i = 0; i < propCount; i++) {
      auto propName = arkTs.GetString(arkTs.GetArrayElement(args[1], p++));
      auto propValue = arkTs.GetString(arkTs.GetArrayElement(args[1], p++));
      propMap[propName] = propValue;
    }
    if(measureFlag=="measure_add_start"){
      measureInst.StartMeasure(propMap);
    } else if(measureFlag=="measure_add_text"){
      measureInst.AddText(propMap);
    } else if(measureFlag=="measure_add_image"){
      measureInst.AddImage(propMap);
    } else if(measureFlag=="measure_add_end"){
      result = measureInst.EndMeasure(width, widthMode, height, heightMode, density);
      break;
    }
  }

  std::vector<napi_value> pack;
  pack.push_back(arkTs.CreateDouble(result.width));
  pack.push_back(arkTs.CreateDouble(result.height));
  return arkTs.CreateArray(pack);
}

void NapiValue2HippyValue(struct napi_env__ * env, napi_value__ * pairItem2) {

}

// HippyValue CallGetLocationOnScreenMethod(napi_env env, napi_ref render_provider_ref,
//                                          const std::string &method, uint32_t component_id) {
//   //   std::future<HippyValue> futHippyValue;
//   //   auto asyncTask = [&]() {
//   //     ArkTS arkTs(env);
//   //     std::vector<napi_value> args = {arkTs.CreateUint32(component_id)};
//   //     auto delegateObject = arkTs.GetObject(render_provider_ref);
//   //     auto napiValue = delegateObject.Call(method.c_str(), args);
//   //     return OhNapiUtils::NapiValue2HippyValue(env, napiValue);
//   //   };
//   //   futHippyValue = std::async(std::launch::async, asyncTask);
//   //   return futHippyValue.get();
//   HippyValue futHippyValue;
//   OhNapiTaskRunner *taskRunner = OhNapiTaskRunner::Instance(env);
//   taskRunner->RunSyncTask([env = env, render_provider_ref = render_provider_ref, method,
//                            component_id, &futHippyValue]() {
//     ArkTS arkTs(env);
//     std::vector<napi_value> args = {arkTs.CreateUint32(component_id)};
//     auto delegateObject = arkTs.GetObject(render_provider_ref);
//     auto napiValue = delegateObject.Call(method.c_str(), args);
//    
//     HippyValueObjectType map;
//     OhNapiObject napiObj = arkTs.GetObject(napiValue);
//     std::vector<napi_value> napiObjArgs0 = {arkTs.CreateString("xOnScreen")};
//     auto napiCallValue0 = napiObj.Call("get", napiObjArgs0);
//     double ohNapiCallValue0 = arkTs.GetDouble(napiCallValue0);
//     map["xOnScreen"].ToDouble(ohNapiCallValue0);
//
//     std::vector<napi_value> napiObjArgs1 = {arkTs.CreateString("yOnScreen")};
//     auto napiCallValue1 = napiObj.Call("get", napiObjArgs1);
//     double ohNapiCallValue1 = arkTs.GetDouble(napiCallValue1);
//     map["yOnScreen"].ToDouble(ohNapiCallValue1);
//
//     std::vector<napi_value> napiObjArgs2 = {arkTs.CreateString("viewWidth")};
//     auto napiCallValue2 = napiObj.Call("get", napiObjArgs2);
//     double ohNapiCallValue2 = arkTs.GetDouble(napiCallValue2);
//     map["viewWidth"].ToDouble(ohNapiCallValue2);
//
//     std::vector<napi_value> napiObjArgs3 = {arkTs.CreateString("viewHeight")};
//     auto napiCallValue3 = napiObj.Call("get", napiObjArgs3);
//     double ohNapiCallValue3 = arkTs.GetDouble(napiCallValue3);
//     map["viewHeight"].ToDouble(ohNapiCallValue3);
//
//     ohNapiCallValue0 = 30;
//     ohNapiCallValue1 = 40;
//     ohNapiCallValue2 = 100;
//     ohNapiCallValue3 = 60;
//     map["xOnScreen"] = ohNapiCallValue0;
//     map["yOnScreen"] = ohNapiCallValue1;
//     map["viewWidth"] = ohNapiCallValue2;
//     map["viewHeight"] = ohNapiCallValue3;
//     //     std::vector<std::pair<napi_value, napi_value>> pairs = napiObj.GetALLObjectProperties();
//     //     for (auto it = pairs.begin(); it != pairs.end(); it++) {
//     //       auto &pair = *it;
//     //       auto &pairItem1 = pair.first;
//     //       auto objKey = arkTs.GetString(pairItem1);
//     //       if (objKey.length() > 0) {
//     //         auto &pairItem2 = pair.second;
//     //                 OhNapiObject callNapi = arkTs.GetObject(pairItem2);
//     //                 std::vector<napi_value> callNapiArgs = {};
//     //                 std::vector<napi_value> args =
//     //                 callNapi.Call("", )
//     //         auto objValue = OhNapiUtils::NapiValue2HippyValue(env, pairItem2);
//     //         map[objKey] = objValue;
//     //       }
//     //     }
//     futHippyValue = HippyValue(map);
//     //         ohNapiObject.GetKeyValuePairs();
//     //     futHippyValue = OhNapiUtils::NapiValue2HippyValue(env, napiValue);
//   });
//   //     HippyValue hippyValue = HippyValue();
//   return futHippyValue;
// }


HippyValue CallGetLocationOnScreenMethod(napi_env env, napi_ref render_provider_ref,
                                         const std::string &method, uint32_t component_id) {
  HippyValue futHippyValue;
  OhNapiTaskRunner *taskRunner = OhNapiTaskRunner::Instance(env);
  taskRunner->RunSyncTask([env = env, render_provider_ref = render_provider_ref, method,
                           component_id, &futHippyValue]() {
    ArkTS arkTs(env);
    std::vector<napi_value> args = {arkTs.CreateUint32(component_id)};
    auto delegateObject = arkTs.GetObject(render_provider_ref);
    auto napiValue = delegateObject.Call(method.c_str(), args);

    HippyValueObjectType map;
    OhNapiObject napiObj = arkTs.GetObject(napiValue);
    std::vector<std::pair<napi_value, napi_value>> pairs = napiObj.GetKeyValuePairs();
    for (auto it = pairs.begin(); it != pairs.end(); it++) {
      auto &pair = *it;
      auto &pairItem1 = pair.first;
      auto objKey = arkTs.GetString(pairItem1);
      if (objKey.length() > 0) {
        auto &pairItem2 = pair.second;
        auto objValue = OhNapiUtils::NapiValue2HippyValue(env, pairItem2);
        map[objKey] = objValue;
      }
    }
    futHippyValue = HippyValue(map);
  });
  return futHippyValue;
}

napi_value HanderCallback(napi_env env, napi_callback_info info) {

  size_t argc = 0;
  napi_value args[0];
  void *data;
  napi_get_cb_info(env, info, &argc, args, nullptr, &data);
  auto scopeCallback = reinterpret_cast<std::function<void()> *>(data);
  if (scopeCallback) {
    (*scopeCallback)();
  }


  FOOTSTONE_LOG(ERROR) << "wangz::napi_value";
  ArkTS arkTs(env);
  auto args1 = arkTs.GetCallbackArgs(info, 1);
  HippyValueObjectType map;
  OhNapiObject napiObj = arkTs.GetObject(args1[0]);
  std::vector<std::pair<napi_value, napi_value>> pairs = napiObj.GetKeyValuePairs();
  for (auto it = pairs.begin(); it != pairs.end(); it++) {
    auto &pair = *it;
    auto &pairItem1 = pair.first;
    auto objKey = arkTs.GetString(pairItem1);
    if (objKey.length() > 0) {
      auto &pairItem2 = pair.second;
      auto objValue = OhNapiUtils::NapiValue2HippyValue(env, pairItem2);
      map[objKey] = objValue;
    }
  }
  //       HippyValue(map);
  FOOTSTONE_LOG(ERROR) << "wangz::pairs.size()::" << pairs.size();
  return nullptr;
}

struct CallbackData {
    napi_threadsafe_function tsfn;
    napi_async_work work;
};

// void CallGetComponentSnapshotMethod(napi_env env, napi_ref render_provider_ref,
//                                     const std::string &method, uint32_t component_id,
//                                     HippyValue &resultMap) {
//   OhNapiTaskRunner *taskRunner = OhNapiTaskRunner::Instance(env);
//   taskRunner->RunSyncTask([env = env, render_provider_ref = render_provider_ref, method,
//                            component_id, &resultMap]() {
//     HippyValue& innerValue = resultMap;    
//     auto scopeCallback = [&innerValue](napi_env copyEnv, HippyValue &v, HippyValue *vPtr) {
//       HippyValueObjectType m;
//       v.ToObject(m);
//       std::string screenShot = "";
//       auto angleIt = m.find("screenShot");
//       if (angleIt == m.end()) {
//       } else {
//         HippyValue hippyValue = angleIt->second;
//         hippyValue.ToString(screenShot);
//       }
//       FOOTSTONE_LOG(ERROR) << "wangz::scopeCallback::" << screenShot;
//       //       OhNapiUtils::HippyValue2NapiValue(copyEnv, resultMap);
//       //         auto copy = OhNapiUtils::HippyValue2NapiValue(copyEnv, v);
//       //     resultMap = OhNapiUtils::NapiValue2HippyValue(copyEnv, copy);
//       HippyValueObjectType m2;
// //       vPtr->ToObject(m2);
//       innerValue.ToObject(m2);
//       std::string screenShot2 = "";
//       auto angleIt2 = m.find("screenShot");
//       if (angleIt2 == m.end()) {
//       } else {
//         HippyValue hippyValue2 = angleIt2->second;
//         hippyValue2.ToString(screenShot2);
//       }
//       FOOTSTONE_LOG(ERROR) << "wangz::scopeCallback222::" << screenShot2;
//     };
//     ScopeDebug* scopePtr = new ScopeDebug();
//     //     ScopeDebug* scopePtr = new ScopeDebug{scopeCallback};
//     scopePtr->name = "12345";
//     scopePtr->hippyValue = &resultMap;
//     scopePtr->dataScope =
//         new std::function<void(napi_env, footstone::HippyValue &, footstone::HippyValue *)>(
//             scopeCallback);
//     NapiCallback napiCallback = [](napi_env env, napi_callback_info info) -> napi_value {
//       FOOTSTONE_LOG(ERROR) << "wangz::scope";
//       size_t argc = 0;
//       napi_get_cb_info(env, info, &argc, nullptr, nullptr, nullptr);
//       if (argc == 0) {
//         return nullptr;
//       }
//       napi_value args[argc];
//       void *data;
//       napi_get_cb_info(env, info, &argc, args, nullptr, &data);
//      
//       FOOTSTONE_LOG(ERROR) << "wangz::napi_value";
//       ArkTS arkTs(env);
//       auto args1 = arkTs.GetCallbackArgs(info, 1);
//       HippyValueObjectType map;
//       OhNapiObject napiObj = arkTs.GetObject(args1[0]);
//       std::vector<std::pair<napi_value, napi_value>> pairs = napiObj.GetKeyValuePairs();
//       for (auto it = pairs.begin(); it != pairs.end(); it++) {
//         auto &pair = *it;
//         auto &pairItem1 = pair.first;
//         auto objKey = arkTs.GetString(pairItem1);
//         if (objKey.length() > 0) {
//           FOOTSTONE_LOG(ERROR) << "wangz::pairs.name::" << objKey;
//           auto &pairItem2 = pair.second;
//           auto objValue = OhNapiUtils::NapiValue2HippyValue(env, pairItem2);
//           map[objKey] = objValue;
//         }
//       }
//       HippyValue resultMap = HippyValue(map);   
//       auto scopeCallback = reinterpret_cast<ScopeDebug *>(data);
//       if (scopeCallback && scopeCallback->dataScope) {
//         (*scopeCallback->dataScope)(env, resultMap, scopeCallback->hippyValue);
//       }      
//       FOOTSTONE_LOG(ERROR) << "wangz::pairs.size()::" << pairs.size();
//       return nullptr;
//     };
//
//     napi_value cb;
//     OhNapiUtils::CreateCB(env, cb, napiCallback, scopePtr);
//
//     ArkTS arkTs(env);
//     std::vector<napi_value> args = {arkTs.CreateUint32(component_id),
//                                     OhNapiUtils::HippyValue2NapiValue(env, resultMap), cb};
//     auto delegateObject = arkTs.GetObject(render_provider_ref);
//     delegateObject.Call(method.c_str(), args);
//     //     FOOTSTONE_DLOG(WARNING) << "UpdateNodeSize render_manager_id invalid";
//     //     napi_value jsCb = delegateObject.Call(method.c_str(), args);
//     //     napi_value thenFunc;
//     //     napi_value undefined = nullptr;
//     //     napi_value promise = nullptr;
//     //     napi_get_undefined(env, &undefined);
//     //     napi_call_function(env, undefined, jsCb, 0, nullptr, &promise);
//     //     if (napi_get_named_property(env, jsCb, "then", &thenFunc) != napi_ok) {
//     //       return;
//     //     }
//     //     napi_value argv[2] = {};
//     //     napi_call_function(env, jsCb, thenFunc, 2, argv, &undefined);
//     //     FOOTSTONE_LOG(ERROR) << "wangz::GetString" << arkTs.GetString(undefined);
//     //     OhNapiObject napiObj = arkTs.GetObject(thenValue);
//     //     std::vector<std::pair<napi_value, napi_value>> pairs = napiObj.GetKeyValuePairs();
//     //     std::vector<napi_value> args2 = {};
//     //     napi_value resultNapiValue2 = napiObj.Call("then", args2);
//     //     auto hvalue = OhNapiUtils::NapiValue2HippyValue(env, resultNapiValue2);
//     //     for (auto it = pairs.begin(); it != pairs.end(); it++) {
//     //             hvalue.ToStringChecked();
//     //     }
//
//     //       auto &pair = *it;
//     //       auto &pairItem1 = pair.first;
//     //       auto objKey = arkTs.GetString(pairItem1);
//     //       if (objKey.length() > 0) {
//     //         auto &pairItem2 = pair.second;
//     //         auto objValue = OhNapiUtils::NapiValue2HippyValue(env, pairItem2);
//     //       }
//     //       if (objKey == "then") {
//     //         std::vector<napi_value> args2 = {};
//     //         auto promObject = arkTs.GetObject(thenValue);
//     //         napi_value resultNapiValue2 = promObject.Call(objKey, args2);
//     //         OhNapiUtils::NapiValue2HippyValue(env, resultNapiValue2);
//     //       }
//     //     }
//
//     //     OhNapiUtils::CallThen(env, thenValue);
//     //     OhNapiUtils::CallThen(env, arkTs.GetReferenceValue(render_provider_ref));
//     //         auto hippyThenValue = OhNapiUtils::NapiValue2HippyValue(env, thenValue);
//     //
//     //         auto promObject = arkTs.GetObject(thenValue);
//     //         std::vector<napi_value> args2 = {};
//     //         napi_value resultNapiValue2 = promObject.Call("then", args2);
//     //         hippyThenValue = OhNapiUtils::NapiValue2HippyValue(env, resultNapiValue2);
//     //
//     //         HippyValueObjectType resultType;
//     //         hippyThenValue.ToObject(resultType);
//     //         std::string screenShot;
//     //         int width;
//     //         int height;
//     //         double screenScale;
//     //         resultType["screenShot"].ToString(screenShot);
//     //         resultType["width"].ToInt32(width);
//     //         resultType["height"].ToInt32(height);
//     //         resultType["screenScale"].ToDouble(screenScale);
//        
//        
//   });
// }

void CallGetComponentSnapshotMethod(napi_env env, napi_ref render_provider_ref,
  const std::string &method, uint32_t component_id, std::shared_ptr<SnapshotResult> snapshotResult) {
  OhNapiTaskRunner *taskRunner = OhNapiTaskRunner::Instance(env);
  auto scopeCallback = [&](napi_env copyEnv, HippyValue &v) {
    HippyValueObjectType m;
    v.ToObject(m);
    std::string screenShot = "";
    auto angleIt = m.find("screenShot");
    if (angleIt == m.end()) {
    } else {
      HippyValue hippyValue = angleIt->second;
      hippyValue.ToString(screenShot);
    }
    FOOTSTONE_LOG(ERROR) << "wangz::scopeCallback::" << screenShot;
        
    //下面代码会出现内存访问错误   
    FOOTSTONE_LOG(ERROR) << "wangz::in::snapshotResult.height::" << snapshotResult->height;
    FOOTSTONE_LOG(ERROR) << "wangz::in::snapshotResult.width::" << snapshotResult->width;
    snapshotResult->height = 55;    
  };

  NapiCallback napiCallback = [](napi_env env, napi_callback_info info) -> napi_value {
    FOOTSTONE_LOG(ERROR) << "wangz::scope";
    size_t argc = 0;
    napi_get_cb_info(env, info, &argc, nullptr, nullptr, nullptr);
    if (argc == 0) {
      return nullptr;
    }
    napi_value args[argc];
    void *data;
    napi_get_cb_info(env, info, &argc, args, nullptr, &data);

    FOOTSTONE_LOG(ERROR) << "wangz::napi_value";
    ArkTS arkTs(env);
    auto args1 = arkTs.GetCallbackArgs(info, 1);
    HippyValueObjectType map;
    OhNapiObject napiObj = arkTs.GetObject(args1[0]);
    std::vector<std::pair<napi_value, napi_value>> pairs = napiObj.GetKeyValuePairs();
    for (auto it = pairs.begin(); it != pairs.end(); it++) {
      auto &pair = *it;
      auto &pairItem1 = pair.first;
      auto objKey = arkTs.GetString(pairItem1);
      if (objKey.length() > 0) {
        FOOTSTONE_LOG(ERROR) << "wangz::pairs.name::" << objKey;
        auto &pairItem2 = pair.second;
        auto objValue = OhNapiUtils::NapiValue2HippyValue(env, pairItem2);
        map[objKey] = objValue;
      }
    }
    HippyValue resultMap = HippyValue(map);
    auto scopeCallback = reinterpret_cast<ScopeDebug *>(data);
    if (scopeCallback && scopeCallback->dataScope) {
      (*scopeCallback->dataScope)(env, resultMap);
    }
    FOOTSTONE_LOG(ERROR) << "wangz::pairs.size()::" << pairs.size();
    return nullptr;
  };

  taskRunner->RunSyncTask([env = env, render_provider_ref = render_provider_ref, method,
                           component_id, &scopeCallback, &napiCallback]() {
    ScopeDebug* scopePtr = new ScopeDebug();
    scopePtr->dataScope = new std::function<void(napi_env, footstone::HippyValue &)>(scopeCallback);
    napi_value cb;
    OhNapiUtils::CreateCB(env, cb, napiCallback, scopePtr); //CreateCB封装了napi_create_function(env, nullptr, 0, napiCallback, scopePtr, &cb)   

    ArkTS arkTs(env);
    HippyValue resultMap;
    std::vector<napi_value> args = {arkTs.CreateUint32(component_id),
                                    OhNapiUtils::HippyValue2NapiValue(env, resultMap), cb};
    auto delegateObject = arkTs.GetObject(render_provider_ref);
    delegateObject.Call(method.c_str(), args);
  });
}

REGISTER_OH_NAPI("NativeRenderProvider", "NativeRenderProvider_UpdateRootSize", UpdateRootSize)
REGISTER_OH_NAPI("NativeRenderProvider", "NativeRenderProvider_UpdateNodeSize", UpdateNodeSize)
REGISTER_OH_NAPI("NativeRenderProvider", "NativeRenderProvider_OnReceivedEvent", OnReceivedEvent)
REGISTER_OH_NAPI("NativeRenderProvider", "NativeRenderProvider_DoCallBack", DoCallBack)
REGISTER_OH_NAPI("NativeRenderProvider", "NativeRenderProvider_DoMeasureText", DoMeasureText)

}
}
}
}
