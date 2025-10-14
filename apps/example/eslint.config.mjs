// eslint.config.js
import wcandillon from "eslint-config-react-native-wcandillon";

export default [
  {
    ignores: ["**/*/components/meshes", "**/Pods/**", "node_modules/**", "dist/**", "android/**", "ios/**", "macos/**", "*.config.js"]
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
