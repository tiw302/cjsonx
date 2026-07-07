const Module = require('./cjsonx_wasm.js');

let isReady = false;
const readyPromise = new Promise((resolve) => {
    Module.onRuntimeInitialized = () => {
        isReady = true;
        resolve();
    };
});

function parse(json) {
    if (!isReady) throw new Error("wasm module not yet initialized!");
    return !!Module.ccall('cjsonx_wasm_parse', 'number', ['string'], [json]);
}

function free() {
    if (!isReady) throw new Error("wasm module not yet initialized!");
    Module.ccall('cjsonx_wasm_free', 'null', [], []);
}

function getError() {
    if (!isReady) throw new Error("wasm module not yet initialized!");
    return Module.ccall('cjsonx_wasm_get_error', 'string', [], []);
}

function getErrorOffset() {
    if (!isReady) throw new Error("wasm module not yet initialized!");
    return Module.ccall('cjsonx_wasm_get_error_offset', 'number', [], []);
}

function dump(maxLen = 65536) {
    if (!isReady) throw new Error("wasm module not yet initialized!");
    const ptr = Module._malloc(maxLen);
    const ok = Module.ccall('cjsonx_wasm_dump', 'number', ['number', 'number'], [ptr, maxLen]);
    const str = Module.UTF8ToString(ptr);
    Module._free(ptr);
    return str;
}

module.exports = {
    parse,
    free,
    getError,
    getErrorOffset,
    dump,
    ready: readyPromise,
    get isReady() { return isReady; }
};
