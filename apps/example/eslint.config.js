// eslint.config.js
import wcandillon from "eslint-config-react-native-wcandillon";
import prettierPlugin from "eslint-plugin-prettier";

export default [
  {
    ignores: ["**/*/components/meshes", "**/Pods/**", "node_modules/**", "dist/**", "android/**", "ios/**", "macos/**", "*.config.js"]
  },
  ...wcandillon,
  {
    files: ["**/*.{js,jsx,ts,tsx}"],
    plugins: {
      prettier: prettierPlugin
    },
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