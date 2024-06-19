/* eslint-disable camelcase */
/* eslint-disable max-len */
import type { Platform, SDK } from "./util";
import { $, build, copyLib, mapKeys } from "./util";

const PATH = `PATH=${__dirname}/../../externals/depot_tools/:$PATH`;

const commonArgs = {
  CMAKE_BUILD_TYPE: "Release",
};

const android = {
  platforms: ["arm64-v8a", "armeabi-v7a", "x86", "x86_64"] as Platform[],
  args: {
    CMAKE_TOOLCHAIN_FILE: "$ANDROID_NDK/build/cmake/android.toolchain.cmake",
    TINT_BUILD_CMD_TOOLS: "OFF",
    ANDROID_PLATFORM: "android-26",
    ...commonArgs,
  },
};

type BuildMatrix = Record<Platform, SDK[]>;

const ios = {
  matrix: {
    arm64: ["iphoneos", "iphonesimulator"],
    x86_64: ["iphonesimulator"],
  } as BuildMatrix,
  args: {
    CMAKE_SYSTEM_NAME: "iOS",
    CMAKE_OSX_DEPLOYMENT_TARGET: "13.0",
    TINT_BUILD_CMD_TOOLS: "OFF",
    DAWN_BUILD_SAMPLES: "OFF",
    DAWN_USE_GLFW: "OFF",
    BUILD_SAMPLES: "OFF",
    TINT_BUILD_TESTS: "OFF",
    ...commonArgs,
  },
};

(async () => {
  // Fetch dependencies
  console.log("gclient sync");
  process.chdir("../externals/dawn");
  $("cp scripts/standalone.gclient .gclient");
  $(`${PATH} gclient sync`);
  process.chdir("../..");
  console.log("gclient sync done");

  // Build iOS
  for (const platform of mapKeys(ios.matrix)) {
    console.log(`Build iOS: ${platform}`);
    for (const sdk of ios.matrix[platform]) {
      await build(`ios_${platform}_${sdk}`, {
        CMAKE_OSX_ARCHITECTURES: platform,
        CMAKE_OSX_SYSROOT: `$(xcrun --sdk ${sdk} --show-sdk-path)`,
        ...ios.args,
      });
      copyLib("ios", platform, sdk);
    }
  }

  console.log("Building fat binary for iphone simulator");
  $(
    "lipo -create package/libs/ios/x86_64_iphonesimulator/libwebgpu_dawn.a package/libs/ios/arm64_iphonesimulator/libwebgpu_dawn.a -output package/libs/ios/libwebgpu_dawn.a",
  );

  console.log("Building libwebgpu_dawn.xcframework");
  $("rm -rf ./package/libs/ios/libwebgpu_dawn.xcframework");
  $(
    "xcodebuild -create-xcframework " +
      "-library ./package/libs/ios/libwebgpu_dawn.a " +
      "-library ./package/libs/ios/arm64_iphoneos/libwebgpu_dawn.a " +
      " -output ./package/libs/ios/libwebgpu_dawn.xcframework ",
  );
  // Build Android
  for (const platform of android.platforms) {
    console.log(`Build Android: ${platform}`);
    await build(`android_${platform}`, {
      ANDROID_ABI: platform,
      ...android.args,
    });
    copyLib("android", platform);
  }

  console.log("Copy headers");
  $("cp -R externals/dawn/out/android_arm64-v8a/gen/include/* package/cpp");
})();
