import * as vscode from "vscode";
import { getUri } from "./utilities/getUri";
import { VERSION } from "./version";
import { spawn, ChildProcess } from 'child_process';
import * as path from 'path';
import * as net from 'net';

class UiDocument implements vscode.CustomDocument {
    public readonly uri: vscode.Uri;
    constructor(uri: vscode.Uri) {
        this.uri = uri;
    }
    dispose(): void {}
}

export class UiProvider implements vscode.CustomReadonlyEditorProvider<UiDocument> {
    public static register(context: vscode.ExtensionContext): vscode.Disposable {
        return vscode.window.registerCustomEditorProvider(
            'dejaview.ui',
            new UiProvider(context),
            {
                webviewOptions: {
                    retainContextWhenHidden: true,
                },
            }
        );
    }

    // Manages all webview panels, keyed by the document URI string.
    private readonly panels = new Map<string, Set<vscode.WebviewPanel>>();
    // Manages the backend process and its port, one per document.
    private readonly backends = new Map<string, { process: ChildProcess, port: number }>();
    // Manages file watchers, one per document.
    private readonly fileWatchers = new Map<string, vscode.FileSystemWatcher>();

    constructor(
        private readonly _context: vscode.ExtensionContext
    ) { }

    async openCustomDocument(
        uri: vscode.Uri,
        _openContext: { backupId?: string },
        _token: vscode.CancellationToken
    ): Promise<UiDocument> {
        return new UiDocument(uri);
    }

    async resolveCustomEditor(
        document: UiDocument,
        webviewPanel: vscode.WebviewPanel,
        _token: vscode.CancellationToken
    ): Promise<void> {
        const uriString = document.uri.toString();

        if (!this.panels.has(uriString)) {
            this.panels.set(uriString, new Set());
        }
        this.panels.get(uriString)!.add(webviewPanel);

        webviewPanel.onDidDispose(() => {
            const panelSet = this.panels.get(uriString);
            panelSet?.delete(webviewPanel);

            if (panelSet?.size === 0) {
                this.panels.delete(uriString);

                // Kill the backend process for this document.
                const backend = this.backends.get(uriString);
                if (backend) {
                    backend.process.kill('SIGTERM');
                    this.backends.delete(uriString);
                }

                const watcher = this.fileWatchers.get(uriString);
                if (watcher) {
                    watcher.dispose();
                    this.fileWatchers.delete(uriString);
                }
            }
        });

        webviewPanel.webview.options = {
            enableScripts: true,
        };
        webviewPanel.webview.onDidReceiveMessage(e => this.onMessage(document, e));

        // Logic to handle dynamic port assignment.
        if (!this.backends.has(uriString)) {
            // This is the first time opening this document, so find a new port.
            const commandString = `trace_processor_shell -D "${path.basename(document.uri.fsPath)}"`;
            webviewPanel.webview.html = this.getLoaderHtml(commandString);

            try {
                const port = await this.findFreePort();
                this.startTraceProcessor(document, webviewPanel, port);
                this.createFileWatcher(document);
            } catch (err) {
                console.error('[DejaView] Could not find a free port.', err);
                this.showErrorInPanels(uriString, 'Could not find an available port to start the backend service.');
            }
        } else {
            // A process is already running for this document, reuse its port.
            const backend = this.backends.get(uriString)!;
            webviewPanel.webview.html = this.getAppHtml(webviewPanel.webview, backend.port);
        }
    }

    /**
     * Finds an available TCP port.
     */
    private findFreePort(): Promise<number> {
        return new Promise((resolve, reject) => {
            const server = net.createServer();
            server.unref();
            server.on('error', reject);
            server.listen(0, () => {
                const { port } = server.address() as net.AddressInfo;
                server.close(() => {
                    resolve(port);
                });
            });
        });
    }

	/**
     * Creates a file watcher for a given document. This version works for files
     * both inside and outside of the current workspace.
     */
    private createFileWatcher(document: UiDocument) {
        const uriString = document.uri.toString();

        // Prevent creating duplicate watchers for the same file.
        if (this.fileWatchers.has(uriString)) {
            return;
        }

        // Create the watcher using the document's absolute file system path.
        // This is not dependent on a workspace. The boolean arguments are:
        // (ignoreCreateEvents, ignoreChangeEvents, ignoreDeleteEvents)
        const watcher = vscode.workspace.createFileSystemWatcher(document.uri.fsPath, true, false, true);
        this.fileWatchers.set(uriString, watcher);

        watcher.onDidChange(async () => {
            console.log(`[DejaView] File changed: ${document.uri.fsPath}. Reloading.`);
            const panelsToReload = this.panels.get(uriString);
            const backend = this.backends.get(uriString);

            if (panelsToReload) {
                for (const panel of panelsToReload) {
                    const commandString = `trace_processor_shell -D "${path.basename(document.uri.fsPath)}"`;
                    // Blank the webview first to indicate a reload is happening.
                    panel.webview.html = '';
                    panel.webview.html = this.getLoaderHtml(commandString);
                    // Restart the processor on the same port.
                	const port = await this.findFreePort();
                    this.startTraceProcessor(document, panel, port);
                }
            }
        });
    }

