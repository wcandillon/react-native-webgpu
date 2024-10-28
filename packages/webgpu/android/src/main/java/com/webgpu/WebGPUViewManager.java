package com.webgpu;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

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
  @ReactProp(name = "androidView")
  public void setAndroidView(WebGPUView view, @Nullable String value) {
    view.setView(value == null ? WebGPUView.SURFACE_VIEW : value);
  }

  @Override
  @ReactProp(name = "contextId")
  public void setContextId(WebGPUView view, int value) {
    view.setContextId(value);
  }
}
