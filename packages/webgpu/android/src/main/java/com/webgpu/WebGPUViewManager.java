package com.webgpu;

import android.util.Log;

import androidx.annotation.NonNull;

import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.annotations.ReactProp;

@ReactModule(name = WebGPUViewManager.NAME)
public class WebGPUViewManager extends WebGPUViewManagerSpec<WebGPUBaseView> {

  public static final String NAME = "WebGPUView";

  private boolean transparent = false;

  @NonNull
  @Override
  public String getName() {
    return NAME;
  }

  @Override
  public WebGPUBaseView createViewInstance(ThemedReactContext context) {
    if (transparent) {
      return new WebGPUTextureView(context);
    }
    return new WebGPUSurfaceView(context);
  }

  @Override
  @ReactProp(name = "androidTransparency")
  public void setAndroidTransparency(WebGPUBaseView view, boolean value) {
    transparent = value;
  }

  @Override
  @ReactProp(name = "contextId")
  public void setContextId(WebGPUBaseView view, int value) {
    view.setContextId(value);
  }
}
