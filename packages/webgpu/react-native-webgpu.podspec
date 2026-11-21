require "json"
require "open3"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))

# Configuration reads the installed release pin only. SDK downloads wait for
# Xcode's platform/architecture settings; headers already ship in this package.
stdout, stderr, status = Open3.capture3(
  '/bin/bash', File.join(__dir__, 'scripts', 'prepare-apple-binaries.sh'),
  '--config'
)
raise "react-native-webgpu: #{stderr.strip}" unless status.success?
binary_config = JSON.parse(stdout)
binary_search_path = '$(inherited) "$(PODS_CONFIGURATION_BUILD_DIR)/ReactNativeWebGPUBinaries"'

Pod::Spec.new do |s|
  s.name         = "react-native-webgpu"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]

  # CocoaPods autolinks only declared native platforms. Knowing a release SDK
  # in the preparer alone does not establish WebGPU runtime support there.
  s.platforms    = {
    :ios => min_ios_version_supported,
    :tvos => min_ios_version_supported,
    :osx => "14.0",
  }
  s.source       = { :git => "https://github.com/wcandillon/react-native-webgpu.git", :tag => "#{s.version}" }

  # CocoaPods and SwiftPM compile these sources independently; an app chooses
  # one native integration. Neither downloads binaries during package setup.
  s.source_files = [
    "apple/**/*.{h,c,cc,cpp,m,mm,swift}",
    "cpp/**/*.{h,cpp}"
  ]

  # Retain nested paths: Dawn's forwarding and generated headers share basenames.
  s.header_mappings_dir = "."
  s.libraries = ['c++'] + binary_config['linkNames']
  s.preserve_paths = ['scripts/*.{js,sh}', 'libs/apple/**/*']
  s.user_target_xcconfig = { 'LIBRARY_SEARCH_PATHS' => binary_search_path }
  s.script_phase = {
    :name => 'Prepare React Native WebGPU binaries',
    :shell_path => '/bin/bash',
    :execution_position => :before_compile,
    :always_out_of_date => '1',
    :script => <<-SCRIPT
      /bin/bash "${PODS_TARGET_SRCROOT}/scripts/prepare-apple-binaries.sh" \
        --output "${PODS_CONFIGURATION_BUILD_DIR}/ReactNativeWebGPUBinaries" \
        --assembly-root "${OBJROOT}/ReactNativeWebGPUBinaries"
    SCRIPT
  }

  # The VideoPlayer API uses AVFoundation / CoreMedia, and shared-texture
  # surfaces use CoreVideo (CVPixelBuffer). Link them so their symbols resolve.
  # ImageIO provides CGImageSource, the image decoder behind createImageBitmap.
  s.frameworks = "AVFoundation", "CoreMedia", "CoreVideo", "ImageIO"

  # Under use_frameworks!, bare includes across folders (e.g. "GPU.h")
  # resolve only through these paths. Keep in sync with internalHeaderDirs
  # in Package.swift.
  header_dirs = %w[apple cpp cpp/jsi cpp/rnwgpu cpp/rnwgpu/api cpp/rnwgpu/api/descriptors cpp/rnwgpu/async cpp/dawn/include]
  header_search_paths = header_dirs.map { |dir| "\"$(PODS_TARGET_SRCROOT)/#{dir}\"" }

  s.pod_target_xcconfig = {
    'HEADER_SEARCH_PATHS' => header_search_paths.join(' '),
    # Preparation needs access to the shared release cache and build products.
    'ENABLE_USER_SCRIPT_SANDBOXING' => 'NO',
    'LIBRARY_SEARCH_PATHS' => binary_search_path,
    # Xcode's all-target headermaps let same-named headers leak across pods
    # (e.g. @shopify/react-native-skia keeps a jsi/ helper layer with
    # identical relative header paths). Resolve includes strictly through our
    # own search paths instead.
    'USE_HEADERMAP' => 'NO',
  }

  install_modules_dependencies(s)
end
