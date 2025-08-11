import * as vscode from "vscode";

// Safe config wrappers that always return a string or string array
function getString(key: string): string {
  return vscode.workspace.getConfiguration().get("dejaview." + key, "");
}

// Convenience functions to extract configs
export function tracePath(): string {
  return getString("trace");
}
