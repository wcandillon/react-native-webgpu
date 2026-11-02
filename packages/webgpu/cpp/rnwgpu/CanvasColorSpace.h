#pragma once

namespace rnwgpu {

// GPUCanvasConfiguration.colorSpace (PredefinedColorSpace): how the canvas's
// values are interpreted when it is composited.
enum class CanvasColorSpace { SRGB, DisplayP3 };

// GPUCanvasConfiguration.toneMapping.mode (GPUCanvasToneMappingMode): whether
// values above 1.0 on a float canvas are clamped to the display's SDR range
// ("standard", the default) or reach its extended range ("extended").
enum class CanvasToneMappingMode { Standard, Extended };

} // namespace rnwgpu
