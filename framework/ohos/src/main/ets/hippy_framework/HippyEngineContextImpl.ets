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
  DefaultProcessor defaultProcessor = new DefaultProcessor(new HippyResourceLoader(this));
  PerformanceProcessor performanceProcessor = new PerformanceProcessor(this);
  mVfsManager.addProcessorAtFirst(performanceProcessor);
  mVfsManager.addProcessorAtLast(defaultProcessor);
  if (mDebugMode) {
                mDevtoolsManager = new DevtoolsManager(true);
                String localCachePath = getGlobalConfigs().getContext().getCacheDir()
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

  mRenderer.setFrameworkProxy(HippyEngineManagerImpl.this)
  ;
  List<Class<?>> controllers = null;
  for (HippyAPIProvider provider : mProviders) {
  if (provider != null && provider.getControllers()

  != null) {
  if (controllers == null) {
  controllers = new ArrayList < > ();
}

controllers.addAll(provider.getControllers());
}
}
mRenderer.init(controllers, mRootView);
}

private
RenderConnector
createRenderer
(
String
rendererName
)
{
            RenderConnector renderer = null;
            try {
                Class rendererClass = Class.forName("com.openhippy.connector." + rendererName);
                renderer = (RenderConnector) (rendererClass.newInstance());
            }
catch
(
Throwable
e
)
{
  e.printStackTrace();
}
if (renderer == null) {
  throw new RuntimeException(
    "Serious error: Failed to create renderer instance!");
}
return renderer;
}

@
Override
public
void
onRuntimeInitialized() {
  mJsDriver.attachToDom(mDomManager);
  if (mRootView != null && (mDebugMode || BuildConfig.DEBUG)) {
    mDomManager.createRoot(mRootView, PixelUtil.getDensity());
    mDomManager.attachToRoot(mRootView);
    mJsDriver.attachToRoot(mRootView);
    if (mDevtoolsManager != null) {
      mDevtoolsManager.attachToRoot(mRootView);
    }
    mRenderer.onRuntimeInitialized(mRootView.getId());
  }
  if (mDevtoolsManager != null) {
    mDevtoolsManager.bind(mJsDriver, mDomManager, mRenderer);
  }
}

@
Override
@
NonNull
public
JsDriver
getJsDriver() {
  return mJsDriver;
}

@
NonNull
DomManager
getDomManager() {
  return mDomManager;
}

@
NonNull
RenderConnector
getRenderer() {
  return mRenderer;
}

public
void
setComponentName
(
String
componentName
)
{
  mComponentName = componentName;
}

@
Override
public
String
getComponentName() {
  return mComponentName;
}

@
Override
@
Nullable
public
HashMap
<
String
,
Object
>
getNativeParams() {
  return mNativeParams;
}

@
Override
public
HippyGlobalConfigs
getGlobalConfigs() {
  return mGlobalConfigs;
}

@
Override
@
NonNull
public
TimeMonitor
getMonitor() {
  return mMonitor;
}

@
Override
@
NonNull
public
VfsManager
getVfsManager() {
  return mVfsManager;
}

@
Override
public
HippyModuleManager
getModuleManager() {
  return mModuleManager;
}

@
Override
public
DevSupportManager
getDevSupportManager() {
  return mDevSupportManager;
}

@
Override
public
DevtoolsManager
getDevtoolsManager() {
  return mDevtoolsManager;
}

@
Override
public
ThreadExecutor
getThreadExecutor() {
  return HippyEngineManagerImpl.this.getThreadExecutor();
}

@
Override
public
HippyBridgeManager
getBridgeManager() {
  return mBridgeManager;
}

@
Override
public
void
addEngineLifecycleEventListener
(
HippyEngineLifecycleEventListener
listener
)
{
  if (mEngineLifecycleEventListeners == null) {
    synchronized(HippyEngineContextImpl.class) {
      if (mEngineLifecycleEventListeners == null) {
        mEngineLifecycleEventListeners = new CopyOnWriteArrayList < > ();
      }
    }
  }
  mEngineLifecycleEventListeners.add(listener);
}

@
Override
public
void
removeEngineLifecycleEventListener
(
HippyEngineLifecycleEventListener
listener
)
{
  if (mEngineLifecycleEventListeners != null) {
    mEngineLifecycleEventListeners.remove(listener);
  }
}

@
Override
public
void
onInstanceLoad
(
int
rootId
)
{

}

@
Override
public
void
onInstanceResume() {
  mRenderer.onResume();
  if (getBridgeManager() != null && mRootView != null) {
    getBridgeManager().resumeInstance(mRootView.getId());
  }
}

