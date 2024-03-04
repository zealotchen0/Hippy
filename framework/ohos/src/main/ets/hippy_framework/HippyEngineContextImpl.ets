import { VfsManager } from '../vfs/VfsManager';
import { HippyBridgeManager } from './bridge/HippyBridgeManager';
import { DomManager } from './connector/DomManager';
import { JsDriver } from './connector/JsDriver';
import { HippyEngineContext } from './HippyEngineContext';
import { HippyModuleManager } from './modules/HippyModuleManager';

public

class HippyEngineContextImpl implements HippyEngineContext {
  private mComponentName: string;
  private mModuleManager: HippyModuleManager;
  private mBridgeManager: HippyBridgeManager;
  private mRenderer: RenderConnector;
  private mDomManager: DomManager;
  private mJsDriver: JsDriver;
  private mVfsManager: VfsManager;

  // TODO
  // private mDevtoolsManager: DevtoolsManager;

  // volatile CopyOnWriteArrayList<HippyEngineLifecycleEventListener> mEngineLifecycleEventListeners;

  public constructor(domManager: DomManager) {
    // this.mVfsManager = (mProcessors != null) ? new VfsManager(mProcessors) : new VfsManager();
    // this.mVfsManager.setId(onCreateVfs(mVfsManager));
    DefaultProcessor
    defaultProcessor = new DefaultProcessor(new HippyResourceLoader(this));
    PerformanceProcessor
    performanceProcessor = new PerformanceProcessor(this);
    mVfsManager.addProcessorAtFirst(performanceProcessor);
    mVfsManager.addProcessorAtLast(defaultProcessor);
    if (mDebugMode) {
      mDevtoolsManager = new DevtoolsManager(true);
      String
      localCachePath = getGlobalConfigs().getContext().getCacheDir()
        .getAbsolutePath();
      mDevtoolsManager.create(localCachePath,
        getDevSupportManager().createDebugUrl(mServerHost));
      //mVfsManager.addProcessorAtFirst(new DevtoolsProcessor(mDevtoolsManager.getId()));
    }

    mModuleManager = new HippyModuleManagerImpl(this, mProviders,
      enableV8Serialization);
    mJsDriver = new JsDriver();
    mBridgeManager = new HippyBridgeManagerImpl(this, mCoreBundleLoader,
      getBridgeType(), enableV8Serialization, mDebugMode,
      mServerHost, mGroupId, mThirdPartyAdapter, v8InitParams, mJsDriver);
    mDomManager = (domManager != null) ? domManager : new DomManager();
    mRenderer = createRenderer(RenderConnector.NATIVE_RENDERER);
    mDomManager.attachToRenderer(mRenderer);
    mRenderer.attachToDom(mDomManager);
    mRenderer.setFrameworkProxy(HippyEngineManagerImpl.this);
  }
}
