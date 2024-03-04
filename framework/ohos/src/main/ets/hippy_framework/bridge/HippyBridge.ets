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

export interface HippyBridge {
  URI_SCHEME_ASSETS: string;
  URI_SCHEME_FILE: string;

  initJSBridge(gobalConfig: string, callback: NativeCallback, groupId: number);

  runScriptFromUri(uri: string, assetManager: AssetManager, canUseCodeCache: boolean,
  codeCacheTag: string, callback: NativeCallback): boolean;

  onDestroy();

  destroy(callback: NativeCallback, isReload: boolean);

  callFunction1(functionId: number, callback: NativeCallback, buffer: ByteBuffer);

  callFunction2(functionId: number, callback: NativeCallback, buffer: byte[]);

  callFunction3(functionId: number, callback: NativeCallback, buffer: byte[], offset: number,
   length: number);

  getV8RuntimeId(): number;
}

export interface BridgeCallback {

  callNatives(moduleName: string, moduleFunc: string, callId: string, params: ByteBuffer);

  reportException(message: string, stackTrace: string);

  reportException(e: Throwable);
}
