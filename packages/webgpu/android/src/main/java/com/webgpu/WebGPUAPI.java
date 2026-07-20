package com.webgpu;

import android.view.Surface;
import android.view.View;

import com.facebook.proguard.annotations.DoNotStrip;

public interface WebGPUAPI {

  void surfaceCreated(
    View source,
    Surface surface
  );

  void surfaceChanged(
    View source,
    Surface surface
  );

  void surfaceDestroyed(View source);

  void surfaceOffscreen(View source);
}
