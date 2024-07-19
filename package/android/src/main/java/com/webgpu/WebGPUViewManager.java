package com.webgpu;

import android.graphics.Color;

import androidx.annotation.Nullable;

import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.annotations.ReactProp;

@ReactModule(name = WebGPUViewManager.NAME)
public class WebGPUViewManager extends com.webgpu.WebGPUViewManagerSpec<WebGPUView> {

  public static final String NAME = "WebGPUView";

  public String getName() {
    return NAME;
  }

  public WebGPUView createViewInstance(ThemedReactContext context) {
    return new WebGPUView(context);
  }

  @ReactProp(name = "color")
  public void setColor(WebGPUView view, @Nullable String color) {
    view.setBackgroundColor(Color.parseColor(color));
  }
}
