/* eslint-disable max-len */
import { chdir } from "process";

import type { Platform } from "./util";
import { $, build, checkBuildArtifacts, copyLib } from "./util";

const PATH = `PATH=${__dirname}/../../../externals/depot_tools/:$PATH`;

const commonArgs = {
  CMAKE_BUILD_TYPE: "Release",
  BUILD_SHARED_LIBS: "OFF",
};

const android = {
  platforms: ["arm64-v8a", "armeabi-v7a", "x86", "x86_64"] as Platform[],
  args: {
    CMAKE_TOOLCHAIN_FILE: "$ANDROID_NDK/build/cmake/android.toolchain.cmake",
    ANDROID_PLATFORM: "android-26",
    ...commonArgs,
  },
};

//type BuildMatrix = Record<Platform, SDK[]>;

// const ios = {
//   matrix: {
//     arm64: ["iphoneos", "iphonesimulator"],
//     x86_64: ["iphonesimulator"],
//   } as BuildMatrix,
//   args: {
//     CMAKE_SYSTEM_NAME: "iOS",
//     CMAKE_OSX_DEPLOYMENT_TARGET: "13.0",
//     ...commonArgs,
//   },
// };

(async () => {
  // Fetch dependencies
  console.log("gclient sync");
  process.chdir("../externals/dawn");
  $("cp scripts/standalone.gclient .gclient");
  $(`${PATH} gclient sync`);
  process.chdir("../..");
  console.log("gclient sync done");

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

  // // Build iOS
  // for (const platform of mapKeys(ios.matrix)) {
  //   console.log(`Build iOS: ${platform}`);
  //   for (const sdk of ios.matrix[platform]) {
  //     await build(
  //       `ios_${platform}_${sdk}`,
  //       {
  //         CMAKE_OSX_ARCHITECTURES: platform,
  //         CMAKE_OSX_SYSROOT: `$(xcrun --sdk ${sdk} --show-sdk-path)`,
  //         ...ios.args,
  //       },
  //       `🍏 ${platform} - ${sdk}`,
  //     );
  //     copyLib("ios", platform, sdk);
  //   }
  // }

  // libs.forEach((lib) => {
  //   console.log(`Building fat binary for iphone simulator: ${lib}`);
  //   $(
  //     `lipo -create package/libs/ios/x86_64_iphonesimulator/${lib}.a package/libs/ios/arm64_iphonesimulator/${lib}.a -output package/libs/ios/${lib}.a`,
  //   );
  // });

  // libs.forEach((lib) => {
  //   console.log(`Building ${lib}`);
  //   $(`rm -rf ./package/libs/ios/${lib}.xcframework`);
  //   $(
  //     "xcodebuild -create-xcframework " +
  //       `-library ./package/libs/ios/${lib}.a ` +
  //       `-library ./package/libs/ios/arm64_iphoneos/${lib}.a ` +
  //       ` -output ./package/libs/ios/${lib}.xcframework `,
  //   );
  // });

  console.log("Copy headers");
  $(
    "cp -R package/scripts/build/out/android_arm64-v8a/dawn/gen/include/* package/cpp",
  );
  $("cp externals/dawn/include/webgpu/webgpu_cpp.h package/cpp/webgpu/");
  $(
    "cp externals/dawn/include/webgpu/webgpu_enum_class_bitmasks.h package/cpp/webgpu/",
  );
  $("cp externals/dawn/include/webgpu/webgpu.h package/cpp/webgpu/");
  chdir("package");
  checkBuildArtifacts();
})();