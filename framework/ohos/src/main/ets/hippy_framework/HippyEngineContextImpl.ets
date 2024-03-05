import { HippyMap } from '../support/common/HippyTypes';
import { TimeMonitor } from '../support/utils/TimeMonitor';
import { VfsManager } from '../vfs/VfsManager';
import { HippyBridgeManager } from './bridge/HippyBridgeManager';
import { DomManager } from './connector/DomManager';
import { JsDriver } from './connector/JsDriver';
import { HippyEngineContext } from './HippyEngineContext';
import { HippyGlobalConfigs } from './HippyGlobalConfigs';
import { HippyModuleManager } from './modules/HippyModuleManager';
import { HashMap } from '@kit.ArkTS';
import { HippyLibrary } from '../hippy_library/HippyLibrary';
import { HippyResourceLoader } from './adapter/http/HippyResourceLoader';
import { DefaultProcessor } from '../vfs/DefaultProcessor';

export class HippyEngineContextImpl implements HippyEngineContext {
  private mEngineInitParams: EngineInitParams;
  private mGlobalConfigs: HippyGlobalConfigs;
  private mDomManager: DomManager;
  private mMonitor: TimeMonitor;
  private mModuleManager: HippyModuleManager;
  private mBridgeManager: HippyBridgeManager;
  public mCoreBundleLoader: HippyBundleLoader;

  // TODO
  // private mDevtoolsManager: DevtoolsManager;


  public constructor(
    params: EngineInitParams,
    domManager: DomManager,
    globalConfigs: HippyGlobalConfigs,
    monitor: TimeMonitor,
    coreBundleLoader: HippyBundleLoader
  ) {
    this.mEngineInitParams = params;
    this.mGlobalConfigs = globalConfigs;
    this.mDomManager = domManager;
    this.mMonitor = monitor;
    this.moduleManager = new HippyModuleManager(this);

    // provider挂载
    let customRenderViewCreatorMap: Map<string, HRRenderViewCreator> | null = null
    if (this.providers) {
      customRenderViewCreatorMap = new Map()
      for (let provider of this.providers) {
        let map = provider.getCustomRenderViewCreatorMap()
        if (map) {
          map.forEach((value, key) => {
            customRenderViewCreatorMap?.set(key, value)
          })
        }
      }
    }
    // Render注册
    this.nativeRenderProvider = new NativeRenderProvider(this.libHippy, customRenderViewCreatorMap)
    this.nativeRenderProvider.getNativeRenderImpl().setFrameworkProxy(this)
    this.nativeRenderer = new NativeRenderer(this.libHippy, this.nativeRenderProvider)
    this.nativeRenderer.attachToDom(this.domMgr.instanceId)
    // DomManager挂载
    this.domMgr.attachToRenderer(this.nativeRenderer.instanceId)
    let rootNodeId = this.getNativeRenderContext().getRootId()
    this.domMgr.createRootNode(rootNodeId, PixelUtil.getDensity())
    this.domMgr.attachToRoot(rootNodeId)
    // jsDriver注册
    this.jsDriver = new JsDriver(this.libHippy, this.moduleManager)

    // BridgeManager注册
    this.mBridgeManager = new HippyBridgeManagerImpl(
      this,
      coreBundleLoader,
      params.enableV8Serialization,
      params.debugMode,
      params.debugServerHost,
      params.groupId,
      params.thirdPartyAdapter,
      params.v8InitParams,
      this.mJsDriver);
  }

  getComponentName(): string {
    throw new Error('Method not implemented.');
  }

  getVfsManager(): VfsManager {
    return this.vfsManager
  }

  getJsDriver(): JsDriver {
    return this.jsDriver;
  }

  getMonitor(): TimeMonitor {
    return this.mMonitor;
  }

  getGlobalConfigs(): HippyGlobalConfigs {
    return this.mGlobalConfigs;
  }

  getModuleManager(): HippyModuleManager {
    return this.moduleManager;
  }

  getBridgeManager(): HippyBridgeManager {
    return this.mBridgeManager;
  }

  getNativeParams() : HashMap<string, Object> {
    return this.nativeParams
  }
}
