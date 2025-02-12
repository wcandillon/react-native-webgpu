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

  it("times out device.lost if the device has not been destroyed", async () => {
    const isDeviceLost = await client.eval(({ device }) => {
      const timeout = new Promise((resolve) => {
        setTimeout(() => {
          resolve(false);
        }, 50);
      });

      return Promise.race([device.lost.then(() => true), timeout]);
    });

    expect(isDeviceLost).toBeFalsy();
  });

  it("resolves an awaited device.lost when device.destroy is called", async () => {
    const result = await client.eval(({ adapter }) =>
      adapter.requestDevice({ label: "myGPU2" }).then((device) => {
        setTimeout(() => {
          device.destroy();
        }, 50);

        return device.lost.then((r) => ({
          reason: r.reason,
          message: r.message,
        }));
      }),
    );

    expect(["unknown", "destroyed"].includes(result.reason)).toBeTruthy();
  });
});
