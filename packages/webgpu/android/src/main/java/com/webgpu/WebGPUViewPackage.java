
package com.webgpu;

import com.facebook.react.TurboReactPackage;
import com.facebook.react.bridge.NativeModule;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.module.model.ReactModuleInfo;
import com.facebook.react.module.model.ReactModuleInfoProvider;
import com.facebook.react.uimanager.ViewManager;

import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class WebGPUViewPackage extends TurboReactPackage {
  @Override
  public NativeModule getModule(String name, ReactApplicationContext reactContext) {
    if (name.equals(WebGPUModule.NAME)) {
      return new WebGPUModule(reactContext);
    }
    return null;
  }

  @Override
  public ReactModuleInfoProvider getReactModuleInfoProvider() {
    return () -> {
      Map<String, ReactModuleInfo> map = new HashMap<>();
      map.put(
        WebGPUModule.NAME,
        new ReactModuleInfo(
          WebGPUModule.NAME,
          WebGPUModule.class.getName(),
          false,
          false,
          false,
          false,
          true // isTurboModule
        )
      );
      return map;
    };
  }

  @Override
  public List<ViewManager> createViewManagers(ReactApplicationContext reactContext) {
    return Collections.singletonList(new WebGPUViewManager());
  }
}