@
Override
public
void
onInstancePause() {
  mRenderer.onPause();
  if (getBridgeManager() != null && mRootView != null) {
    getBridgeManager().pauseInstance(mRootView.getId());
  }
}

@
Override
public
void
onInstanceDestroy
(
int
rootId
)
{
  mDomManager.releaseRoot(rootId);
  mDomManager.destroyRoot(rootId);
  mRenderer.destroyRoot(rootId);
  if (mDestroyModuleListeners != null) {
                Callback<Boolean> callback = mDestroyModuleListeners.get(rootId);
                if (callback != null) {
                    callback.callback(true, null);
                }
  mDestroyModuleListeners.remove(rootId);
}
}

@
Override
public
void
handleException
(
Throwable
throwable
)
{
  if (mDebugMode && mDevSupportManager != null) {
    mDevSupportManager.handleException(throwable);
  } else {
    if (throwable instanceof HippyJsException) {
      if (mGlobalConfigs != null) {
                        mGlobalConfigs.getExceptionHandler()
                                .handleJsException((HippyJsException) throwable);
                    }
      if (mModuleListener != null) {
        mModuleListener
        .
        onJsException
        (
        (
        HippyJsException
        )
        throwable
        )
        ;
      }
    } else if (mGlobalConfigs != null) {
      mGlobalConfigs.getExceptionHandler()
        .handleNativeException(new RuntimeException(throwable), true);
    }
  }
}

@
Override
public
int
getEngineId() {
  return HippyEngineManagerImpl.this.getEngineId();
}

@
Override
public
int
getDomManagerId() {
  return mDomManager.getInstanceId();
}

@
Override
public
int
getVfsId() {
  return mVfsManager.getId();
}

public
int
getDevtoolsId() {
  return mDevtoolsManager != null ? mDevtoolsManager.getId() : -1;
}

@
Override
public
ViewGroup
getRootView() {
  return mRootView;
}

@
Override
public
View
getRootView
(
int
rootId
)
{
  return (mRenderer instanceof NativeRenderConnector)
    ? ((NativeRenderConnector) mRenderer
  )
  .
  getRootView
  (
  rootId
  )
  :
  null
  ;
}

@
Nullable
public
View
findViewById
(
int
nodeId
)
{
  return (mRenderer instanceof NativeRenderConnector && mRootView != null)
    ? ((NativeRenderConnector) mRenderer
  )
  .
  findViewById(mRootView.getId(), nodeId)
  :
  null;
}

@
Nullable
public
View
createRootView
(
@
NonNull
Context
context
)
{
            View rootView = mRenderer.createRootView(context);
            if (rootView != null) {
                mDomManager.createRoot(rootView, PixelUtil.getDensity());
                mDomManager.attachToRoot(rootView);
                mJsDriver.attachToRoot(rootView);
                if (mDevtoolsManager != null) {
                    mDevtoolsManager.attachToRoot(rootView);
                }
}
return rootView;
}

@
Override
public
void
onBridgeDestroyed
(
final
boolean
isReload
,
Throwable
e
)
{
            UIThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    if (isReload) {
                        restartEngineInBackground(true);
                    } else {
                        onDestroyEngine();
                    }
}
}
);
}

@
Override
public
void
onLoadModuleCompleted
(
ModuleLoadStatus
statusCode
,
@
Nullable
String
msg
)
{
            notifyModuleLoaded(statusCode, msg);
            MonitorGroup monitorGroup = mEngineContext.getMonitor()
                    .endGroup(MonitorGroupType.RUN_JS_BUNDLE);
            if (monitorGroup != null) {
                mGlobalConfigs.getEngineMonitorAdapter()
                        .onLoadModuleCompleted(statusCode, mEngineContext.getComponentName(),
                                monitorGroup);
            }
}

@
Override
public
void
onLoadInstanceCompleted
(
long
result
,
String
reason
)
{
  mEngineContext.getMonitor().startPoint(MonitorGroupType.LOAD_INSTANCE,
    TimeMonitor.MONITOR_POINT_FIRST_FRAME);
}

public
void
destroyBridge
(
boolean
isReload
)
{
  mBridgeManager.destroyBridge(isReload);
}

public
void
destroy
(
boolean
isReload
)
{
  if (mDevtoolsManager != null) {
    mDevtoolsManager.destroy(isReload);
  }
  mRenderer.destroy();
  if (!isReload) {
    mDomManager.destroy();
  }
  mModuleManager.destroy();
  mVfsManager.destroy();
  onDestroyVfs(mVfsManager.getId());
  if (mEngineLifecycleEventListeners != null) {
    mEngineLifecycleEventListeners.clear();
  }
}
}
