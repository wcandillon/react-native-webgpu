
//import { mat4, vec3 } from "wgpu-matrix";
import { RNCanvasContext } from "react-native-wgpu";
import { cubePositionOffset, cubeUVOffset, cubeVertexArray, cubeVertexCount, cubeVertexSize } from "../components/cube";
import { basicVertWGSL, vertexPositionColorWGSL } from "./Shaders";
import { mat4, vec3 } from "wgpu-matrix";
import { SharedValue } from "react-native-reanimated";

export class CubeScene {
  private device: GPUDevice | null = null;
  private adapter: GPUAdapter | null = null;
  private renderPassDescriptor: GPURenderPassDescriptor | null = null;
  private pipeline: GPURenderPipeline | null = null;
  private uniformBuffer: GPUBuffer | null = null;
  private uniformBindGroup: GPUBindGroup | null = null;
  private verticesBuffer: GPUBuffer | null = null;
  private context: RNCanvasContext;

  constructor(context: RNCanvasContext) {
    this.context = context;
  }

  async init() {
    this.adapter = await navigator.gpu.requestAdapter();
    if (!this.adapter) {
      throw new Error("Failed to request adapter");
    }
    this.device = await this.adapter.requestDevice();
    if (!this.device) {
      throw new Error("Failed to request device");
    }
    const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
    this.context.configure({
      device: this.device,
      format: presentationFormat,
      alphaMode: "premultiplied",
    });
    const { canvas } = this.context;
    this.verticesBuffer = this.device.createBuffer({
        size: cubeVertexArray.byteLength,
        usage: GPUBufferUsage.VERTEX,
        mappedAtCreation: true,
      });
      new Float32Array(this.verticesBuffer.getMappedRange()).set(cubeVertexArray);
      this.verticesBuffer.unmap();
  
    this.pipeline = this.device.createRenderPipeline({
        layout: "auto",
        vertex: {
          module: this.device.createShaderModule({
            code: basicVertWGSL,
          }),
          buffers: [
            {
              arrayStride: cubeVertexSize,
              attributes: [
                {
                  // position
                  shaderLocation: 0,
                  offset: cubePositionOffset,
                  format: "float32x4",
                },
                {
                  // uv
                  shaderLocation: 1,
                  offset: cubeUVOffset,
                  format: "float32x2",
                },
              ],
            },
          ],
        },
        fragment: {
          module: this.device.createShaderModule({
            code: vertexPositionColorWGSL,
          }),
          targets: [
            {
              format: presentationFormat,
            },
          ],
        },
        primitive: {
          topology: "triangle-list",
  
          // Backface culling since the cube is solid piece of geometry.
          // Faces pointing away from the camera will be occluded by faces
          // pointing toward the camera.
          cullMode: "back",
        },
  
        // Enable depth testing so that the fragment closest to the camera
        // is rendered in front.
        depthStencil: {
          depthWriteEnabled: true,
          depthCompare: "less",
          format: "depth24plus",
        },
      });
  
      const depthTexture = this.device.createTexture({
        size: [canvas.width, canvas.height],
        format: "depth24plus",
        usage: GPUTextureUsage.RENDER_ATTACHMENT,
      });
  
      const uniformBufferSize = 4 * 16; // 4x4 matrix
      this.uniformBuffer = this.device.createBuffer({
        size: uniformBufferSize,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
      });
  
      this.uniformBindGroup = this.device.createBindGroup({
        layout: this.pipeline.getBindGroupLayout(0),
        entries: [
          {
            binding: 0,
            resource: {
              buffer: this.uniformBuffer,
            },
          },
        ],
      });
  
      this.renderPassDescriptor = {
        // @ts-expect-error
        colorAttachments: [
          {
            view: undefined, // Assigned later
            clearValue: [0, 0, 0, 0],
            loadOp: "clear",
            storeOp: "store",
          },
        ],
        depthStencilAttachment: {
          view: depthTexture.createView(),
  
          depthClearValue: 1.0,
          depthLoadOp: "clear",
          depthStoreOp: "store",
        },
      };
  }

  render(rotateX: SharedValue<number>, rotateY: SharedValue<number>) {
    const aspect = this.context.canvas.width / this.context.canvas.height;
    const projectionMatrix = mat4.perspective(
      (2 * Math.PI) / 5,
      aspect,
      1,
      100.0,
    );
    const modelViewProjectionMatrix = mat4.create();
    function getTransformationMatrix() {
        const viewMatrix = mat4.identity();
        mat4.translate(viewMatrix, vec3.fromValues(0, 0, -4), viewMatrix);
        mat4.rotate(
          viewMatrix,
          vec3.fromValues(1, 0, 0),
          rotateX.value,
          viewMatrix,
        );
        mat4.rotate(
          viewMatrix,
          vec3.fromValues(0, 1, 0),
          rotateY.value,
          viewMatrix,
        );
        mat4.multiply(projectionMatrix, viewMatrix, modelViewProjectionMatrix);
  
        return modelViewProjectionMatrix;
      }
  
        const transformationMatrix = getTransformationMatrix();
        if (!this.device) {
            throw new Error("Device not initialized");
        }
        if (!this.uniformBuffer) {
            throw new Error("Uniform buffer not initialized");
        }
        if (!this.renderPassDescriptor) {
            throw new Error("Render pass descriptor not initialized");
        }
        if (!this.pipeline) {
            throw new Error("Pipeline not initialized");
        }
        if (!this.verticesBuffer) {
            throw new Error("Vertices buffer not initialized");
        }
        this.device.queue.writeBuffer(
          this.uniformBuffer,
          0,
          transformationMatrix.buffer,
          transformationMatrix.byteOffset,
          transformationMatrix.byteLength,
        );
        // @ts-expect-error
        this.renderPassDescriptor.colorAttachments[0].view = this.context
          .getCurrentTexture()
          .createView();
  
        const commandEncoder = this.device.createCommandEncoder();
        const passEncoder = commandEncoder.beginRenderPass(this.renderPassDescriptor);
        passEncoder.setPipeline(this.pipeline);
        passEncoder.setBindGroup(0, this.uniformBindGroup);
        passEncoder.setVertexBuffer(0, this.verticesBuffer);
        passEncoder.draw(cubeVertexCount);
        passEncoder.end();
        this.device.queue.submit([commandEncoder.finish()]);
        this.context.present();
  }
}
