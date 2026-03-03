import type { GatewayClient } from "./gateway.js";

let _gateway: GatewayClient | null = null;

export function setGateway(gw: GatewayClient): void {
  _gateway = gw;
}

export function getGateway(): GatewayClient | null {
  return _gateway;
}
