/* eslint-disable camelcase */
/* eslint-disable max-len */
import { chdir } from "process";

import type { Platform } from "./util";
import { $, build, checkBuildArtifacts, copyLib, libs, mapKeys } from "./util";

const commonArgs = {
  CMAKE_BUILD_TYPE: "Release",
  BUILD_SAMPLES: "OFF",
  TINT_BUILD_TESTS: "OFF",
  TINT_BUILD_CMD_TOOLS: "OFF",
  TINT_BUILD_IR_BINARY: "OFF",
  DAWN_BUILD_SAMPLES: "OFF",
  DAWN_USE_GLFW: "OFF",
  DAWN_FETCH_DEPENDENCIES: "ON",
  DAWN_BUILD_MONOLITHIC_LIBRARY: "ON",
};

const android = {
  platforms: ["arm64-v8a", "armeabi-v7a", "x86", "x86_64"] as Platform[],
  args: {
    CMAKE_TOOLCHAIN_FILE: "$ANDROID_NDK/build/cmake/android.toolchain.cmake",
    ANDROID_PLATFORM: "android-26",
    ...commonArgs,
  },
};

const ios = {
  matrix: {
    arm64: ["iphoneos", "iphonesimulator"],
    x86_64: ["iphonesimulator"],
  },
  args: {
    CMAKE_SYSTEM_NAME: "iOS",
    CMAKE_OSX_DEPLOYMENT_TARGET: "13.0",
    ...commonArgs,
  },
};

(async () => {
  process.chdir("..");
  process.chdir("externals/dawn");
  $("git reset --hard HEAD");
  // $("git submodule update --recursive --force");
  // $("git submodule foreach --recursive git reset --hard");
  // $("git submodule foreach --recursive git clean -fdx");
  $(
    "git fetch https://dawn.googlesource.com/dawn refs/changes/96/195996/25 && git checkout FETCH_HEAD",
  );
  $("git apply ../../package/scripts/build/patch.patch");
  process.chdir("../..");

  // Build iOS
  for (const platform of mapKeys(ios.matrix)) {
    console.log(`Build iOS: ${platform}`);
    for (const sdk of ios.matrix[platform]) {
      await build(
        `ios_${platform}_${sdk}`,
        {
          CMAKE_OSX_ARCHITECTURES: platform,
          CMAKE_OSX_SYSROOT: `$(xcrun --sdk ${sdk} --show-sdk-path)`,
          ...ios.args,
        },
        `🍏 ${platform} ${sdk}`,
      );
      copyLib("ios", platform, sdk);
    }
  }

  libs.forEach((lib) => {
    console.log(`Building fat binary for iphone simulator: ${lib}`);
    $(
      `lipo -create package/libs/ios/x86_64_iphonesimulator/${lib}.dylib package/libs/ios/arm64_iphonesimulator/${lib}.dylib -output package/libs/ios/${lib}.dylib`,
    );
  });

  libs.forEach((lib) => {
    console.log(`Building ${lib}`);
    $(`rm -rf ./package/libs/ios/${lib}.xcframework`);
    $(
      "xcodebuild -create-xcframework " +
        `-library ./package/libs/ios/${lib}.dylib ` +
        `-library ./package/libs/ios/arm64_iphoneos/${lib}.dylib ` +
        ` -output ./package/libs/ios/${lib}.xcframework `,
    );
  });
  // Build Android
  for (const platform of android.platforms) {
    console.log(`Build Android: ${platform}`);
    await build(
      `android_${platform}`,
      {
        ANDROID_ABI: platform,
        ...android.args,
      },
      `🤖 ${platform}`,
    );
    copyLib("android", platform);
  }

  console.log("Copy headers");
  $("cp -R externals/dawn/out/android_arm64-v8a/gen/include/* package/cpp");
  $("cp externals/dawn/include/webgpu/webgpu_cpp.h package/cpp/webgpu/");
  $(
    "cp externals/dawn/include/webgpu/webgpu_enum_class_bitmasks.h package/cpp/webgpu/",
  );
  $("cp externals/dawn/include/webgpu/webgpu.h package/cpp/webgpu/");
  chdir("package");
  checkBuildArtifacts();
})();
