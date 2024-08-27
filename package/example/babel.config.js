const path = require("path");
const pak = require("../package.json");

module.exports = {
  presets: ["module:@react-native/babel-preset"],
  plugins: [
    [
      "module-resolver",
      {
        extensions: [".tsx", ".ts", ".js", ".json"],
        alias: {
          [pak.name]: path.join(__dirname, "..", pak.source),
          "@react-three/fiber/native": [
            __dirname,
            "build/react-three-fiber/packages/fiber/native",
          ],
        },
      },
    ],
  ],
};
