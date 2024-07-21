import _ from "lodash";

import dawn from "../../../libs/dawn.json";

export const mapKeys = <T extends object>(obj: T) =>
  Object.keys(obj) as (keyof T)[];

export const hasPropery = <O, T extends string>(
  object: unknown,
  property: T,
): object is O & Record<T, unknown> => {
  return typeof object === "object" && object !== null && property in object;
};

export const resolved: Record<
  string,
  {
    extra?: string;
    ctor?: string;
  }
> = {
  GPU: {
    ctor: `GPU()
      : HybridObject("GPU")  {
          wgpu::InstanceDescriptor instanceDesc;
          instanceDesc.features.timedWaitAnyEnable = true;
          instanceDesc.features.timedWaitAnyMaxCount = 64;
          _instance = wgpu::CreateInstance(&instanceDesc);
          auto instance = &_instance;
          _async = std::make_shared<AsyncRunner>(instance);
      }`,
  },
  GPUBuffer: {
    extra: `struct Mapping {
      uint64_t start;
      uint64_t end;
      inline bool Intersects(uint64_t s, uint64_t e) const { return s < end && e > start; }
      std::shared_ptr<ArrayBuffer> buffer;
  };
  std::vector<Mapping> mappings;
  `,
  },
};

const toNativeName = (name: string) => {
  return _.upperFirst(_.camelCase(name));
};

export const resolveNative = (
  className: string,
  methodName: string,
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
): any => {
  const key = Object.keys(dawn).find(
    (k) => toNativeName(k) === className.substring(3),
  );
  const object = dawn[key as keyof typeof dawn];
  if (!object) {
    return null;
  }
  if (!hasPropery(object, "methods")) {
    return null;
  }
  return object.methods.find((m) => {
    return _.camelCase(m.name.toLowerCase()) === methodName;
  });
};

export const resolveExtra = (className: string) => {
  return resolved[className]?.extra ?? "";
};

export const resolveCtor = (className: string): string | undefined => {
  return resolved[className]?.ctor;
};
