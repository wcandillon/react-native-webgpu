module.exports = {
  dependency: {
    platforms: {
      android: {
        componentDescriptors: [
          "WebGPUViewComponentDescriptor",
        ],
        cmakeListsPath: "./jni/CMakeLists.txt"
      },
    },
  },
};
