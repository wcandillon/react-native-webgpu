// WebGPU's GPUObjectBase mixin. Specs extend it so nitrogen flattens `label`
// into every GPU object.
export interface GPUObjectBase {
  label: string;
}
