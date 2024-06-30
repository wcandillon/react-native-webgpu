require "json"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))

Pod::Spec.new do |s|
  s.name         = "react-native-webgpu"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.description  = <<-DESC
                  react-native-webgpu
                   DESC
  s.homepage     = "https://github.com/wcandillon/react-native-webgpu"
  s.license      = "MIT"
  s.authors      = { "William Candillon" => "wcandillon@gmail.com" }
  s.platforms    = { :ios => "13.0" }
  s.source       = { :git => "https://github.com/wcandillon/react-native-webgpu/react-native-webgpu.git", :tag => "#{s.version}" }

  s.requires_arc = true
  s.pod_target_xcconfig = {
    'GCC_PREPROCESSOR_DEFINITIONS' => '$(inherited)',
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++17',
    'DEFINES_MODULE' => 'YES',
    "HEADER_SEARCH_PATHS" => '"$(PODS_TARGET_SRCROOT)/cpp/"/**'
  }

  s.ios.vendored_frameworks = [
    'libs/ios/libwebgpu_dawn.xcframework',
  ]

  s.source_files = [
    "ios/**/*.{h,c,cc,cpp,m,mm,swift}",  
    "cpp/**/*.{h,cpp}"
  ]

  if defined?(install_modules_dependencies()) != nil
    install_modules_dependencies(s)
    s.dependency "React"
    s.dependency "React-callinvoker"
    s.dependency "React-Core"
  else
    s.dependency "React"
    s.dependency "React-callinvoker"
    s.dependency "React-Core"
  end
end
