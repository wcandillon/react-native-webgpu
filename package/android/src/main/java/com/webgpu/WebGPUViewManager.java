package com.webgpu;

import androidx.annotation.Nullable;

import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.annotations.ReactProp;

@ReactModule(name = WebGPUViewManager.NAME)
public class WebGPUViewManager extends com.webgpu.WgpuViewManagerSpec<WebGPUView> {

  public static final String NAME = "WebGPUView";

  @Override
  public String getName() {
    return NAME;
  }

  @Override
  public WebGPUView createViewInstance(ThemedReactContext context) {
    return new WebGPUView(context);
  }

//  @Override
  @ReactProp(name = "contextId")
  public void setContextId(WebGPUView view, @Nullable Integer contextId) {
    view.setContextId(contextId);
  }

  @Override
  public void setContextId(WebGPUView view, int value) {
    view.setContextId(value);
  }
}
