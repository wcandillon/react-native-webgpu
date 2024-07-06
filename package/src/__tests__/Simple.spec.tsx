import { client } from "./setup";

describe("Simple", () => {
  it("should render", async () => {
    const result = await client.eval(() => 1 + 1);
    expect(result).toBe(2);
  });
});
