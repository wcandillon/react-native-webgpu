#pragma once

namespace rnwgpu {

// GPUCanvasConfiguration.colorSpace (PredefinedColorSpace): how the canvas's
// values are interpreted when it is composited.
enum class CanvasColorSpace { SRGB, DisplayP3 };

// GPUCanvasConfiguration.toneMapping.mode (GPUCanvasToneMappingMode): whether
// values above 1.0 on a float canvas are clamped to the display's SDR range
// ("standard", the default) or reach its extended range ("extended").
enum class CanvasToneMappingMode { Standard, Extended };

// What a canvas's layer is tagged with on Apple platforms: the colorSpace and
// toneMapping props of the Canvas, read when it mounts (see
// apple/MetalLayerColorSpace.h).
struct CanvasLayerColorSpace {
  CanvasColorSpace colorSpace = CanvasColorSpace::SRGB;
  CanvasToneMappingMode toneMappingMode = CanvasToneMappingMode::Standard;

  bool operator==(const CanvasLayerColorSpace &other) const {
    return colorSpace == other.colorSpace &&
           toneMappingMode == other.toneMappingMode;
  }
  bool operator!=(const CanvasLayerColorSpace &other) const {
    return !(*this == other);
  }
};

} // namespace rnwgpu
