import { createModifier } from "@expo/ui/swift-ui/modifiers";

// Custom modifier backed by our native RenderEffect module (registered as
// "layerEffectShader" via ViewModifierRegistry). Applies the Metal
// vignette/aberration shader as a SwiftUI .layerEffect to @expo/ui content.
export const layerEffectShader = (params?: { enabled?: boolean }) =>
  createModifier("layerEffectShader", params ?? {});
