// swift-tools-version: 6.0
//
// SwiftPM manifest for react-native-webgpu. iOS only.
// Additive: the CocoaPods podspec remains the supported default.
//
// React Native 0.87+ references a library that ships its own Package.swift
// through a symlink at <app>/ios/build/generated/autolinking/libs/<name>, and
// resolves the relative paths below against that symlink. They are the same
// for every standard app, because the symlink location is. Earlier React
// Native releases have no SwiftPM autolinking: keep CocoaPods there.

import Foundation
import PackageDescription

// Absolute location of this package. Xcode passes the autolinking symlink as
// the package directory, so resolve it before deriving sibling paths.
let packageRoot = URL(fileURLWithPath: Context.packageDirectory)
  .resolvingSymlinksInPath().path
let sibling = { (relative: String) -> String in
  URL(fileURLWithPath: "\(packageRoot)/\(relative)").standardized.path
}
// A node package next to this one: a sibling in the consumer's node_modules,
// or under this monorepo's root.
let nodePackage = { (name: String) -> String? in
  ["../\(name)", "../../node_modules/\(name)"]
    .map(sibling)
    .first { FileManager.default.fileExists(atPath: "\($0)/package.json") }
}
let readJSON = { (path: String) -> [String: Any]? in
  guard let data = FileManager.default.contents(atPath: path) else { return nil }
  return try? JSONSerialization.jsonObject(with: data) as? [String: Any]
}

// Dawn, the WebGPU implementation, comes from the react-native-webgpu-dawn npm
// package this package depends on: an xcframework with the `webgpu/` and
// `dawn/` headers embedded in every slice, which SwiftPM exposes to dependents
// without any search path. react-native-skia's Graphite backend links the
// same package, so an app installing both contains exactly one Dawn.
guard let dawnPath = nodePackage("react-native-webgpu-dawn") else {
  // Reached when the dependency was skipped at install time, which SwiftPM
  // would otherwise report as an unresolvable dependency path.
  fatalError(
    """
    react-native-webgpu-dawn was not found next to react-native-webgpu. It ships \
    the Dawn binaries this package links against. Reinstall dependencies.
    """)
}
let ownPackage = readJSON("\(packageRoot)/package.json") ?? [:]
let dawnVersion =
  (readJSON("\(dawnPath)/package.json")?["version"] as? String) ?? "unknown"

// Skia Graphite and WebGPU must link the exact same Dawn, or the pair links
// fine and crashes at runtime. react-native-skia's podspec enforces this at
// `pod install` time; a Swift package has no install hook, so the same check
// runs here, at manifest evaluation. SwiftPM caches evaluations, so after
// changing either package's Dawn reset the package caches (File > Packages >
// Reset Package Caches) if this does not re-run.
//
// A react-native-skia that depends on react-native-webgpu-dawn itself must pin
// the same version as this package. Older Graphite builds (react-native-skia
// 3.0.x) vendor their own Dawn and record its release tag at
// libs/.dawn-version in their binary packages; this package's `dawn` field
// names the release that Dawn is interchangeable with.
if let skiaPath = nodePackage("react-native-skia"),
  let skiaPackage = readJSON("\(skiaPath)/package.json"),
  let skiaDawn = (skiaPackage["dependencies"] as? [String: String])?["react-native-webgpu-dawn"]
{
  if skiaDawn != dawnVersion {
    fatalError(
      """
      react-native-webgpu: Dawn version mismatch. This package links \
      react-native-webgpu-dawn \(dawnVersion) but react-native-skia depends on \
      \(skiaDawn). Align the two packages so the app contains exactly one Dawn.
      """)
  }
} else {
  let dawnReleaseTag =
    "dawn-\((ownPackage["dawn"] as? String ?? "").replacingOccurrences(of: "/", with: "-"))"
  // The graphite packages are normally hoisted next to this package, but may
  // be nested under react-native-skia when the package manager cannot hoist
  // them. The v2 Graphite previews (e.g. @shopify/react-native-skia@2.12.0-next.1)
  // keep the marker in their own libs/. Every marker found is checked.
  let skiaDawnMarkers = [
    "react-native-skia-graphite-apple-ios/libs/.dawn-version",
    "react-native-skia-graphite-apple-macos/libs/.dawn-version",
    "react-native-skia/node_modules/react-native-skia-graphite-apple-ios/libs/.dawn-version",
    "react-native-skia/node_modules/react-native-skia-graphite-apple-macos/libs/.dawn-version",
    "@shopify/react-native-skia/libs/.dawn-version",
  ].flatMap { marker in
    [
      sibling("../\(marker)"), // consumer: sibling in node_modules
      sibling("../../node_modules/\(marker)"), // this monorepo
    ]
  }
  for marker in skiaDawnMarkers {
    guard let data = FileManager.default.contents(atPath: marker),
      let skiaDawn = String(data: data, encoding: .utf8)?
        .trimmingCharacters(in: .whitespacesAndNewlines)
    else { continue }
    if skiaDawn != dawnReleaseTag {
      fatalError(
        """
        react-native-webgpu: Dawn version mismatch. This package links \
        react-native-webgpu-dawn \(dawnVersion) (\(dawnReleaseTag)) but the \
        react-native-skia Graphite binaries (\(marker)) link \(skiaDawn). Align \
        the two packages so the app contains exactly one Dawn.
        """)
    }
  }
}

