// Pins the SwiftPM autolinking target name. Without it, React Native derives
// the name — today from the npm package name, but an in-flight React Native
// change derives it from the podspec instead. The name is also the header
// import prefix and the symlink name under
// <app>/ios/build/generated/autolinking/libs/, so it must not move once apps
// depend on it.
module.exports = { spm: { name: "react-native-webgpu" } };
