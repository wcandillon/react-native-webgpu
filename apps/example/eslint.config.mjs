// eslint.config.js
import wcandillon from "eslint-config-react-native-wcandillon";

export default [
  {
    ignores: ["**/*/components/meshes", "**/Pods/**", "node_modules/**", "dist/**", "android/**", "ios/**", "macos/**", "*.config.js",
      // Dormant Skia demos, excluded from tsconfig.json until React Native Skia
      // supports SDK-based Dawn preparation. Remove both entries together.
      "src/MNISTInference/**", "src/IconBenchmark/**"]
  },
  ...wcandillon,
  {
    files: ["**/*.{js,jsx,ts,tsx}"],
    languageOptions: {
      parserOptions: {
        ecmaVersion: "latest",
        sourceType: "module"
      }
    },
    rules: {
      "no-bitwise": "off",
      "@typescript-eslint/no-require-imports": "off",
      "import/extensions": "off",
      "prettier/prettier": [
        "error",
        {
          "trailingComma": "all"
        }
      ]
    }
  }
];
