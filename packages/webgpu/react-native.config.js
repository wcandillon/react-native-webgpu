module.exports = {
  dependency: {
    platforms: {
      android: {
        componentDescriptors: [
          "WebGPUViewComponentDescriptor",
        ],
        cmakeListsPath: "./codegen/CMakeLists.txt"
      },
    },
  },
};
