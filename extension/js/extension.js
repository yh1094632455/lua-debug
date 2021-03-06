const vscode = require("vscode");
const configurationProvider = require("./configurationProvider");
const descriptorFactory = require("./descriptorFactory");
const trackerFactory = require("./trackerFactory");
const pickProcess = require("./pickProcess");

function activate(context) {
    exports.context = context;
    context.subscriptions.push(
        vscode.debug.registerDebugConfigurationProvider('lua', configurationProvider),
        vscode.debug.registerDebugAdapterDescriptorFactory('lua', descriptorFactory),
        vscode.debug.registerDebugAdapterTrackerFactory('lua', trackerFactory),
        vscode.commands.registerCommand("extension.lua-debug.pickProcess", pickProcess.pick),
        vscode.commands.registerCommand("extension.lua-debug.runEditorContents", (uri) => {
            vscode.debug.startDebugging(undefined, {
                type: 'lua',
                name: 'Run Editor Contents',
                request: 'launch',
                program: uri.fsPath,
                noDebug: true
            });
        }),
        vscode.commands.registerCommand("extension.lua-debug.debugEditorContents", (uri) => {
            vscode.debug.startDebugging(undefined, {
                type: 'lua',
                name: 'Debug Editor Contents',
                request: 'launch',
                program: uri.fsPath
            });
        }),
        vscode.commands.registerCommand('extension.lua-debug.showIntegerAsDec', (variable) => {
            vscode.workspace.getConfiguration("lua.debug.variables").update("showIntegerAsHex", false, vscode.ConfigurationTarget.workspace);
            const ds = vscode.debug.activeDebugSession;
            if (ds) {
                ds.customRequest('customRequestShowIntegerAsDec');
            }
        }),
        vscode.commands.registerCommand('extension.lua-debug.showIntegerAsHex', (variable) => {
            vscode.workspace.getConfiguration("lua.debug.variables").update("showIntegerAsHex", true, vscode.ConfigurationTarget.workspace);
            const ds = vscode.debug.activeDebugSession;
            if (ds) {
                ds.customRequest('customRequestShowIntegerAsHex');
            }
        })
    );
}

function deactivate() {
}

exports.activate = activate;
exports.deactivate = deactivate;
