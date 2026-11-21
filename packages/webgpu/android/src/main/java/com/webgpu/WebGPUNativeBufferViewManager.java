package com.webgpu;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.facebook.react.common.MapBuilder;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.annotations.ReactProp;

import java.util.Map;

@ReactModule(name = WebGPUNativeBufferViewManager.NAME)
public class WebGPUNativeBufferViewManager
    extends WebGPUNativeBufferViewManagerSpec<WebGPUNativeBufferView> {

  public static final String NAME = "WebGPUNativeBufferView";

  @NonNull
  @Override
  public String getName() {
    return NAME;
  }

  @NonNull
  @Override
  public WebGPUNativeBufferView createViewInstance(@NonNull ThemedReactContext context) {
    return new WebGPUNativeBufferView(context);
  }

  @Override
  public void onDropViewInstance(@NonNull WebGPUNativeBufferView view) {
    super.onDropViewInstance(view);
    view.destroy();
  }

  @Override
  @ReactProp(name = "viewId")
  public void setViewId(WebGPUNativeBufferView view, int value) {
    view.setViewId(value);
  }

  @Override
  @ReactProp(name = "resizeMode")
  public void setResizeMode(WebGPUNativeBufferView view, @Nullable String value) {
    view.setResizeMode(value);
  }

  @Nullable
  @Override
  public Map<String, Object> getExportedCustomDirectEventTypeConstants() {
    return MapBuilder.<String, Object>of(
        WebGPUNativeBufferView.PIXEL_SIZE_CHANGE_EVENT,
        MapBuilder.of("registrationName", "onPixelSizeChange"));
  }
}