    /**
     * Starts or restarts the trace_processor_shell backend.
     * Now accepts a port number.
     */
    private startTraceProcessor(document: UiDocument, webviewPanel: vscode.WebviewPanel, port: number) {
        const uriString = document.uri.toString();
        let rpcStarted = false;

        const existingBackend = this.backends.get(uriString);
        if (existingBackend) {
            existingBackend.process.kill('SIGKILL');
        }

        const command = path.join(this._context.extensionPath, 'out', 'linux', 'trace_processor_shell');
        // Use the dynamically assigned port.
        const args = ['-D', document.uri.fsPath, '--http-port', port.toString()];

        try {
            const proc = spawn(command, args);
            // Store both the process and its port.
            this.backends.set(uriString, { process: proc, port: port });

            const onData = (data: Buffer) => {
                const output = data.toString();
                console.log(`[trace_processor_shell] ${output.trim()}`);
                if (!rpcStarted && output.includes(`[HTTP] Starting RPC server on localhost:${port}`)) {
                    rpcStarted = true;

                    const panelsToUpdate = this.panels.get(uriString);
                    if (panelsToUpdate) {
                        for (const panel of panelsToUpdate) {
                            panel.webview.html = this.getAppHtml(panel.webview, port);
                        }
                    }

                    proc.stdout?.removeListener('data', onData);
                    proc.stderr?.removeListener('data', onData);
                }
            };

            proc.stdout?.on('data', onData);
            proc.stderr?.on('data', onData);

            proc.on('error', (err) => {
                console.error(`[DejaView] Failed to start trace_processor_shell:`, err);
                this.showErrorInPanels(uriString, `Failed to start process: ${err.message}`);
                this.backends.delete(uriString);
            });

            proc.on('close', (code) => {
                // Safely remove the backend entry only if it's the one that just closed.
                const currentBackend = this.backends.get(uriString);
                if (currentBackend && currentBackend.process.pid === proc.pid) {
                    this.backends.delete(uriString);
                }

                if (rpcStarted) {
                    const errorMessage = `The trace_processor_shell backend process stopped unexpectedly (exit code: ${code}). You may need to reload the window.`;
                    this.showErrorInPanels(uriString, errorMessage);
                } else if (code !== 0) {
                    const errorMessage = `Process exited unexpectedly with code ${code}. Check command path and file permissions.`;
                    this.showErrorInPanels(uriString, errorMessage);
                }
            });
        } catch (err: any) {
            console.error('[DejaView] Error spawning trace_processor_shell:', err);
            this.showErrorInPanels(uriString, `Error spawning process: ${err.message}`);
        }
    }

    /**
     * Helper to display an error message in all visible panels for a given document.
     */
    private showErrorInPanels(uriString: string, error: string) {
        const panelsToShowError = this.panels.get(uriString);
        if (panelsToShowError) {
            for (const panel of panelsToShowError) {
                if (panel.visible) {
                    panel.webview.html = this.getErrorHtml(error);
                }
            }
        }
    }

    /**
     * Generates the common HTML structure and CSS for all webview pages.
     */
    private _getHtmlTemplate(options: { title: string; body: string; head?: string }): string {
        return /*html*/ `
        <!DOCTYPE html>
        <html lang="en">
        <head>
            <meta charset="UTF-8">
            <meta name="viewport" content="width=device-width, initial-scale=1.0">
            <title>${options.title}</title>
            <style>
                body {
                    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
                    display: flex;
                    flex-direction: column;
                    align-items: center;
                    justify-content: center;
                    height: 100vh;
                    margin: 0;
                    padding: 2em;
                    box-sizing: border-box;
                    background-color: var(--vscode-editor-background);
                    color: var(--vscode-editor-foreground);
                }
                h1, h2, p {
                    text-align: center;
                }
                .spinner {
                    width: 40px;
                    height: 40px;
                    border: 4px solid var(--vscode-descriptionForeground);
                    border-top-color: var(--vscode-button-background);
                    border-radius: 50%;
                    animation: spin 1s linear infinite;
                    margin-bottom: 20px;
                }
                @keyframes spin { to { transform: rotate(360deg); } }
                code, pre {
                    background-color: var(--vscode-textBlockQuote-background);
                    padding: 1em;
                    border-radius: 5px;
                    white-space: pre-wrap;
                    word-break: break-all;
                    font-family: "SF Mono", Monaco, Consolas, "Courier New", monospace;
                    width: 100%;
                    max-width: 800px;
                    box-sizing: border-box;
                    text-align: left;
                }
                code {
                    padding: 0.2em 0.4em;
                    display: inline-block;
                    white-space: normal;
                    word-break: normal;
                }
                .error-container h1 {
                    color: var(--vscode-errorForeground);
                }
                a {
                    color: var(--vscode-textLink-foreground);
                }
                a:hover {
                    color: var(--vscode-textLink-activeForeground);
                }
                body.app-loaded {
                    padding: 0;
                    justify-content: flex-start;
                    align-items: stretch;
                }
            </style>
            ${options.head || ''}
        </head>
        <body data-dejaview_version='{"stable":"${VERSION}"}'>
            ${options.body}
        </body>
        </html>`;
    }

