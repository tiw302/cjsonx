const { execSync } = require('child_process');

const c = {
    ok: '\x1b[92m',
    err: '\x1b[91m',
    info: '\x1b[96m',
    rs: '\x1b[0m'
};

console.log(`${c.info}[⚙] building cjsonx wasm module...${c.rs}`);

try {
    execSync('emcc --version', { stdio: 'ignore' });
} catch (e) {
    console.error(`${c.err}[✗] error: emscripten (emcc) could not be found.${c.rs}`);
    console.error(`${c.info}please install emscripten sdk first.${c.rs}`);
    process.exit(1);
}

const exportedFunctions = [
    '_cjsonx_wasm_free', '_cjsonx_wasm_parse', '_cjsonx_wasm_get_error',
    '_cjsonx_wasm_get_error_offset', '_cjsonx_wasm_dump',
    '_malloc', '_free'
];

const runtimeMethods = ['ccall', 'cwrap', 'UTF8ToString', 'getValue', 'setValue'];

const baseArgs = [
    'emcc wasm_wrapper.c',
    '-O3',
    '-s WASM=1',
    `-s EXPORTED_FUNCTIONS="[${exportedFunctions.map(f => `'${f}'`).join(',')}]"`,
    `-s EXPORTED_RUNTIME_METHODS="[${runtimeMethods.map(m => `'${m}'`).join(',')}]"`
];

const cmdStandard = [...baseArgs, '-o cjsonx_wasm.js'].join(' ');

try {
    console.log(`${c.info}Building standard WASM module...${c.rs}`);
    execSync(cmdStandard, { stdio: 'inherit', shell: true });
    console.log(`${c.ok}[✓] build successful!${c.rs}`);
} catch (e) {
    console.error(`${c.err}[✗] build failed.${c.rs}`);
    process.exit(1);
}
