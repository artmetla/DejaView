import * as child_process from "child_process";
import * as vscode from "vscode";

// Convenience function to run a command in the current workspace
export async function runCommand(cmd: string, args: string[]): Promise<string> {
  const workspaceFolders = vscode.workspace.workspaceFolders;
  if (!workspaceFolders || !workspaceFolders.length) {
    vscode.window.showErrorMessage("No root workspace");
    return "";
  }

  let child = child_process.spawn(cmd, args, { cwd: workspaceFolders[0].uri.path });
  let stdoutChunks: string[] = [];
  await new Promise((resolve) => {
    child.stdout.on("data", function (chunk) {
      stdoutChunks.push(chunk.toString());
    });
    child.on("close", resolve);
  });

  return stdoutChunks.join("");
}
