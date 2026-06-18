import ExpoModulesCore
import ExpoUI

public class RenderEffectModule: Module {
  public func definition() -> ModuleDefinition {
    Name("RenderEffect")

    // Register our Metal shader as a custom @expo/ui SwiftUI modifier so it can
    // be applied to React-Native-authored SwiftUI content from JS.
    OnCreate {
      ViewModifierRegistry.register("layerEffectShader") { params, appContext, _ in
        return try LayerEffectShaderModifier(from: params, appContext: appContext)
      }
    }

    OnDestroy {
      ViewModifierRegistry.unregister("layerEffectShader")
    }

    // The original standalone SwiftUI demo view (kept for reference / fallback).
    View(RenderEffectView.self) {
      Prop("intensity") { (view: RenderEffectView, intensity: Double) in
        view.intensity = intensity
      }
    }
  }
}
