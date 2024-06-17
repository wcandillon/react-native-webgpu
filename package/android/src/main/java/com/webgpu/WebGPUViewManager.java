package com.webgpu;

import android.graphics.Color;

import androidx.annotation.Nullable;

import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.annotations.ReactProp;

@ReactModule(name = WebGPUViewManager.NAME)
public class WebGPUViewManager extends WebGPUViewManagerSpec<WebGPUView> {

  public static final String NAME = "WebGPUView";

  @Override
  public String getName() {
    return NAME;
  }

  @Override
  public WebGPUView createViewInstance(ThemedReactContext context) {
    return new WebGPUView(context);
  }

  @Override
  @ReactProp(name = "color")
  public void setColor(WebGPUView view, @Nullable String color) {
    view.setBackgroundColor(Color.parseColor(color));
  }
}
