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
import { ComponentContent, display, window } from '@kit.ArkUI';
import { LogUtils } from '../../support/utils/LogUtils';

class Params {
  text: string = "";
  onClickHandle: (event: ClickEvent) => void;
  radius: number;
  btnPosition: Position | null = { x: 0, y: 0 };
  onTouchHandle: (event: TouchEvent) => void;
  runColor = 0x317aff;
  stopColor = 0xddd9d8;
  btnColor = this.runColor;

  constructor(text: string, btnPosition: Position | null,
    radius: number,
    onClickHandle: (event: ClickEvent) => void,
    onTouchHandle: (event: TouchEvent) => void
  ) {
    this.text = text;
    this.radius = radius;
    this.onClickHandle = onClickHandle;
    this.btnPosition = btnPosition;
    this.onTouchHandle = onTouchHandle;
  }
}

@Builder
function buildeFloatBtn(params: Params) {
  Row() {
    Image(null).width(params.radius).backgroundColor(params.btnColor).fillColor(params.btnColor)
  }
  .width(params.radius * 2)
  .height(params.radius * 2)
  .justifyContent(FlexAlign.Center)
  .borderRadius(params.radius)
  .backgroundColor(params.btnColor)
  .position(params.btnPosition)
  .onClick(params.onClickHandle)
  .onTouch(params.onTouchHandle)
}

@Builder
function buildReload(params: Params) {
  Flex() {
    Text(params.text).fontSize(20).margin({ left: 20, top: 20 })
  }
  .width("88%")
  .height("8%")
  .align(Alignment.Center)
  .backgroundColor(Color.White)
  .onClick(params.onClickHandle)
}

export class DevFloatButton {
  private mWidth: number;
  private floatButtonArray: ComponentContent<Params>[] = [];
  private uiContext?: UIContext;
  private btnPosition: Position = {
    x: 0,
    y: 0,
  };
  private statusHeight: number = 0 //状态栏高度
  private bottomAvoidAreaHeight: number = 0 //手机底部规避区域高度
  private startLeft: number = 0 //开始移动那一刻悬浮按钮距离窗口左边距离
  private startTop: number = 0 //开始移动那一刻悬浮按钮距离窗口顶部距离
  private startX: number = 0 //开始移动触摸点x坐标，相对窗口左上角
  private startY: number = 0 //开始移动触摸点y坐标，相对窗口左上角
  private radius: number = 18 //悬浮按钮半径，单位vp
  private winWidth: number = 0 //窗口宽度
  private winHeight: number = 0 //窗口高度

  constructor(context: Context, onClickHandle: (event: ClickEvent) => void) {
    this.mWidth = this.getDensityPixels(context) as number;
    this.getWindowInfo(onClickHandle);
  }

  getWindowInfo(onClickHandle: (event: ClickEvent) => void) {
    window.getLastWindow(getContext(this), (err, windowClass) => {
      if (!err.code) {
        this.uiContext = windowClass.getUIContext();
        //状态栏高度
        this.statusHeight =
          px2vp(windowClass.getWindowAvoidArea(window.AvoidAreaType.TYPE_SYSTEM).topRect.height)
        //获取手机底部规避区域高度
        this.bottomAvoidAreaHeight =
          px2vp(windowClass.getWindowAvoidArea(window.AvoidAreaType.TYPE_NAVIGATION_INDICATOR)
            .bottomRect
            .height)
        //获取窗口宽高
        let windowProperties = windowClass.getWindowProperties()
        this.winWidth = px2vp(windowProperties.windowRect.width)
        this.winHeight = px2vp(windowProperties.windowRect.height)
        //设置初始位置
        this.btnPosition.x = this.winWidth * 0
        this.btnPosition.y = this.winHeight * 0.1
        this.create(onClickHandle);
      }
    });
  }

  private updateCurrentBtnPosition(params: Params, event: TouchEvent) {
    let touch = event.touches[0];
    //计算悬浮球与左边距离（x坐标）， 当前悬浮球距离左边=开始位置(x轴)+（当前触摸点x坐标-开始移动触摸点x坐标）
    let curLeft = this.startLeft + (touch.windowX - this.startX);
    //限制悬浮球不能移除屏幕左边
    curLeft = Math.max(0, curLeft)
    //限制悬浮球不能移除屏幕右边
    this.btnPosition.x = Math.min(this.winWidth - 2 * this.radius, curLeft);

    //计算悬浮球与顶部距离（y坐标）， 当前悬浮球距离顶部=开始位置(y轴)+（当前触摸点y坐标-开始移动触摸点y坐标）
    let curTop = this.startTop + (touch.windowY - this.startY);
    //限制悬浮球不能移除屏幕上边
    curTop = Math.max(0, curTop);
    //限制悬浮球不能移除屏幕下边
    this.btnPosition.y = Math.min(this.winHeight - 2 * this.radius - this.bottomAvoidAreaHeight -
    this.statusHeight, curTop);
    this.floatButtonArray.forEach((btn: ComponentContent<Params>) => {
      btn.update(params);
    });
  }

  private create(onClickHandle: (event: ClickEvent) => void) {
    let params = new Params("Test", this.btnPosition, this.radius, (event: ClickEvent) => {
      const action = this.uiContext?.getPromptAction();
      if (this.uiContext != null) {
        let contentNode = new ComponentContent(this.uiContext, wrapBuilder(buildReload),
          new Params("Reload", null, this.radius, (reloaClickEvent: ClickEvent) => {
            onClickHandle(reloaClickEvent);
            action?.closeCustomDialog(contentNode);
          }, (event: TouchEvent) => {
          }));
        action?.openCustomDialog(contentNode, {
          autoCancel: true,
          alignment: DialogAlignment.Center,
        });
      }
    },
      (event: TouchEvent) => {
        //手指按下记录初始触摸点坐标、悬浮按钮位置
        if (event.type === TouchType.Down) {
          this.startX = event.touches[0].windowX;
          this.startY = event.touches[0].windowY;
          this.startLeft = this.btnPosition.x as number;
          this.startTop = this.btnPosition.y as number;
        } else if (event.type === TouchType.Move) {
          this.updateCurrentBtnPosition(params, event);
        }
      }
    );
    if (this.uiContext != null) {
      let componentContent =
        new ComponentContent(this.uiContext, wrapBuilder<[Params]>(buildeFloatBtn), params);
      this.floatButtonArray.push(componentContent);
      this.uiContext?.getOverlayManager().addComponentContent(componentContent, 99);
    }
  }

  public delete() {
    if (this.floatButtonArray.length > 0) {
      this.uiContext?.getOverlayManager().removeComponentContent(this.floatButtonArray.pop());
    }
  }

  private getDensityPixels(context: Context): number {
    let displayClass: display.Display = display.getDefaultDisplaySync();
    return displayClass.densityPixels;
  }

}
