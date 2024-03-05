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
import { BinaryWriter } from '../../serialization/writer/BinaryWriter';
import { HippyAny, HippyMap, HippyValue } from '../../support/common/HippyTypes';
import { ArgumentUtils } from '../../support/utils/ArgumentUtils';
import { I18nUtil } from '../../support/utils/I18nUtil';
import { HippyThirdPartyAdapter } from '../adapter/thirdparty/HippyThirdPartyAdapter';
import { Serializer } from '../serialization/Serializer';
import { HippyBundleLoader } from './bundleloader/HippyBundleLoader';
import { HippyBridgeManager } from './HippyBridgeManager';

export enum BridgeState {
  UNINITIALIZED = 0,
  INITIALIZED = 1,
  DESTROYED = 2,
}

export class HippyBridgeManagerImpl implements HippyBridgeManager {
  // Bridge 状态
  public static MSG_CODE_INIT_BRIDGE = 10;
  public static MSG_CODE_RUN_BUNDLE = 11;
  public static MSG_CODE_CALL_FUNCTION = 12;
  public static MSG_CODE_DESTROY_BRIDGE = 13;
  public static MSG_CODE_ON_BRIDGE_DESTROYED = 14;
  // Instance 状态
  public static FUNCTION_ACTION_LOAD_INSTANCE = 1;
  public static FUNCTION_ACTION_RESUME_INSTANCE = 2;
  public static FUNCTION_ACTION_PAUSE_INSTANCE = 3;
  public static FUNCTION_ACTION_DESTROY_INSTANCE = 4;
  public static FUNCTION_ACTION_CALLBACK = 5;
  public static FUNCTION_ACTION_CALL_JS_MODULE = 6;
  public static FUNCTION_ACTION_ON_WEBSOCKET_MESSAGE = 7;
  // Bridge状态定义
  public static BRIDGE_TYPE_SINGLE_THREAD = 2;
  public static BRIDGE_TYPE_NORMAL = 1;
  // 销毁状态
  public static DESTROY_CLOSE = 0;
  public static DESTROY_RELOAD = 1;

  public mContext: HippyEngineContext;
  public mCoreBundleLoader: HippyBundleLoader;
  public mEnableV8Serialization: boolean;
  public mIsDevModule: boolean;
  public mDebugServerHost: string;
  public mGroupId: number;
  public mThirdPartyAdapter: HippyThirdPartyAdapter;
  public mV8InitParams: V8InitParams;
  public mJsDriver: JsDriver;

  public constructor(
    context: HippyEngineContext,
    coreBundleLoader: HippyBundleLoader,
    enableV8Serialization: boolean,
    isDevModule: boolean,
    debugServerHost: string,
    groupId: number,
    thirdPartyAdapter: HippyThirdPartyAdapter,
    v8InitParams: V8InitParams,
    jsDriver: JsDriver) {

    this.mContext = context;
    this.mCoreBundleLoader = coreBundleLoader;
    this.mGroupId = groupId;
    this.mThirdPartyAdapter = thirdPartyAdapter;
    this.mEnableV8Serialization = enableV8Serialization;
    this.mHippyBridge = new HippyBridgeImpl(
      context,
      this,
      enableV8Serialization,
      isDevModule,
      debugServerHost,
      v8InitParams,
      jsDriver);
    if (enableV8Serialization) {
      // compatibleSerializer = new Serializer();
      // recommendSerializer = new com.tencent.mtt.hippy.serialization.recommend.Serializer();
    } else {
      // mStringBuilder = new StringBuilder(1024);
    }
  }