// Unlike a native Xcode target, SwiftPM doesn't generate an implicit
// per-target header map, so quoted includes only resolve within the
// including file's own directory. The sources use bare cross-directory
// includes (e.g. apple/WebGPUModule.h includes "RNWebGPUManager.h" from
// cpp/rnwgpu), so every directory that's the target of one of those bare
// includes has to be listed explicitly here. The "webgpu/..." and "dawn/..."
// includes resolve through the Dawn xcframework's embedded headers.
let internalHeaderDirs = [
  "apple",
  "cpp",
  "cpp/jsi",
  "cpp/rnwgpu",
  "cpp/rnwgpu/api",
  "cpp/rnwgpu/api/descriptors",
  "cpp/rnwgpu/async",
]

let package = Package(
  name: "ReactNativeWebGPU",
  // SwiftPM refuses to link a product whose floor is above the depending
  // target's. React Native's generated Autolinked aggregate hardcodes iOS 15.0
  // (react-native#58379 derives it from the app instead), so this must not
  // exceed 15.0; a lower floor is always accepted.
  platforms: [.iOS(.v15)],
  products: [
    // Autolinking looks this name up verbatim; react-native.config.js pins it.
    .library(name: "ReactNativeWebGPU", targets: ["ReactNativeWebGPU"])
  ],
  dependencies: [
    .package(name: "React-GeneratedCode", path: "../../../ios"),
    .package(name: "ReactNative", path: "../../../../xcframeworks"),
    .package(path: dawnPath),
  ],
  targets: [
    .target(
      name: "ReactNativeWebGPU",
      dependencies: [
        .product(name: "ReactHeaders", package: "ReactNative"),
        .product(name: "ReactNativeHeaders", package: "ReactNative"),
        .product(name: "ReactNativeDependenciesHeaders", package: "ReactNative"),
        // RNWgpuViewSpec, generated by the app's codegen run.
        .product(name: "ReactAppHeaders", package: "React-GeneratedCode"),
        .product(name: "react-native-webgpu-dawn", package: "react-native-webgpu-dawn"),
      ],
      // apple/ and cpp/ have no common ancestor below the package root, and
      // .headerSearchPath cannot escape the target path.
      path: ".",
      // Explicit, so SwiftPM never walks node_modules, lib or android.
      sources: ["apple", "cpp/jsi", "cpp/rnwgpu"],
      // SwiftPM requires a public headers directory inside the target and
      // defaults to "include", which this package does not have.
      publicHeadersPath: "apple",
      cxxSettings: internalHeaderDirs.map { CXXSetting.headerSearchPath($0) } + [
        // CocoaPods forces both project-wide; the SwiftPM path defines neither.
        // React Native's headers gate on them, and the prebuilt React.framework
        // was built with both, so the module must see the same declarations.
        .define("RCT_NEW_ARCH_ENABLED", to: "1"),
        .define("RCT_REMOVE_LEGACY_ARCH", to: "1"),

        // React's prebuilt C++ ABI is NDEBUG-gated (DebugStringConvertible's
        // vtable and ShadowNode layout diverge). Omitting either breaks the
        // Release link.
        .define("DEBUG", .when(configuration: .debug)),
        .define("NDEBUG", .when(configuration: .release)),
      ],
      linkerSettings: [
        // VideoPlayer uses AVFoundation / CoreMedia, shared-texture surfaces
        // use CoreVideo (CVPixelBuffer), and ImageIO provides CGImageSource,
        // the image decoder behind createImageBitmap.
        .linkedFramework("AVFoundation"),
        .linkedFramework("CoreMedia"),
        .linkedFramework("CoreVideo"),
        .linkedFramework("ImageIO"),
      ]
    ),
  ],
  // React Native's headers need C++20.
  cxxLanguageStandard: .cxx20
)
