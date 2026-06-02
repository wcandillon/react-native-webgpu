import { client } from "./setup";

describe("Dawn toggles", () => {
  it("requests a device with enabled and disabled dawnToggles", async () => {
    const result = await client.eval(({ gpu }) => {
      return gpu.requestAdapter().then((adapter) =>
        adapter!
          .requestDevice({
            dawnToggles: {
              enabledToggles: ["disable_symbol_renaming"],
              disabledToggles: ["lazy_clear_resource_on_first_use"],
            },
          })
          .then((device) => !!device),
      );
    });
    expect(result).toBe(true);
  });

  it("requests a device with no dawnToggles (unchanged behavior)", async () => {
    const result = await client.eval(({ gpu }) => {
      return gpu
        .requestAdapter()
        .then((adapter) => adapter!.requestDevice().then((device) => !!device));
    });
    expect(result).toBe(true);
  });

  it("ignores unknown toggle names without failing device creation", async () => {
    const result = await client.eval(({ gpu }) => {
      return gpu.requestAdapter().then((adapter) =>
        adapter!
          .requestDevice({
            dawnToggles: { enabledToggles: ["this_toggle_does_not_exist"] },
          })
          .then((device) => !!device),
      );
    });
    expect(result).toBe(true);
  });
});
