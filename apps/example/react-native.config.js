const project = (() => {
  try {
    const { configureProjects } = require("react-native-test-app");
    return configureProjects({
      android: {
        sourceDir: "android",
        packageName: "com.microsoft.reacttestapp"
      },
      ios: {
        sourceDir: "ios",
      },
      windows: {
        sourceDir: "windows",
        solutionFile: "windows/example.sln",
      },
    });
  } catch (_) {
    return undefined;
  }
})();

// Workaround: react-native-nitro-image's NitroImagePackage extends
// BaseReactPackage, which @react-native-community/cli@13's autolinking regex
// does not detect. Without this override, autolinking returns android: null
// for nitro-image and vision-camera's gradle build fails with
// "Project with path ':react-native-nitro-image' could not be found".
const path = require("path");

module.exports = {
  ...(project ? { project } : undefined),
  dependencies: {
    "react-native-nitro-image": {
      platforms: {
        android: {
          sourceDir: path.join(
            __dirname,
            "../../node_modules/react-native-nitro-image/android",
          ),
          packageImportPath: "import com.margelo.nitro.image.NitroImagePackage;",
          packageInstance: "new NitroImagePackage()",
        },
      },
    },
  },
};