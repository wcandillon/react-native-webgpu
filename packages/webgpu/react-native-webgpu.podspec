require "json"
require "fileutils"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))
folly_compiler_flags = '-DFOLLY_NO_CONFIG -DFOLLY_MOBILE=1 -DFOLLY_USE_LIBCPP=1 -Wno-comma -Wno-shorten-64-to-32'

# Resolve a node package directory using Node's own module resolution
# (mirrors `require.resolve(pkg/package.json)`). A lambda rather than a `def`:
# CocoaPods evaluates the podspec inside the `Pod` module, where top-level
# methods are not reachable at the call site.
resolve_node_package = lambda do |name, base_dir|
  script = "process.stdout.write(require('path').dirname(require.resolve('#{name}/package.json')))"
  dir = Dir.chdir(base_dir) { `node -e "#{script}" 2>/dev/null`.strip }
  dir.empty? ? nil : dir
end

# Dawn, the WebGPU implementation, ships in the react-native-webgpu-dawn npm
# package this package depends on (react-native-skia's Graphite backend links
# the same one, so an app installing both contains exactly one Dawn). Its
# headers are used in place, but CocoaPods only vendors frameworks from inside
# the pod, so the xcframework is copied into libs/apple at `pod install` time.
# The copy is stamped with the package version and skipped when it already
# matches, which keeps CocoaPods' cache intact across installs.
dawn_dir = resolve_node_package.call('react-native-webgpu-dawn', __dir__)
if dawn_dir.nil?
  raise "react-native-webgpu: the react-native-webgpu-dawn package was not found. " \
        "It ships the Dawn binaries this package links against; make sure " \
        "dependencies are installed (yarn install / npm install), then run `pod install` again."
end
dawn_version = JSON.parse(File.read(File.join(dawn_dir, 'package.json')))['version'].to_s
dawn_include = File.join(dawn_dir, 'include')
dawn_libs = File.join(__dir__, 'libs', 'apple')
dawn_marker = File.join(dawn_libs, '.version')
unless File.exist?(dawn_marker) && File.read(dawn_marker).strip == dawn_version
  Pod::UI.puts "react-native-webgpu: installing Dawn (react-native-webgpu-dawn #{dawn_version})"
  FileUtils.rm_rf(dawn_libs)
  FileUtils.mkdir_p(dawn_libs)
  FileUtils.cp_r(File.join(dawn_dir, 'libs', 'apple', 'libwebgpu_dawn.xcframework'), dawn_libs)
  File.write(dawn_marker, dawn_version)
end

Pod::Spec.new do |s|
  s.name         = "react-native-webgpu"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]

  s.platforms    = { :ios => min_ios_version_supported, :osx => "10.15", :visionos => "1.0" }
  s.source       = { :git => "https://github.com/wcandillon/react-native-webgpu.git", :tag => "#{s.version}" }

  # CocoaPods is the supported default. Package.swift (React Native 0.87+
  # SwiftPM autolinking) is additive: SwiftPM ignores this podspec, and
  # CocoaPods ignores Package.swift. An app links through one or the other,
  # never both, so set RNWGPU_USE_SPM=1 before `pod install` when linking
  # through the Swift package instead: RN's autolinking and codegen both key
  # off this podspec being present, so it still needs to exist, but it must
  # not compile or vendor anything itself or the app gets the native module
  # twice.
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
    'HEADER_SEARCH_PATHS' => "$(PODS_TARGET_SRCROOT)/cpp \"#{dawn_include}\"",
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
        "HEADER_SEARCH_PATHS" => "\"$(PODS_ROOT)/boost\" \"$(PODS_TARGET_SRCROOT)/cpp\" \"#{dawn_include}\"",
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
