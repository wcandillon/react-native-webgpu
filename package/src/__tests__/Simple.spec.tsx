import { client } from "./setup";

describe("Test e2e settings", () => {
  it("execute a simple function", async () => {
    const result = await client.eval(() => 1 + 1);
    expect(result).toBe(2);
  });
});
