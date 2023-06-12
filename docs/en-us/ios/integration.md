# iOS Integration

> Note: the following documents assume that you have some experience in iOS development.

This tutorial shows how to integrate Hippy into an iOS project.

---

# Integrate with Cocoapods

1. install [cocoapods](https://cocoapods.org/), Hippy iOS SDK [version query](https://cocoapods.org/pods/hippy)

2. Create a podfile file under user-defined project directory with the following text

    ```text
    # keep podfile directory structure
    install! "cocoapods", :preserve_pod_file_structure => true
    platform :ios, '11.0'
    # replace TargetName with user's project name
    target TargetName do
        # use latest version of Hippy
        pod 'hippy'
        # if you want to assign Hippy version, say, 2.0.0
        #pod 'hippy', '2.0.0'
    end
    ```

    > Tip: When integrating versions `2.13.0` to `2.16.x`, if you access hippy in the form of a static link library, you need to set the `force_load` compilation parameter to load all symbols of hippy.
    >
    > `2.17.x` and above versions do not need to be configured.
    >
    > There are many ways to set `force_load`, you can choose any of the following configurations, and adjust according to the actual situation:
    >
    > * Directly set `*-force_load "${PODS_CONFIGURATION_BUILD_DIR}/hippy/libhippy.a"*` in the configuration of `Other Linker Flags` in the Build Settings of the target corresponding to the main project.
    >
    > * Add `post_install hook` in the Podfile configuration file of the App project, and add `force_load` to xcconfig by yourself.
    >

3. Execute in the command line

    ```text
    pod install
    ```

4. open the project by the `.xcworkspace` file generated by cocoapods.

# Write Code to Start Debugging or Load Business Code

Hippy provides a code-splitting loading interface and a non-code-splitting loading interface. All service packages are carried through `HippyRootView`. Creating a business is to create a `RootView`.

## Using the code-splitting load interface

``` objectivec
/** This method is suitable for the following scenarios.
 * Prepare the JS environment and load package 1 before the business is started, and load package 2 when the business is started to reduce package loading time
 * We suggest package 1 as the base package, which is not related to the business and contains only some common base components, common to all businesses
 * Package 2 is loaded as business code
*/

// Load package 1 address first to create execution environment
//commonBundlePath being package 1's path
NSURL * commonBundlePath = getCommonBundlePath();
HippyBridge *bridge = [[HippyBridge alloc] initWithBundleURL: commonBundlePath
                                                moduleProvider: nil
                                                launchOptions: nil];

// Create rootview by bridge and package2 address
- (instancetype)initWithBridge:(HippyBridge *)bridge  
    businessURL:(NSURL *)businessURL // business package address
    moduleName:(NSString *)moduleName // business package startup function name
    initialProperties:(NSDictionary *)initialProperties // initialize parameters
    shareOptions:(NSDictionary *)shareOptions // configuration parameters (advanced)
    isDebugMode:(BOOL)isDebugMode // whether debug mode
    delegate:(id<HippyRootViewDelegate>)delegate // rootview load callback

```

## Using the non-code-splitting load interface

``` objectivec
Subpackage loading interface - (instancetype)initWithBundleURL:(NSURL *)bundleURL // package address
    moduleName:(NSString *)moduleName // business package startup function name
    initialProperties:(NSDictionary *)initialProperties // initialize parameters
    shareOptions:(NSDictionary *)shareOptions // configuration parameters (advanced)
    isDebugMode:(BOOL)isDebugMode // whether debug mode
    delegate:(id <HippyRootViewDelegate>)delegate // rootview load callback
```

!> Regardless of whether rootview is initialized with or without code-splitting, if **isdebugmode** is YES, all parameters will be ignored and the test dataset is loaded directly using the npm local service. Using code-splitting loading can be combined with a series of strategies, such as pre-loading bridge, global single bridge, and so on, to optimize page opening speed.