/* eslint-disable @typescript-eslint/no-explicit-any */
/* eslint-disable no-var */
import type { Server, WebSocket } from "ws";

jest.setTimeout(180 * 1000);

type TestOS = "ios" | "android" | "web" | "node";

declare global {
  var testServer: Server;
  var testClient: WebSocket;
  var testOS: TestOS;
}
export let client: TestingClient;

beforeAll(async () => {
  client = new RemoteTestingClient();
});

export interface EvalContext {
  gpu: GPU;
  [key: string]: any;
}

interface TestingClient {
  eval<Ctx extends EvalContext = EvalContext, R = any>(
    fn: (ctx: Ctx) => R,
    ctx?: Ctx,
  ): Promise<R>;
  OS: TestOS;
  arch: "paper" | "fabric";
}

class RemoteTestingClient implements TestingClient {

  readonly OS = global.testOS;
  readonly arch = global.testArch;

  eval<Ctx extends EvalContext, R>(
    fn: (ctx: Ctx) => any,
    context?: Ctx,
  ): Promise<R> {
    const ctx = this.prepareContext(context);
    const body = { code: fn.toString(), ctx };
    return this.handleResponse<R>(JSON.stringify(body));
  }

  private handleResponse<R>(body: string): Promise<R> {
    return new Promise((resolve) => {
      this.client.once("message", (raw: Buffer) => {
        resolve(JSON.parse(raw.toString()));
      });
      this.client.send(body);
    });
  }

  private get client() {
    if (global.testClient === null) {
      throw new Error("Client is not connected. Did you call init?");
    }
    return global.testClient!;
  }

  private prepareContext<Ctx extends EvalContext>(context?: Ctx): EvalContext {
    const ctx: any = {};
    if (context) {
      for (const [key, value] of Object.entries(context)) {
        ctx[key] = value;
      }
    }
    return ctx;
  }
}
