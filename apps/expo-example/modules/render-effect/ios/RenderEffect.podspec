Pod::Spec.new do |s|
  s.name           = 'RenderEffect'
  s.version        = '0.1.0'
  s.summary        = 'Path B PoC: native compositor shader (SwiftUI layerEffect)'
  s.description    = 'Applies a Metal layerEffect to native scrolling content.'
  s.author         = ''
  s.homepage       = 'https://github.com/wcandillon/react-native-webgpu'
  s.platforms      = {
    :ios => '15.1',
    :tvos => '15.1'
  }
  s.source         = { git: '' }
  s.static_framework = true

  s.dependency 'ExpoModulesCore'
  # ExpoUI provides ViewModifierRegistry, used to register our custom
  # layerEffect shader modifier for @expo/ui SwiftUI content.
  s.dependency 'ExpoUI'

  s.pod_target_xcconfig = {
    'DEFINES_MODULE' => 'YES',
    'SWIFT_COMPILATION_MODE' => 'wholemodule'
  }

  s.source_files = "**/*.{h,m,mm,swift,hpp,cpp}"

  # Compile the .metal into default.metallib inside RenderEffectShaders.bundle.
  # SwiftUI's ShaderLibrary.default only reads the app's main-bundle metallib, so
  # a pod shader must be shipped in its own bundle and loaded via ShaderLibrary(url:).
  s.resource_bundles = {
    'RenderEffectShaders' => ['**/*.metal']
  }
end
