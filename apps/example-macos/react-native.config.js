const { configureProjects } = require("react-native-test-app");

// Keep native dependencies and project generation inside the desktop runner.
module.exports = {
  project: configureProjects({ macos: { sourceDir: "macos" } }),
};
