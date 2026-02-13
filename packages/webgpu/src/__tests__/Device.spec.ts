import { client } from "./setup";

describe("Device", () => {
  it("request device (1)", async () => {
    const result = await client.eval(({ gpu }) =>
      gpu
        .requestAdapter()
        .then((adapter) =>
          adapter!.requestDevice().then((device) => device !== null),
        ),
    );
    expect(result).toBe(true);
  });

  it("request device (2)", async () => {
    const result = await client.eval(({ gpu }) =>
      gpu
        .requestAdapter()
        .then((adapter) =>
          adapter!.requestDevice(undefined).then((device) => device.label),
        ),
    );
    expect(result).toBe("");
  });
  it("request device (3)", async () => {
    const result = await client.eval(({ gpu }) =>
      gpu
        .requestAdapter()
        .then((adapter) =>
          adapter!
            .requestDevice({ label: "MyGPU" })
            .then((device) => device.label),
        ),
    );
    expect(result).toBe("MyGPU");
  });
  it("destroy device (3)", async () => {
    const result = await client.eval(({ gpu }) =>
      gpu.requestAdapter().then((adapter) =>
        adapter!.requestDevice({ label: "MyGPU" }).then((device) => {
          device.destroy();
          return device.lost.then((r) => ({
            reason: r.reason,
            message: r.message,
          }));
        }),
      ),
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
    const result = await client.eval(({ gpu }) =>
      gpu.requestAdapter().then((adapter) =>
        adapter!.requestDevice({ label: "myGPU2" }).then((device) => {
          setTimeout(() => {
            device.destroy();
          }, 50);

          return device.lost.then((r) => ({
            reason: r.reason,
            message: r.message,
          }));
        }),
      ),
    );

    expect(["unknown", "destroyed"].includes(result.reason)).toBeTruthy();
  });

  it("should have addEventListener method", async () => {
    const result = await client.eval(({ device }) => {
      return typeof device.addEventListener === "function";
    });
    expect(result).toBe(true);
  });

  it("should have removeEventListener method", async () => {
    const result = await client.eval(({ device }) => {
      return typeof device.removeEventListener === "function";
    });
    expect(result).toBe(true);
  });

  it("should call addEventListener without throwing", async () => {
    const result = await client.eval(({ device }) => {
      try {
        device.addEventListener("uncapturederror", () => {});
        return true;
      } catch {
        return false;
      }
    });
    expect(result).toBe(true);
  });

  it("should call removeEventListener without throwing", async () => {
    const result = await client.eval(({ device }) => {
      try {
        const listener = () => {};
        device.addEventListener("uncapturederror", listener);
        device.removeEventListener("uncapturederror", listener);
        return true;
      } catch {
        return false;
      }
    });
    expect(result).toBe(true);
  });

  it("should receive uncapturederror event when validation error occurs", async () => {
    const result = await client.eval(({ gpu }) =>
      gpu.requestAdapter().then((adapter) =>
        adapter!.requestDevice().then((device) => {
          return new Promise<{
            received: boolean;
            eventType: string;
            hasError: boolean;
            errorMessage: string;
          }>((resolve) => {
            // Set a timeout in case the event never fires
            const timeout = setTimeout(() => {
              resolve({
                received: false,
                eventType: "",
                hasError: false,
                errorMessage: "",
              });
            }, 1000);

            device.addEventListener(
              "uncapturederror",
              (event: GPUUncapturedErrorEvent) => {
                clearTimeout(timeout);
                resolve({
                  received: true,
                  eventType: event.type,
                  hasError: event.error !== null && event.error !== undefined,
                  errorMessage: event.error?.message ?? "",
                });
              },
            );

            // Create an uncaptured validation error by using invalid parameters
            // Without pushErrorScope, this error becomes uncaptured
            device.createSampler({
              maxAnisotropy: 0, // Invalid: must be at least 1
            });
          });
        }),
      ),
    );

    expect(result.received).toBe(true);
    expect(result.eventType).toBe("uncapturederror");
    expect(result.hasError).toBe(true);
    expect(result.errorMessage.length).toBeGreaterThan(0);
  });
});
