import _ from "lodash";
import type { MethodSignature, Type } from "ts-morph";

interface JsiMethod {
  async: boolean;
  name: string;
  apiName: string;
  dependencies: string[];
  args: string[];
  argNames: string[];
  returns: string;
  wgpuReturns: string;
}

export const getJSIMethod = (method: MethodSignature): JsiMethod => {
  const async = method.getReturnType().getSymbol()?.getName() === "Promise";
  const name = method.getName();
  const apiName = _.upperFirst(name);
  const returns = getType(method.getReturnType()!);
  const dependencies: string[] = returns.startsWith("GPU") ? [returns] : [];
  const args: string[] = method.getParameters().map((p) => {
    const type = getType(p.getType());
    if (type.startsWith("GPU")) {
      dependencies.push(type);
    }
    return `std::shared_ptr<${type}> ${p.getName()}`;
  });
  const argNames: string[] = method
    .getParameters()
    .map((p) => `${p.getName()}`);
  return {
    async,
    apiName,
    name,
    dependencies,
    args,
    argNames,
    returns,
    wgpuReturns: `wgpu::${returns.substring(3)}`,
  };
};

const getType = (type: Type): string => {
  if (type.isUnion()) {
    return getType(
      type.getUnionTypes().filter((t) => !t.isNull() && !t.isUndefined())[0],
    );
  } else if (type.isNumber()) {
    return "double";
  }
  if (type.getTypeArguments()[0]) {
    return getType(type.getTypeArguments()[0]);
  }
  return type.getText();
};
