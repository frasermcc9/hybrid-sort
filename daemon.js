const chokidar = require("chokidar");
const { exec } = require("child_process");
const { basename } = require("path");
const colors = require("colors");
const { mkdirSync, existsSync } = require("fs");

console.log("Watching files for changes...");

/**
 * @param {string} src
 */
const getOutputPath = (src) => "./build/" + basename(src.slice(0, src.length - 2));

/**
 *
 * @param {string} cmd
 * @returns {Promise<string>}
 */
const promiseExec = (cmd) =>
    new Promise((res, rej) => {
        exec(cmd, (err, stdout, stderr) => {
            if (err) rej(err);
            if (stderr) rej(stderr);
            if (stdout) res(stdout);
            res();
        });
    });

chokidar.watch("**/*.c", { usePolling: true }).on("all", async (event, path) => {
    const outPath = getOutputPath(path);
    const fileName = basename(path);

    const consoleOutput = [];

    if (!existsSync("./build")) {
        mkdirSync("./build");
    }

    const header = `----------- ${fileName} -----------`;
    consoleOutput.push("");
    consoleOutput.push(colors.blue.bold("-".repeat(header.length)));
    consoleOutput.push(colors.blue.bold(header));
    consoleOutput.push(colors.blue.bold("-".repeat(header.length)));
    consoleOutput.push("");

    try {
        consoleOutput.push(colors.cyan("Compile Result:"));
        await promiseExec(`cc -O2 ${path} -o ${outPath} -lm -lpthread -lrt`);
        consoleOutput.push(colors.green("🚀 Compiled successfully"));
    } catch (e) {
        consoleOutput.push(colors.red("❌ Compile errors:"));
        consoleOutput.push(colors.yellow.bold(e));
    }

    try {
        const exec = await promiseExec(`${outPath} 5`);
        consoleOutput.push(colors.cyan("Execution Result:"));
        consoleOutput.push(exec);
        consoleOutput.push(colors.green("Execution finished"));
    } catch (e) {
        consoleOutput.push(colors.red("❌ Execution failed:"));
        consoleOutput.push(colors.yellow.bold(e));
    }

    consoleOutput.forEach((s) => console.log(s));
});
