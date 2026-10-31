// Run a WASI program under Node, as a test runs a native one: its
// arguments, its environment, the working directory it was started in,
// and its exit status as Node's.
//
//   node scripts/wasi-run.js program.wasm [args...]
//
// The emulator the wasm32-wasi toolchain gives CTest
// (cmake/toolchains/wasm32-wasi.cmake). Node's own WASI, so the runtime
// comes from the distribution like every other tool here.
"use strict";

const fs = require("fs");
const path = require("path");
const { WASI } = require("wasi");

async function main() {
    const program = process.argv[2];
    // wasi-libc resolves a relative path against "/", its working
    // directory, so "/" is the test's own; the repository is opened too,
    // at its own path, for the tests that read files from the source tree.
    const repo = path.resolve(__dirname, "..");
    const wasi = new WASI({
        version: "preview1",
        args: [path.basename(program), ...process.argv.slice(3)],
        env: process.env,
        preopens: { "/": process.cwd(), [repo]: repo },
        returnOnExit: true,
    });
    const module = await WebAssembly.compile(fs.readFileSync(program));
    const instance = await WebAssembly.instantiate(module, wasi.getImportObject
        ? wasi.getImportObject()
        : { wasi_snapshot_preview1: wasi.wasiImport });
    process.exitCode = wasi.start(instance);
}

main().catch((e) => {
    console.error(e && e.stack ? e.stack : String(e));
    process.exitCode = 1;
});
