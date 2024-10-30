const fs = require("fs");
const path = require("path");
const Cheerio = require("cheerio");

const schemePath = path.join(
  __dirname,
  "../ios/example.xcodeproj/xcshareddata/xcschemes/example.xcscheme"
);

if (!fs.existsSync(schemePath)) {
  throw new Error(
    "Must call disable-metal-validation.js only after prebuilding the ios folder!"
  );
}

const $ = Cheerio.load(fs.readFileSync(schemePath).toString(), { xml: true });

$('LaunchAction[buildConfiguration="Debug"]').attr(
  "enableGPUValidationMode",
  "1"
);

fs.writeFileSync(schemePath, $.xml());
console.info("Disabled metal validation API");

fs.writeFileSync(
  path.join(__dirname, "../ios/.xcode.env.local"),
  //Yarn can make the wrong node path appear here
  "export NODE_BINARY=$(command -v node)\n"
);
