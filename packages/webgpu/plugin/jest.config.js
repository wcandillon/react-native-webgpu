// Standalone config for the Expo config plugin unit tests. The package's main
// jest config drives the e2e harness (it boots a WebSocket server and waits
// for the example app), which the plugin tests must not depend on.
module.exports = {
  rootDir: __dirname,
  testEnvironment: "node",
  testMatch: ["<rootDir>/src/__tests__/**/*.test.ts"],
  transform: {
    "^.+\\.(js|ts)$": [
      "babel-jest",
      {
        presets: [
          [require.resolve("@babel/preset-env"), { targets: { node: "current" } }],
          require.resolve("@babel/preset-typescript"),
        ],
      },
    ],
  },
};
