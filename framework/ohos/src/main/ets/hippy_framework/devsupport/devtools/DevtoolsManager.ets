/* Tencent is pleased to support the open source community by making Hippy available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

export class DevtoolsManager {

    private mDebugMode = false;
    private mId = -1;

    constructor(debugMode: boolean) {
      this.mDebugMode = debugMode;
    }

    public getId(): number {
        return this.mId;
    }

    public create(dataDir: string, wsUrl: string) {
        if (this.mDebugMode) {
            this.mId = onCreateDevtools(dataDir, wsUrl);
        }
    }

    public destroy(isReload: boolean) {
        if (this.mDebugMode) {
            onDestroyDevtools(this.getId(), isReload);
        }
    }

    public bind(driver: Connector, Connector dom, Connector render) {
        if (this.mDebugMode) {
            onBindDevtools(this.getId(), driver.getInstanceId(), dom.getInstanceId(), render.getInstanceId());
        }
    }

    public attachToRoot(@NonNull View root) {
        if (this.mDebugMode) {
            onAttachToRoot(this.getId(), root.getId());
        }
    }
}
