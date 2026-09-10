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

  # RN's autolinking and codegen both key off this podspec being present and
  # supporting the platform (see react-native.config.js docs: there is no way
  # to disable CocoaPods linking for a dependency without also disabling
  # codegen for it). So when the app links react-native-webgpu via the local
  # Swift Package (packages/webgpu/Package.swift) instead of CocoaPods, this
  # pod still needs to exist to keep the RNWgpuViewSpec codegen output
  # flowing — it just shouldn't compile or link anything itself, or the app
  # would get the native module twice. Set RNWGPU_USE_SPM=1 before
  # `pod install` for that case.
  if ENV['RNWGPU_USE_SPM']
    # The Swift package resolves React Native's headers from the
    # static-library Pods layout (Pods/Headers/Public). With `use_frameworks!`
    # CocoaPods keeps headers inside each framework instead, so none of those
    # paths exist and the package fails to compile with header-not-found
    # errors that don't point back here. Refuse the combination up front.
    # The React Native template drives `use_frameworks!` from USE_FRAMEWORKS.
    if ENV['USE_FRAMEWORKS']
      raise "react-native-webgpu: RNWGPU_USE_SPM needs the static-library Pods layout. " \
            "Unset USE_FRAMEWORKS (or drop use_frameworks! from the Podfile) to link " \
            "react-native-webgpu through its Swift package."
    end
    s.source_files = "apple/RNWGUIKit.h"
  else
    s.source_files = [
      "apple/**/*.{h,c,cc,cpp,m,mm,swift}",
      "cpp/**/*.{h,cpp}"
    ]

    s.vendored_frameworks = 'libs/apple/libwebgpu_dawn.xcframework'

    # The VideoPlayer API uses AVFoundation / CoreMedia, and shared-texture
    # surfaces use CoreVideo (CVPixelBuffer). Link them so their symbols resolve.
    # ImageIO provides CGImageSource, the image decoder behind createImageBitmap.
    s.frameworks = "AVFoundation", "CoreMedia", "CoreVideo", "ImageIO"
  end

  s.pod_target_xcconfig = {
    'HEADER_SEARCH_PATHS' => '$(PODS_TARGET_SRCROOT)/cpp',
    # Xcode's all-target headermaps let same-named headers leak across pods
    # (e.g. @shopify/react-native-skia keeps a jsi/ helper layer with
    # identical relative header paths). Resolve includes strictly through our
    # own search paths instead.
    'USE_HEADERMAP' => 'NO',
  }

  # Use install_modules_dependencies helper to install the dependencies if React Native version >=0.71.0.
  # See https://github.com/facebook/react-native/blob/febf6b7f33fdb4904669f99d795eba4c0f95d7bf/scripts/cocoapods/new_architecture.rb#L79.
  if respond_to?(:install_modules_dependencies, true)
    install_modules_dependencies(s)
  else
    s.dependency "React-Core"
    s.compiler_flags = folly_compiler_flags
    s.pod_target_xcconfig    = {
        "HEADER_SEARCH_PATHS" => "\"$(PODS_ROOT)/boost\" \"$(PODS_TARGET_SRCROOT)/cpp\"",
        "OTHER_CPLUSPLUSFLAGS" => "-DFOLLY_NO_CONFIG -DFOLLY_MOBILE=1 -DFOLLY_USE_LIBCPP=1",
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
