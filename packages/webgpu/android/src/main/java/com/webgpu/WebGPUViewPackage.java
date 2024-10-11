
package com.webgpu;

import com.facebook.react.ReactPackage;
import com.facebook.react.bridge.NativeModule;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.uimanager.ViewManager;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class WebGPUViewPackage implements ReactPackage {
  @Override
  public List<ViewManager> createViewManagers(ReactApplicationContext reactContext) {
    List<ViewManager> viewManagers = new ArrayList<>();
    viewManagers.add(new WebGPUViewManager());
    return viewManagers;
  }


  @Override
  public List<NativeModule> createNativeModules(ReactApplicationContext reactContext) {
      return List.of(new WebGPUModule(reactContext));
  }
}
