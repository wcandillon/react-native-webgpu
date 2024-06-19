import { $, checkFileExists } from "./util";

$("cp -R artifacts/libs .");
$("cp -R artifacts/cpp/dawn cpp");
$("cp -R artifacts/cpp/webgpu cpp");
checkFileExists("libs/android/x86/libwebgpu_dawn.a");
checkFileExists("libs/android/x86_64/libwebgpu_dawn.a");
checkFileExists("libs/android/armeabi-v7a/libwebgpu_dawn.a");
checkFileExists("libs/android/arm64-v8a/libwebgpu_dawn.a");
checkFileExists("libs/ios/libwebgpu_dawn.xcframework");
checkFileExists("libs/ios/arm64_iphoneos/libwebgpu_dawn.a");
checkFileExists("cpp/dawn/webgpu_cpp.h");