  initBridge(): void {
    this.mHippyBridge.initJSBridge(getGlobalConfigs(), new NativeCallback(mHandler) {
      @Override
      public void Call(long result, Message message, String action,
      String reason) {
        if (result != 0 || mBridgeState == BridgeState.DESTROYED) {
          String info =
            "initJSBridge error: result " + result + ", reason "
              + reason + ", bridge state " + mBridgeState;
          reportException(new Throwable(info));
          return;
        }
        long runtimeId = mHippyBridge.getV8RuntimeId();
        if (mContext != null) {
          mContext.onRuntimeInitialized();
        }
        if (enableTurbo()) {
          mTurboModuleManager = new TurboModuleManager(mContext);
          mTurboModuleManager.install(runtimeId);
        }
        if (mThirdPartyAdapter != null) {
          mThirdPartyAdapter.onRuntimeInit(runtimeId);
        }
        if (mCoreBundleLoader != null) {
          final TimeMonitor timeMonitor = mContext.getMonitor();
          timeMonitor.startPoint(MonitorGroupType.ENGINE_INITIALIZE,
            TimeMonitor.MONITOR_POINT_LOAD_COMMON_JS);
          mCoreBundleLoader
            .load(mHippyBridge, new NativeCallback(mHandler) {
            @Override
            public void Call(long result, Message message,
            String action, String reason) {
              if (mBridgeState == BridgeState.DESTROYED) {
                return;
              }
              RuntimeException exception = null;
              if (result == 0) {
                mBridgeState = BridgeState.INITIALIZED;
              } else {
                exception = new RuntimeException(
                  "load coreJsBundle failed, check your core jsBundle:"
                    + reason);
              }
              timeMonitor.startPoint(
                MonitorGroupType.ENGINE_INITIALIZE,
                TimeMonitor.MONITOR_POINT_NOTIFY_ENGINE_INITIALIZED);
              callback.callback((result == 0), exception);
            }
          });
        } else {
          mBridgeState = BridgeState.INITIALIZED;
          callback.callback(true, null);
        }
      }
    }, mGroupId);
  }

  runBundle(id: number, loader: HippyBundleLoader): void {
    throw new Error('Method not implemented.');
  }

  resumeInstance(id: number): void {
    throw new Error('Method not implemented.');
  }

  pauseInstance(id: number): void {
    throw new Error('Method not implemented.');
  }

  destroyInstance(id: number): void {
    throw new Error('Method not implemented.');
  }

  execCallback(params: HippyMap): void {
    throw new Error('Method not implemented.');
  }

  destroyBridge(isReload: boolean): void {
    throw new Error('Method not implemented.');
  }

  destroy(): void {
    throw new Error('Method not implemented.');
  }

  callJavaScriptModule(mName: string, name: string, params: HippyMap): void {
    throw new Error('Method not implemented.');
  }

  getThirdPartyAdapter(): HippyThirdPartyAdapter {
    throw new Error('Method not implemented.');
  }


  loadInstance(name: string, id: number, params: HippyMap) {
    // TODO(hot):
    let map = new Map<string, HippyAny>()
    map.set("name", name)
    map.set("id", id)
    map.set("params", params)

    let binaryWriter = new BinaryWriter()
    let serializer = new Serializer(binaryWriter)
    serializer.writeHeader()
    serializer.writeValue(params)
    let buffer = binaryWriter.chunked().buffer
  }

  getGlobalConfigs(): string {
    // TODO(hot):
    let globalParams = new Map<string, HippyValue | Map<string, HippyValue> | Map<string, HippyValue | Map<string, HippyValue>>>()
    let dimensionMap = new Map<string, HippyValue>()

    globalParams.set("Dimensions", dimensionMap)

    let packageName = ""
    let versionName = ""
    let pageUrl = ""
    let nightMode = false
    let extraDataMap = new Map<string, HippyValue>()

    let platformParams = new Map<string, HippyValue | Map<string, HippyValue>>()
    platformParams.set("OS", "ohos")
    platformParams.set("PackageName", (packageName == null) ? "" : packageName)
    platformParams.set("VersionName", (versionName == null) ? "" : versionName)
    platformParams.set("APILevel", 0)
    platformParams.set("NightMode", nightMode)
    platformParams.set("SDKVersion", "")

    let localization = new Map<string, HippyValue>()
    localization.set("language", I18nUtil.getLanguage())
    localization.set("country", I18nUtil.getCountry())
    localization.set("direction", I18nUtil.getLayoutDirection())
    platformParams.set("Localization", localization)

    globalParams.set("Platform", platformParams)

    // TODO(etkamo): isSupportDev

    let host = new Map<string, HippyValue | Map<string, HippyValue>>()
    host.set("url", (pageUrl == null) ? "" : pageUrl)
    host.set("appName", (packageName == null) ? "" : packageName)
    host.set("appVersion", (versionName == null) ? "" : versionName)
    host.set("nightMode", nightMode)
    host.set("extra", extraDataMap)
    globalParams.set("HostConfig", host)

    return ArgumentUtils.objectToJson(globalParams)
  }
}
