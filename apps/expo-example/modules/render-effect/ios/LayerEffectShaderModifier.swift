import ExpoModulesCore
import SwiftUI

// A custom @expo/ui SwiftUI view modifier that applies our Metal
// vignette/aberration shader as a .layerEffect. Because @expo/ui renders genuine
// SwiftUI, this runs inside the SAME render tree as the React-Native-authored
// content (Host / VStack / Text), so the compositor re-applies the shader every
// frame the content changes, with no capture and no readback.
//
// Registered as "layerEffectShader" via ViewModifierRegistry (see
// RenderEffectModule), invoked from JS through createModifier("layerEffectShader").
internal struct LayerEffectShaderModifier: ViewModifier, Record {
  @Field var enabled: Bool = true

  @ViewBuilder
  func body(content: Content) -> some View {
    if enabled, #available(iOS 17.0, *), let url = renderEffectMetallibURL() {
      // visualEffect provides the GeometryProxy so the shader gets the live
      // size without a layout-affecting GeometryReader.
      content.visualEffect { effect, proxy in
        effect.layerEffect(
          ShaderLibrary(url: url).vignetteAberration(
            .float2(Float(proxy.size.width), Float(proxy.size.height))
          ),
          maxSampleOffset: CGSize(width: 80, height: 8)
        )
      }
    } else {
      content
    }
  }
}
