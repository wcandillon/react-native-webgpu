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
  it("destroy device (3)", async () => {
    const result = await client.eval(({ adapter }) =>
      adapter.requestDevice({ label: "MyGPU" }).then((device) => {
        device.destroy();
        return device.lost.then((r) => ({
          reason: r.reason,
          message: r.message,
        }));
      }),
    );
    expect(["unknown", "destroyed"].includes(result.reason)).toBe(true);
  });

  it("device lost", async () => {
    const isDeviceLost = await client.eval(({ adapter }) =>
      adapter.requestDevice({ label: "MyGPU" }).then((device) => {
        const timeout = new Promise((resolve) => {
          setTimeout(() => {
            resolve(false);
          }, 50);
        });

        return Promise.race([device.lost.then(() => true), timeout]);
      }),
    );

    expect(isDeviceLost).toBeFalsy();
  });
});
