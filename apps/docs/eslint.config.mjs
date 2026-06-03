// @ts-check
import wcandillon from "eslint-config-react-native-wcandillon";

export default [
  {
    ignores: [
      "node_modules/**",
      "build/**",
      ".docusaurus/**",
      "*.config.js",
    ],
  },
  ...wcandillon,
  {
    files: ["**/*.{js,jsx,ts,tsx}"],
    languageOptions: {
      parserOptions: {
        ecmaVersion: "latest",
        sourceType: "module",
      },
    },
    rules: {
      "no-bitwise": "off",
      "@typescript-eslint/no-require-imports": "off",
      "import/extensions": "off",
      "import/no-unresolved": "off",
      "prettier/prettier": [
        "error",
        {
          trailingComma: "all",
        },
      ],
    },
  },
  {
    // Docusaurus requires default exports for its config, sidebars, pages,
    // and swizzled theme components.
    files: [
      "docusaurus.config.ts",
      "sidebars.ts",
      "src/pages/**/*.{ts,tsx}",
      "src/theme/**/*.{ts,tsx}",
    ],
    rules: {
      "import/no-default-export": "off",
    },
  },
];