    private getLoaderHtml(command: string): string {
        const body = /*html*/ `
            <div class="spinner"></div>
            <h2>Starting Trace Processor</h2>
            <p>Please wait while the backend starts...</p>
            <code>${command}</code>
        `;
        return this._getHtmlTemplate({ title: 'Loading Trace...', body });
    }

    private getErrorHtml(error: string): string {
        const body = /*html*/ `
            <div class="error-container">
                <h1>An Error Occurred</h1>
                <p>The DejaView backend encountered a problem:</p>
                <pre>${error}</pre>
            </div>
        `;
        return this._getHtmlTemplate({ title: 'Error', body });
    }

    private getAppHtml(webview: vscode.Webview, port: number): string {
        const scriptUri = getUri(webview, this._context.extensionUri, ["out", "ui", "ui", "dist"]);

        const head = /*html*/`
            <script type="text/javascript">
                'use strict';
                (function () {
                    const TIMEOUT_MS = 120000;
                    let errTimerId;

                    function showError(err) {
                        if (errTimerId) clearTimeout(errTimerId);
                        const loader = document.getElementById('app-loader');
                        const errorContainer = document.getElementById('app-load-failure');
                        const errorDetails = document.getElementById('app-load-failure-details');

                        if (loader) loader.style.display = 'none';
                        if (errorContainer) errorContainer.style.display = 'flex';
                        if (errorDetails) {
                            console.error(err);
                            errorDetails.innerText = \`\${err}\`;
                        }
                    }

                    errTimerId = setTimeout(() => showError('Timed out while loading the UI.'), TIMEOUT_MS);

                    window.onerror = (msg, url, line, col, error) => showError(error || msg);
                    window.onunhandledrejection = (event) => showError(event.reason);
                    window.rpc_port = '${port}';

                    document.addEventListener('DOMContentLoaded', () => {
                        const versionStr = document.body.dataset['dejaview_version'] || '{}';
                        const versionMap = JSON.parse(versionStr);
                        const version = versionMap['stable'];

                        const script = document.createElement('script');
                        script.async = true;
                        script.src = "${scriptUri}/" + version + '/frontend_bundle.js';

                        script.onload = () => {
                            clearTimeout(errTimerId);
                            const loader = document.getElementById('app-loader');
                            if (loader) loader.style.display = 'none';
                            document.body.classList.add('app-loaded');
                        };

                        script.onerror = () => showError(\`Failed to load main script from: \${script.src}\`);
                        document.head.append(script);
                    });
                })();
            </script>
        `;

        const body = /*html*/`
            <div id="app-loader" style="display: flex; flex-direction: column; align-items: center; justify-content: center; width: 100%; height: 100%;">
                <div class="spinner"></div>
                <h2>Loading DejaView UI</h2>
                <p>Fetching and initializing the user interface...</p>
            </div>

            <div id="app-load-failure" style="display: none; flex-direction: column; align-items: center; justify-content: center;" class="error-container">
                <h1>Failed to Load UI</h1>
                <p>An unrecoverable problem occurred while loading the DejaView UI.</p>
                <p>Please <a href="https://github.com/FlorentRevest/DejaView/issues/new" target="_blank">file a bug</a> with the details below.</p>
                <pre id="app-load-failure-details"></pre>
            </div>
        `;

        return this._getHtmlTemplate({ title: 'DejaView', body, head });
    }

    private async onMessage(_document: UiDocument, message: any) {
        switch (message.command) {
            case 'open':
                const symbols = await vscode.commands.executeCommand<vscode.SymbolInformation[]>(
                    'vscode.executeWorkspaceSymbolProvider',
                    message.symbol
                );
                if (!symbols || symbols.length === 0) {
                    vscode.window.showWarningMessage(`Symbol "${message.symbol}" not found.`);
                    return;
                }
                const symbol = symbols[0];
                const { uri, range } = symbol.location;
                const doc = await vscode.workspace.openTextDocument(uri);
                const editor = await vscode.window.showTextDocument(doc);
                editor.selection = new vscode.Selection(range.start, range.start);
                editor.revealRange(range, vscode.TextEditorRevealType.InCenter);
                return;
        }
    }
}

export function activate(context: vscode.ExtensionContext) {
	context.subscriptions.push(UiProvider.register(context));
}
