package com.webgpu;

import androidx.annotation.NonNull;

import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.annotations.ReactProp;

@ReactModule(name = WebGPUViewManager.NAME)
public class WebGPUViewManager extends WebGPUViewManagerSpec<WebGPUBaseView> {

  public static final String NAME = "WebGPUView";

  @NonNull
  @Override
  public String getName() {
    return NAME;
  }

  @Override
  public WebGPUBaseView createViewInstance(ThemedReactContext context) {
    return new WebGPUTextureView(context);
  }

  @Override
  @ReactProp(name = "contextId")
  public void setContextId(WebGPUBaseView view, int value) {
    view.setContextId(value);
  }
}
