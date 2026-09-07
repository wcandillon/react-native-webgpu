package com.webgpu;

import androidx.annotation.NonNull;

import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.annotations.ReactProp;

@ReactModule(name = WebGPUViewManager.NAME)
public class WebGPUViewManager extends WebGPUViewManagerSpec<WebGPUView> {

  public static final String NAME = "WebGPUView";

  @NonNull
  @Override
  public String getName() {
    return NAME;
  }

  @NonNull
  @Override
  public WebGPUView createViewInstance(@NonNull ThemedReactContext context) {
    return new WebGPUView(context);
  }

  @Override
  public void onDropViewInstance(@NonNull WebGPUView view) {
    super.onDropViewInstance(view);
    view.destroy();
  }

  @Override
  @ReactProp(name = "transparent")
  public void setTransparent(WebGPUView view, boolean value) {
    view.setTransparent(value);
  }

  @Override
  @ReactProp(name = "androidView")
  public void setAndroidView(WebGPUView view, String value) {
    view.setAndroidView(value);
  }

  @Override
  @ReactProp(name = "androidZOrderOnTop")
  public void setAndroidZOrderOnTop(WebGPUView view, boolean value) {
    view.setZOrderOnTop(value);
  }

  @Override
  @ReactProp(name = "androidTranslucent")
  public void setAndroidTranslucent(WebGPUView view, boolean value) {
    view.setTranslucent(value);
  }

  @Override
  protected void onAfterUpdateTransaction(@NonNull WebGPUView view) {
    super.onAfterUpdateTransaction(view);
    view.updateView();
  }

  @Override
  @ReactProp(name = "contextId")
  public void setContextId(WebGPUView view, int value) {
    view.setContextId(value);
  }
}
