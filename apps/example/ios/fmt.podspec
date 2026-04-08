# Overrides React Native's fmt.podspec to use fmt 12.1.0 for Xcode 16.4+ compatibility
# See: https://github.com/facebook/react-native/pull/56099

Pod::Spec.new do |spec|
  spec.name = "fmt"
  spec.version = "12.1.0"
  spec.license = { :type => "MIT" }
  spec.homepage = "https://github.com/fmtlib/fmt"
  spec.summary = "{fmt} is an open-source formatting library for C++. It can be used as a safe and fast alternative to (s)printf and iostreams."
  spec.authors = "The fmt contributors"
  spec.source = {
    :git => "https://github.com/fmtlib/fmt.git",
    :tag => "12.1.0"
  }
  spec.pod_target_xcconfig = {
    "CLANG_CXX_LANGUAGE_STANDARD" => "c++20",
    "GCC_WARN_INHIBIT_ALL_WARNINGS" => "YES"
  }
  spec.platforms = min_supported_versions
  spec.libraries = "c++"
  spec.public_header_files = "include/fmt/*.h"
  spec.header_mappings_dir = "include"
  spec.source_files = ["include/fmt/*.h", "src/format.cc"]
end
