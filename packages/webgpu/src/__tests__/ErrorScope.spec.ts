import { client } from "./setup";

describe("Error Scope", () => {
  it("should capture validation error when creating sampler with invalid maxAnisotropy", async () => {
    const result = await client.eval(({ device }) => {
      device.pushErrorScope("validation");
      device.createSampler({
        maxAnisotropy: 0, // Invalid, maxAnisotropy must be at least 1.
      });
      return device.popErrorScope().then((error) => {
        if (error) {
          return {
            hasError: true,
            message: error.message,
            messageLength: error.message.length,
            messageNotEmpty: error.message.length > 0,
          };
        } else {
          return {
            hasError: false,
            message: "",
            messageLength: 0,
            messageNotEmpty: false,
          };
        }
      });
    });

    expect(result.hasError).toBe(true);
    expect(result.messageNotEmpty).toBe(true);
    expect(result.messageLength).toBeGreaterThan(0);
  });
  it("should capture and return error messages from popErrorScope", async () => {
    const result = await client.eval(({ device }) => {
      // Invalid WGSL shader with syntax error (missing closing parenthesis)
      const invalidShaderWGSL = `@fragment
  fn main() -> @location(0) vec4f {
    return vec4(1.0, 0.0, 0.0, 1.0;
  }`;
      device.pushErrorScope("validation");
      // This should generate a validation error due to syntax error
      device.createShaderModule({
        code: invalidShaderWGSL,
      });
      return device.popErrorScope().then((error) => {
        if (error) {
          return {
            hasError: true,
            message: error.message,
            messageLength: error.message.length,
            messageNotEmpty: error.message.length > 0,
            messageContainsExpected:
              error.message.includes("expected") ||
              error.message.includes("error") ||
              error.message.includes("parsing"),
          };
        } else {
          return {
            hasError: false,
            message: "",
            messageLength: 0,
            messageNotEmpty: false,
            messageContainsExpected: false,
          };
        }
      });
    });
    expect(result.hasError).toBe(true);
    expect(result.messageNotEmpty).toBe(true);
    expect(result.messageLength).toBeGreaterThan(0);
    // The error message should contain some indication that it's a parsing error
    expect(result.messageContainsExpected).toBe(true);
  });

  it("should return null when no error occurs", async () => {
    const result = await client.eval(({ device, shaders: { redFragWGSL } }) => {
      device.pushErrorScope("validation");
      // This should not generate any errors
      device.createShaderModule({
        code: redFragWGSL,
      });
      return device.popErrorScope().then((error) => {
        return {
          hasError: error !== null,
          error: error,
        };
      });
    });
    expect(result.hasError).toBe(false);
    expect(result.error).toBe(null);
  });
});
