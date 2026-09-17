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
  @ReactProp(name = "opaque", defaultBoolean = true)
  public void setOpaque(WebGPUView view, boolean value) {
    view.setOpaque(value);
  }

  @Override
  @ReactProp(name = "androidSurfaceType")
  public void setAndroidSurfaceType(WebGPUView view, String value) {
    view.setSurfaceType(value);
  }

  @Override
  @ReactProp(name = "androidZOrderOnTop")
  public void setAndroidZOrderOnTop(WebGPUView view, boolean value) {
    view.setZOrderOnTop(value);
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
