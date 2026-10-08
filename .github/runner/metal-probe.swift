// Probe whether Metal is usable where this runs: on a GitHub-hosted macOS
// runner (macOS VM) and inside the iOS simulator on that runner. It creates
// the default device, runs a compute kernel and a render pass, and checks the
// results on the CPU. Exit code 0 means Metal works end to end.
import Foundation
import Metal

func fail(_ message: String) -> Never {
    print("FAIL: \(message)")
    exit(1)
}

guard let device = MTLCreateSystemDefaultDevice() else {
    fail("MTLCreateSystemDefaultDevice() returned nil: no Metal device")
}

print("device: \(device.name)")
#if os(macOS)
print("all devices: \(MTLCopyAllDevices().map { $0.name })")
print("lowPower: \(device.isLowPower) headless: \(device.isHeadless) removable: \(device.isRemovable)")
#endif
print("families: apple7=\(device.supportsFamily(.apple7)) apple8=\(device.supportsFamily(.apple8)) metal3=\(device.supportsFamily(.metal3)) common3=\(device.supportsFamily(.common3))")
print("maxThreadsPerThreadgroup: \(device.maxThreadsPerThreadgroup)")
print("maxBufferLength: \(device.maxBufferLength)")
print("argumentBuffersSupport tier: \(device.argumentBuffersSupport.rawValue)")
print("unifiedMemory: \(device.hasUnifiedMemory)")

let source = """
#include <metal_stdlib>
using namespace metal;

kernel void square(device const float* in [[buffer(0)]],
                   device float* out [[buffer(1)]],
                   uint id [[thread_position_in_grid]]) {
  out[id] = in[id] * in[id];
}

struct VSOut {
  float4 position [[position]];
};

vertex VSOut vs(uint vid [[vertex_id]]) {
  // One triangle that covers the whole clip space.
  float2 p[3] = { float2(-1, -1), float2(3, -1), float2(-1, 3) };
  VSOut o;
  o.position = float4(p[vid], 0, 1);
  return o;
}

fragment float4 fs() {
  return float4(0, 1, 0, 1);
}
"""

do {
    let library = try device.makeLibrary(source: source, options: nil)
    guard let queue = device.makeCommandQueue() else { fail("no command queue") }

    // Compute
    guard let squareFn = library.makeFunction(name: "square") else { fail("no kernel") }
    let pipeline = try device.makeComputePipelineState(function: squareFn)
    let count = 4096
    var input = (0..<count).map { Float($0) }
    guard let inBuf = device.makeBuffer(bytes: &input, length: count * 4, options: .storageModeShared),
          let outBuf = device.makeBuffer(length: count * 4, options: .storageModeShared) else {
        fail("no buffers")
    }
    let cmd = queue.makeCommandBuffer()!
    let enc = cmd.makeComputeCommandEncoder()!
    enc.setComputePipelineState(pipeline)
    enc.setBuffer(inBuf, offset: 0, index: 0)
    enc.setBuffer(outBuf, offset: 0, index: 1)
    let tg = min(pipeline.maxTotalThreadsPerThreadgroup, count)
    enc.dispatchThreads(MTLSize(width: count, height: 1, depth: 1),
                        threadsPerThreadgroup: MTLSize(width: tg, height: 1, depth: 1))
    enc.endEncoding()
    cmd.commit()
    cmd.waitUntilCompleted()
    if let error = cmd.error { fail("compute command buffer: \(error)") }
    let out = outBuf.contents().bindMemory(to: Float.self, capacity: count)
    let computeOK = (0..<count).allSatisfy { out[$0] == Float($0) * Float($0) }
    print("compute kernel: \(computeOK ? "correct" : "WRONG RESULT")")
    if !computeOK { exit(1) }

    // Render
    let width = 64, height = 64
    let desc = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .bgra8Unorm,
                                                        width: width, height: height,
                                                        mipmapped: false)
    desc.usage = [.renderTarget, .shaderRead]
    desc.storageMode = .private
    guard let target = device.makeTexture(descriptor: desc) else { fail("no render target") }
    let pipeDesc = MTLRenderPipelineDescriptor()
    pipeDesc.vertexFunction = library.makeFunction(name: "vs")
    pipeDesc.fragmentFunction = library.makeFunction(name: "fs")
    pipeDesc.colorAttachments[0].pixelFormat = .bgra8Unorm
    let renderPipeline = try device.makeRenderPipelineState(descriptor: pipeDesc)
    let pass = MTLRenderPassDescriptor()
    pass.colorAttachments[0].texture = target
    pass.colorAttachments[0].loadAction = .clear
    pass.colorAttachments[0].storeAction = .store
    pass.colorAttachments[0].clearColor = MTLClearColor(red: 1, green: 0, blue: 0, alpha: 1)
    let bytesPerRow = width * 4
    guard let readback = device.makeBuffer(length: bytesPerRow * height, options: .storageModeShared) else {
        fail("no readback buffer")
    }
    let rcmd = queue.makeCommandBuffer()!
    let renc = rcmd.makeRenderCommandEncoder(descriptor: pass)!
    renc.setRenderPipelineState(renderPipeline)
    renc.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: 3)
    renc.endEncoding()
    let blit = rcmd.makeBlitCommandEncoder()!
    blit.copy(from: target, sourceSlice: 0, sourceLevel: 0,
              sourceOrigin: MTLOrigin(x: 0, y: 0, z: 0),
              sourceSize: MTLSize(width: width, height: height, depth: 1),
              to: readback, destinationOffset: 0,
              destinationBytesPerRow: bytesPerRow, destinationBytesPerImage: bytesPerRow * height)
    blit.endEncoding()
    rcmd.commit()
    rcmd.waitUntilCompleted()
    if let error = rcmd.error { fail("render command buffer: \(error)") }
    let px = readback.contents().bindMemory(to: UInt8.self, capacity: bytesPerRow * height)
    // BGRA: a fully covered green target reads as 0, 255, 0, 255.
    let renderOK = (0..<(width * height)).allSatisfy { i in
        px[i * 4] == 0 && px[i * 4 + 1] == 255 && px[i * 4 + 2] == 0 && px[i * 4 + 3] == 255
    }
    print("render pass: \(renderOK ? "correct" : "WRONG RESULT") (pixel 0 = \(px[0]),\(px[1]),\(px[2]),\(px[3]))")
    if !renderOK { exit(1) }
    print("OK: Metal compute and render work here")
} catch {
    fail("\(error)")
}
