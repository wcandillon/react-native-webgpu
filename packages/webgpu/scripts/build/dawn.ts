/* eslint-disable max-len */
/* eslint-disable camelcase */

import { chdir } from "process";

import type { Platform } from "./util";
import {
  $,
  build,
  checkBuildArtifacts,
  copyLib,
  libs,
  mapKeys,
  projectRoot,
} from "./util";

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
  DAWN_ENABLE_OPENGLES: "OFF",
  DAWN_ENABLE_DESKTOP_GL: "OFF",
};

const PLATFORM_MAP: Record<string, string> = {
  arm64_iphoneos: "OS64",
  arm64_iphonesimulator: "SIMULATORARM64",
  x86_64_iphonesimulator: "SIMULATOR64",
  arm64_xros: "VISIONOS",
  arm64_xrsimulator: "SIMULATOR_VISIONOS",
  x86_64_xrsimulator: "SIMULATOR64_VISIONOS",
  universal_macosx: "MAC_UNIVERSAL",
};

const android = {
  platforms: ["arm64-v8a", "armeabi-v7a", "x86", "x86_64"] as Platform[],
  args: {
    CMAKE_TOOLCHAIN_FILE: "$ANDROID_NDK/build/cmake/android.toolchain.cmake",
    ANDROID_PLATFORM: "android-26",
    ...commonArgs,
  },
};

const apple = {
  matrix: {
    arm64: ["iphoneos", "iphonesimulator", "xros", "xrsimulator"] as const,
    x86_64: ["iphonesimulator", "xrsimulator"] as const,
    universal: ["macosx"] as const,
  },
  args: {
    CMAKE_TOOLCHAIN_FILE: `${__dirname}/apple.toolchain.cmake`,
    ...commonArgs,
  },
};

(async () => {
  process.chdir("../..");
  process.chdir("externals/dawn");
  $("git reset --hard HEAD");
  $(`git apply ${__dirname}/static_build.patch`);
  process.chdir("../..");

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

  // Build Apple
  for (const platform of mapKeys(apple.matrix)) {
    console.log(`Build Apple: ${platform}`);
    for (const sdk of apple.matrix[platform]) {
      await build(
        `apple_${platform}_${sdk}`,
        {
          PLATFORM: PLATFORM_MAP[`${platform}_${sdk}`],
          ...apple.args,
        },
        `🍏 ${platform} ${sdk}`,
      );
      copyLib("apple", platform, sdk);
    }
  }

  libs.forEach((lib) => {
    console.log(`📱 Building fat binary for iphone simulator: ${lib}`);
    $(
      `lipo -create ${projectRoot}/libs/apple/x86_64_iphonesimulator/${lib}.a ${projectRoot}/libs/apple/arm64_iphonesimulator/${lib}.a -output ${projectRoot}/libs/apple/${lib}.a`,
    );
  });

  libs.forEach((lib) => {
    console.log(`👓 Building fat binary for visionos simulator: ${lib}`);
    $(
      `lipo -create ${projectRoot}/libs/apple/x86_64_xrsimulator/${lib}.a ${projectRoot}/libs/apple/arm64_xrsimulator/${lib}.a -output ${projectRoot}/libs/apple/${lib}_visionos.a`,
    );
  });

  libs.forEach((lib) => {
    console.log(`📱 Building ${lib} for iOS`);
    // iOS
    $(`rm -rf ${projectRoot}/libs/apple/${lib}.xcframework`);
    $(
      "xcodebuild -create-xcframework " +
        `-library ${projectRoot}/libs/apple/${lib}.a ` +
        `-library ${projectRoot}/libs/apple/arm64_iphoneos/${lib}.a ` +
        ` -output ${projectRoot}/libs/apple/${lib}.xcframework `,
    );
    console.log(`👓 Building ${lib} for VisionOS`);
    // VisionOS
    $(`rm -rf ${projectRoot}/libs/apple/${lib}_visionos.xcframework`);
    $(
      "xcodebuild -create-xcframework " +
        `-library ${projectRoot}/libs/apple/${lib}_visionos.a ` +
        `-library ${projectRoot}/libs/apple/arm64_xros/${lib}.a ` +
        ` -output ${projectRoot}/libs/apple/${lib}_visionos.xcframework `,
    );
    console.log(`🖥️ Building ${lib} for macOS`);
    // macOS
    $(
      "xcodebuild -create-xcframework " +
        `-library ${projectRoot}/libs/apple/universal_macosx/${lib}.a ` +
        ` -output ${projectRoot}/libs/apple/${lib}_macosx.xcframework `,
    );
  });

  console.log("Copy headers");
  $(
    `cp -R externals/dawn/out/android_arm64-v8a/gen/include/* ${projectRoot}/cpp`,
  );
  $(`cp externals/dawn/include/webgpu/webgpu_cpp.h ${projectRoot}/cpp/webgpu/`);
  $(
    `cp externals/dawn/include/webgpu/webgpu_enum_class_bitmasks.h ${projectRoot}/cpp/webgpu/`,
  );
  $(`cp externals/dawn/include/webgpu/webgpu.h ${projectRoot}/cpp/webgpu/`);
  $(`cp externals/dawn/src/dawn/dawn.json ${projectRoot}/libs`);
  chdir(projectRoot);
  checkBuildArtifacts();
})();
