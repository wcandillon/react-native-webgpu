// Pins the SwiftPM target name. Without it the name is derived: today from the
// npm name (ReactNativeWebgpu), but an in-flight React Native change derives it
// from the podspec instead. The name is also the header import prefix, so it
// must not move. Package.swift declares the same name for its package, product
// and target; scripts/package-swift-template.ts carries it as SWIFT_PACKAGE_NAME.
module.exports = { spm: { name: "ReactNativeWebGPU" } };
