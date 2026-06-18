package expo.modules.rendereffect

import expo.modules.kotlin.modules.Module
import expo.modules.kotlin.modules.ModuleDefinition

class RenderEffectModule : Module() {
  override fun definition() = ModuleDefinition {
    Name("RenderEffect")

    View(RenderEffectView::class) {
      Prop("intensity") { _: RenderEffectView, _: Double ->
        // Reserved; the PoC ships one shader.
      }
    }
  }
}
