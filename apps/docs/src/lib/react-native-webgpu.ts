// Re-export from source so Turbopack/webpack resolve via relative imports
// instead of a Windows absolute-path alias (unsupported in Turbopack dev).
export * from "../../../../packages/webgpu/src/index";
