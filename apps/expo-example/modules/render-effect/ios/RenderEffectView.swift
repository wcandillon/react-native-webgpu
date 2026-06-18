import ExpoModulesCore
import SwiftUI

// Locate the compiled Metal library shipped in the module's resource bundle.
// SwiftUI's ShaderLibrary.default only looks in the app's main bundle, so a
// shader living in this pod must be loaded explicitly from its own bundle.
func renderEffectMetallibURL() -> URL? {
  let frameworkBundle = Bundle(for: RenderEffectView.self)
  // CocoaPods nests the resource bundle inside the framework bundle.
  if let resURL = frameworkBundle.url(forResource: "RenderEffectShaders", withExtension: "bundle"),
     let resBundle = Bundle(url: resURL),
     let metallib = resBundle.url(forResource: "default", withExtension: "metallib") {
    return metallib
  }
  // Fallback: the framework's own metallib (if the .metal got compiled there).
  if let metallib = frameworkBundle.url(forResource: "default", withExtension: "metallib") {
    return metallib
  }
  return nil
}

@available(iOS 17.0, *)
private struct LayerEffectModifier: ViewModifier {
  let metallibURL: URL?
  let size: CGSize

  func body(content: Content) -> some View {
    if let url = metallibURL, size.width > 0, size.height > 0 {
      content.layerEffect(
        ShaderLibrary(url: url).vignetteAberration(
          .float2(Float(size.width), Float(size.height))
        ),
        maxSampleOffset: CGSize(width: 80, height: 8)
      )
    } else {
      content
    }
  }
}

// The SwiftUI content: a scrolling list with the Metal layerEffect applied by
// the compositor (when the shader is found). The shader re-runs per frame as the
// list scrolls, with no readback and no per-frame work from JS, the iOS analog
// of Android's View.setRenderEffect.
@available(iOS 17.0, *)
struct RenderEffectContent: View {
  let metallibURL: URL?

  private let bg = Color(red: 0.059, green: 0.090, blue: 0.165)
  private let rowColors: [Color] = [.blue, .purple, .teal, .pink, .orange, .indigo, .green]

  var body: some View {
    GeometryReader { geo in
      // TimelineView(.animation) redraws every display frame with no JS and no
      // capture; the static GPU shader re-applies to the live content each
      // frame, which is the realtime value of Path B.
      TimelineView(.animation) { timeline in
        animated(t: timeline.date.timeIntervalSinceReferenceDate)
          .frame(maxWidth: .infinity, maxHeight: .infinity)
          .background(bg)
          .modifier(LayerEffectModifier(metallibURL: metallibURL, size: geo.size))
      }
      .overlay(alignment: .top) {
        Text(metallibURL == nil ? "⚠️ no shader" : "✨ live content + GPU shader")
          .font(.caption.weight(.semibold))
          .padding(.horizontal, 10)
          .padding(.vertical, 6)
          .background(.black.opacity(0.55))
          .foregroundStyle(.white)
          .clipShape(Capsule())
          .padding(.top, 10)
      }
    }
  }

  private func animated(t: Double) -> some View {
    VStack(spacing: 14) {
      // A ticking monospaced counter: proves the content is redrawing every frame.
      Text(String(format: "%07.2f", t.truncatingRemainder(dividingBy: 10000)))
        .font(.system(size: 44, weight: .heavy, design: .monospaced))
        .foregroundStyle(.white)
        .padding(.top, 64)

      ForEach(0..<7, id: \.self) { i in
        let phase = t * 1.6 + Double(i) * 0.6
        Capsule()
          .fill(rowColors[i % rowColors.count].gradient)
          .frame(height: 46)
          .overlay(
            Text("Row #\(i)")
              .font(.headline.weight(.bold))
              .foregroundStyle(.white)
          )
          .padding(.horizontal, 24)
          .offset(x: CGFloat(sin(phase)) * 70)
      }

      Circle()
        .fill(Color.cyan.gradient)
        .frame(width: 64, height: 64)
        .offset(x: CGFloat(sin(t * 1.2)) * 120)

      Spacer(minLength: 0)
    }
  }
}

// ExpoView host. The UIHostingController is added as a child view controller and
// pinned to the edges so the SwiftUI content fills the native view.
class RenderEffectView: ExpoView {
  var intensity: Double = 1.0

  private var hostingController: UIViewController?

  required init(appContext: AppContext? = nil) {
    super.init(appContext: appContext)
    let metallibURL = renderEffectMetallibURL()

    let controller: UIViewController
    if #available(iOS 17.0, *) {
      controller = UIHostingController(rootView: RenderEffectContent(metallibURL: metallibURL))
    } else {
      controller = UIHostingController(rootView: FallbackContent())
    }
    controller.view.backgroundColor = .clear
    controller.view.translatesAutoresizingMaskIntoConstraints = false
    addSubview(controller.view)
    NSLayoutConstraint.activate([
      controller.view.topAnchor.constraint(equalTo: topAnchor),
      controller.view.bottomAnchor.constraint(equalTo: bottomAnchor),
      controller.view.leadingAnchor.constraint(equalTo: leadingAnchor),
      controller.view.trailingAnchor.constraint(equalTo: trailingAnchor),
    ])
    hostingController = controller
  }
}

// Plain list for iOS < 17 (no layerEffect available).
struct FallbackContent: View {
  private let rows = Array(0..<40)
  var body: some View {
    ScrollView {
      LazyVStack(spacing: 0) {
        ForEach(rows, id: \.self) { i in
          Text("Scrolling row #\(i)")
            .font(.title3.weight(.semibold))
            .foregroundStyle(.white)
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(.horizontal, 24)
            .frame(height: 72)
            .background(
              i % 2 == 0
                ? Color(red: 0.118, green: 0.161, blue: 0.231)
                : Color(red: 0.200, green: 0.255, blue: 0.333)
            )
        }
      }
    }
    .background(Color(red: 0.059, green: 0.090, blue: 0.165))
  }
}
