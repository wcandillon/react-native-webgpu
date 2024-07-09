import { client } from "./setup";

describe("Device", () => {
  it("request device (1)", async () => {
    const result = await client.eval(({ adapter }) =>
      adapter.requestDevice().then((device) => device !== null),
    );
    expect(result).toBe(true);
  });

  it("request device (2)", async () => {
    const result = await client.eval(({ adapter }) =>
      adapter.requestDevice(undefined).then((device) => device.label),
    );
    expect(result).toBe("");
  });
  it("request device (3)", async () => {
    const result = await client.eval(({ adapter }) =>
      adapter.requestDevice({ label: "MyGPU" }).then((device) => device.label),
    );
    expect(result).toBe("MyGPU");
  });
});
