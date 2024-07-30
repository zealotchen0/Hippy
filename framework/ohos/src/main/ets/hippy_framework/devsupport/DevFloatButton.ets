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
import { ComponentContent, display } from '@kit.ArkUI';
import { LogUtils } from '../../support/utils/LogUtils';

class Params {
  text: string = "";
  onClickHandle: (event: ClickEvent) => void;

  constructor(text: string, onClickHandle: (event: ClickEvent) => void) {
    this.text = text;
    this.onClickHandle = onClickHandle;
  }
}

@Builder
function builderImage(params: Params) {
  Image($r('app.media.devtool'))
    .width(30)
    .height(30)
    .margin({top: 500, left: 300})
    .visibility(Visibility.Visible)
    .draggable(true)
    .onClick(params.onClickHandle)
    .onDragEnd((event: DragEvent, extraParams) => {
      let ret = event.getResult();
      if (ret == 0) {
        LogUtils.i("DevFloatButton", "Image::onDragEnd::enter ret == 0")
      } else {
        LogUtils.i("DevFloatButton", "Image::onDragEnd::enter ret != 0")
      }
    });
}

@Builder
function buildReload(params: Params) {
  Flex() {
    Text(params.text).fontSize(20).margin({ left: 20, top: 20 })
  }.width("85%").height("8%").align(Alignment.Center).backgroundColor(Color.White)
  .onClick(params.onClickHandle)
}

export class DevFloatButton {
  private mWidth: number;
  private floatButtonArray: ComponentContent<Params>[] = [];
  private uiContext: UIContext;

  constructor(context: Context, uiContext: UIContext, onClickHandler: (event: ClickEvent) => void) {
    this.mWidth = this.getDensityPixels(context) as number;

    this.uiContext = uiContext;
    this.create(onClickHandler);
  }

  private create(onClickHandler: (event: ClickEvent) => void) {
    let componentContent = new ComponentContent(
      this.uiContext, wrapBuilder<[Params]>(builderImage),
      new Params("Test", (event: ClickEvent) => {
        const action = this.uiContext.getPromptAction();
        let contentNode =
          new ComponentContent(
            this.uiContext, wrapBuilder(buildReload),
            new Params("Reload",
              (reloadClickEvent: ClickEvent) => {
                onClickHandler(reloadClickEvent);
                action.closeCustomDialog(contentNode);
              }
            )
          );
        action.openCustomDialog(contentNode, {
          autoCancel: true,
          alignment: DialogAlignment.Center,
        });
      })
    )
    this.floatButtonArray.push(componentContent);
    this.uiContext.getOverlayManager().addComponentContent(componentContent, 99);
  }

  public delete() {
    if (this.floatButtonArray.length > 0) {
      this.uiContext.getOverlayManager().removeComponentContent(this.floatButtonArray.pop());
    }
  }

  private getDensityPixels(context: Context): number {
    let displayClass: display.Display = display.getDefaultDisplaySync();
    return displayClass.densityPixels;
  }

}
