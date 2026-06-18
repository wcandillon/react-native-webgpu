package expo.modules.rendereffect

import android.content.Context
import android.graphics.Color
import android.graphics.RenderEffect
import android.graphics.RuntimeShader
import android.os.Build
import android.view.Gravity
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import expo.modules.kotlin.AppContext
import expo.modules.kotlin.views.ExpoView

// Path B Android view: a native scrolling list with a GPU compositor shader
// attached via View.setRenderEffect + a RuntimeShader (AGSL). The compositor
// re-runs the shader on every redraw while scrolling, with no readback and no
// per-frame work from JS, the Android analog of SwiftUI's .layerEffect.
class RenderEffectView(context: Context, appContext: AppContext) :
  ExpoView(context, appContext) {

  // AGSL counterpart of the iOS Metal shader: vignette + edge chromatic
  // aberration. `content` is the input view layer; coordinates are in pixels.
  private val agsl = """
    uniform shader content;
    uniform float2 iResolution;
    half4 main(float2 fragCoord) {
      float2 uv = fragCoord / iResolution;
      float2 centered = uv - 0.5;
      float dist = length(centered);
      // Constant horizontal RGB split (visible everywhere) growing to the edges.
      float split = (0.012 + dist * 0.04) * iResolution.x;
      half4 c;
      c.r = content.eval(fragCoord + float2(split, 0.0)).r;
      c.g = content.eval(fragCoord).g;
      c.b = content.eval(fragCoord - float2(split, 0.0)).b;
      c.a = 1.0;
      // Strong vignette (smoothstep needs edge0 < edge1, so invert).
      float vignette = 1.0 - smoothstep(0.25, 0.95, dist);
      vignette *= vignette;
      c.rgb *= vignette;
      return c;
    }
  """.trimIndent()

  init {
    val scroll = ScrollView(context)
    val column = LinearLayout(context).apply {
      orientation = LinearLayout.VERTICAL
    }
    for (i in 0 until 40) {
      val row = TextView(context).apply {
        text = "Scrolling row #$i"
        textSize = 20f
        gravity = Gravity.CENTER_VERTICAL
        setPadding(48, 36, 48, 36)
        setTextColor(Color.parseColor("#F8FAFC"))
        setBackgroundColor(
          if (i % 2 == 0) Color.parseColor("#1E293B") else Color.parseColor("#334155")
        )
      }
      column.addView(row)
    }
    scroll.addView(column)
    addView(scroll)
  }

  override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
    super.onSizeChanged(w, h, oldw, oldh)
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU && w > 0 && h > 0) {
      val shader = RuntimeShader(agsl)
      shader.setFloatUniform("iResolution", w.toFloat(), h.toFloat())
      setRenderEffect(RenderEffect.createRuntimeShaderEffect(shader, "content"))
    }
  }
}
