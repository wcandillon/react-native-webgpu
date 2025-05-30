import * as fs from "fs";
import * as path from "path";
import { execSync } from "child_process";

const PACKAGE_ROOT = path.join(__dirname, "..");
const DAWN_BUILD_ZIP = path.join(PACKAGE_ROOT, "dawn-build.zip");
const LIBS_DIR = path.join(PACKAGE_ROOT, "libs");
const CPP_DIR = path.join(PACKAGE_ROOT, "cpp");

function ensureDirectoryExists(dir: string) {
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }
}

function cleanupExistingArtifacts() {
  console.log("🧹 Cleaning up existing Dawn artifacts...");
  
  // Clean up existing libs directory
  if (fs.existsSync(LIBS_DIR)) {
    execSync(`rm -rf "${LIBS_DIR}"`);
    console.log("✅ Existing libs directory cleaned");
  }
  
  // Clean up Dawn-related headers in cpp directory
  const dawnDirs = ["dawn", "webgpu"];
  for (const dirName of dawnDirs) {
    const dirPath = path.join(CPP_DIR, dirName);
    if (fs.existsSync(dirPath)) {
      execSync(`rm -rf "${dirPath}"`);
      console.log(`✅ Existing ${dirName} headers cleaned`);
    }
  }
}

function extractDawnBuild() {
  console.log("Installing Dawn build artifacts...");

  if (!fs.existsSync(DAWN_BUILD_ZIP)) {
    console.error(`❌ Dawn build zip not found at: ${DAWN_BUILD_ZIP}`);
    process.exit(1);
  }

  // Clean up existing artifacts first
  cleanupExistingArtifacts();

  // Create temporary extraction directory
  const tempDir = path.join(PACKAGE_ROOT, "temp-dawn-extract");
  ensureDirectoryExists(tempDir);

  try {
    // Extract the zip file
    console.log("🔄 Extracting dawn-build.zip...");
    execSync(`cd "${tempDir}" && unzip -q "${DAWN_BUILD_ZIP}"`);

    // Check if libs directory exists in extracted content
    const extractedLibsDir = path.join(tempDir, "libs");
    if (fs.existsSync(extractedLibsDir)) {
      console.log("📦 Installing libraries...");
      ensureDirectoryExists(LIBS_DIR);
      
      // Copy libs directory contents
      execSync(`cp -R "${extractedLibsDir}"/* "${LIBS_DIR}"/`);
      console.log("✅ Libraries installed successfully");
    }

    // Check for different possible header directory structures
    const possibleHeaderDirs = [
      path.join(tempDir, "headers"),
      path.join(tempDir, "cpp"),
      path.join(tempDir, "include"),
      tempDir // Headers might be directly in the root
    ];
    
    let headersInstalled = false;
    
    for (const headerDir of possibleHeaderDirs) {
      // Check if this directory contains webgpu or dawn subdirectories
      const webgpuDir = path.join(headerDir, "webgpu");
      const dawnDir = path.join(headerDir, "dawn");
      
      if (fs.existsSync(webgpuDir) || fs.existsSync(dawnDir)) {
        console.log(`📄 Installing headers from ${path.relative(tempDir, headerDir)}...`);
        ensureDirectoryExists(CPP_DIR);
        
        // Copy webgpu headers if they exist
        if (fs.existsSync(webgpuDir)) {
          const targetWebgpuDir = path.join(CPP_DIR, "webgpu");
          ensureDirectoryExists(targetWebgpuDir);
          execSync(`cp -R "${webgpuDir}"/* "${targetWebgpuDir}"/`);
          console.log("✅ WebGPU headers installed");
        }
        
        // Copy dawn headers if they exist
        if (fs.existsSync(dawnDir)) {
          const targetDawnDir = path.join(CPP_DIR, "dawn");
          ensureDirectoryExists(targetDawnDir);
          execSync(`cp -R "${dawnDir}"/* "${targetDawnDir}"/`);
          console.log("✅ Dawn headers installed");
        }
        
        headersInstalled = true;
        break;
      }
    }
    
    if (!headersInstalled) {
      console.warn("⚠️  No webgpu or dawn headers found in extracted content");
    }

    // Clean up temporary directory
    execSync(`rm -rf "${tempDir}"`);
    
    console.log("🎉 Dawn build installation completed successfully!");

  } catch (error) {
    console.error("❌ Error during installation:", error);
    
    // Clean up on error
    if (fs.existsSync(tempDir)) {
      execSync(`rm -rf "${tempDir}"`);
    }
    
    process.exit(1);
  }
}

extractDawnBuild();