require "json"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))
folly_compiler_flags = '-DFOLLY_NO_CONFIG -DFOLLY_MOBILE=1 -DFOLLY_USE_LIBCPP=1 -Wno-comma -Wno-shorten-64-to-32'

Pod::Spec.new do |s|
  s.name         = "react-native-webgpu"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]

  s.platforms    = { :ios => min_ios_version_supported, :osx => "10.15", :visionos => "1.0" }
  s.source       = { :git => "https://github.com/wcandillon/react-native-webgpu.git", :tag => "#{s.version}" }

  s.source_files = [
    "apple/**/*.{h,c,cc,cpp,m,mm,swift}",
    "cpp/**/*.{h,cpp}"
  ]

  # Only the embedding API and the Apple module headers are public. The internal
  # C++ headers (cpp/jsi, cpp/rnwgpu, cpp/webgpu) stay private so a host pod
  # (e.g. Skia Graphite) never gets them on its include path: they use bare
  # includes and would otherwise collide with the host's own jsi/* utilities.
  # A host consumes the bindings via <react-native-wgpu/Embedding.h> only.
  s.public_header_files = [
    "apple/**/*.h",
    "cpp/public/*.h"
  ]

  # "Bring your own Dawn" mode. When another pod (e.g. Skia Graphite) already
  # links a Dawn build into the app, set RN_WEBGPU_EXTERNAL_DAWN=1 to skip
  # bundling our own libwebgpu_dawn.xcframework. In that mode react-native-wgpu
  # is header-only-ish C++: it links nothing Dawn-related and its Dawn symbols
  # are resolved from the host pod when the final app binary is linked.
  # Optionally set RN_WEBGPU_DAWN_HEADERS to the directory that contains the
  # `webgpu/` header folder (Dawn's include dir) so we compile against the
  # host's exact Dawn version instead of our bundled copy.
  external_dawn = ENV['RN_WEBGPU_EXTERNAL_DAWN'] == '1'
  dawn_headers  = ENV['RN_WEBGPU_DAWN_HEADERS']

  unless external_dawn
    s.vendored_frameworks = 'libs/apple/libwebgpu_dawn.xcframework'
  end

  # Header search paths. We recurse cpp/ so this package's own bare includes
  # (e.g. "NativeObject.h", "GPUBuffer.h") resolve from the source tree even
  # though those headers are now private (no longer flattened into the pod's
  # public headers). The host's Dawn headers are prepended in external mode so
  # the host's <webgpu/webgpu.h> wins over our bundled copy (ABI must match).
  header_search_paths = '"$(PODS_TARGET_SRCROOT)/cpp/"/** "$(PODS_TARGET_SRCROOT)/cpp"'
  if external_dawn && dawn_headers && !dawn_headers.empty?
    header_search_paths = "\"#{dawn_headers}\" #{header_search_paths}"
  end

  # In external mode define RN_WEBGPU_EXTERNAL_DAWN so the C++ can install the
  # host's Dawn proc table (the host's dawn_native build dispatches wgpu* calls
  # through a proc table; our bundled monolithic Dawn does not need this).
  external_dawn_defs = external_dawn ? ' RN_WEBGPU_EXTERNAL_DAWN=1' : ''

  # The VideoPlayer API uses AVFoundation / CoreMedia, and shared-texture
  # surfaces use CoreVideo (CVPixelBuffer). Link them so their symbols resolve.
  s.frameworks = "AVFoundation", "CoreMedia", "CoreVideo"

  s.pod_target_xcconfig = {
    'HEADER_SEARCH_PATHS' => header_search_paths,
    'GCC_PREPROCESSOR_DEFINITIONS' => "$(inherited)#{external_dawn_defs}",
  }

  # Use install_modules_dependencies helper to install the dependencies if React Native version >=0.71.0.
  # See https://github.com/facebook/react-native/blob/febf6b7f33fdb4904669f99d795eba4c0f95d7bf/scripts/cocoapods/new_architecture.rb#L79.
  if respond_to?(:install_modules_dependencies, true)
    install_modules_dependencies(s)
  else
    s.dependency "React-Core"
    s.compiler_flags = folly_compiler_flags
    s.pod_target_xcconfig    = {
        "HEADER_SEARCH_PATHS" => "\"$(PODS_ROOT)/boost\" #{header_search_paths}",
        "OTHER_CPLUSPLUSFLAGS" => "-DFOLLY_NO_CONFIG -DFOLLY_MOBILE=1 -DFOLLY_USE_LIBCPP=1#{external_dawn ? ' -DRN_WEBGPU_EXTERNAL_DAWN=1' : ''}",
        "CLANG_CXX_LANGUAGE_STANDARD" => "c++20"
    }
    s.dependency "React-RCTFabric"
    s.dependency "React-Codegen"
    s.dependency "RCT-Folly"
    s.dependency "RCTRequired"
    s.dependency "RCTTypeSafety"
    s.dependency "ReactCommon/turbomodule/core"
  end
end
