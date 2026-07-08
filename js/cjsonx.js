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

const Type = {
    NULL: 0,
    BOOL: 1,
    NUMBER: 2,
    STRING: 3,
    ARRAY: 4,
    OBJECT: 5
};

class Value {
    constructor(idx) {
        this.idx = idx;
    }
    get type() {
        return Module.ccall('cjsonx_wasm_get_type', 'number', ['number'], [this.idx]);
    }
    get size() {
        return Module.ccall('cjsonx_wasm_get_size', 'number', ['number'], [this.idx]);
    }
    get bool() {
        return !!Module.ccall('cjsonx_wasm_get_bool', 'number', ['number'], [this.idx]);
    }
    get num() {
        return Module.ccall('cjsonx_wasm_get_num', 'number', ['number'], [this.idx]);
    }
    get str() {
        const ptr = Module.ccall('cjsonx_wasm_get_str', 'number', ['number'], [this.idx]);
        const len = Module.ccall('cjsonx_wasm_get_str_len', 'number', ['number'], [this.idx]);
        return Module.UTF8ToString(ptr, len);
    }
    get(key) {
        const childIdx = Module.ccall('cjsonx_wasm_object_get', 'number', ['number', 'string'], [this.idx, key]);
        if (childIdx === 4294967295) return null; // uint32_max
        return new Value(childIdx);
    }
    getIndex(index) {
        const childIdx = Module.ccall('cjsonx_wasm_get_array_item', 'number', ['number', 'number'], [this.idx, index]);
        if (childIdx === 4294967295) return null;
        return new Value(childIdx);
    }
    pointer(path) {
        const childIdx = Module.ccall('cjsonx_wasm_pointer_get', 'number', ['number', 'string'], [this.idx, path]);
        if (childIdx === 4294967295) return null;
        return new Value(childIdx);
    }
    // recursively convert DOM to native javascript object
    toJS() {
        const t = this.type;
        if (t === 0) return null; // null
        if (t === 1) return this.bool; // bool
        if (t === 2) return this.num; // number
        if (t === 3) return this.str; // string
        if (t === 4) { // array
            const arr = [];
            const sz = this.size;
            for (let i = 0; i < sz; i++) {
                const item = this.getIndex(i);
                arr.push(item ? item.toJS() : null);
            }
            return arr;
        }
        if (t === 5) { // object
            const obj = {};
            const sz = this.size;
            for (let i = 0; i < sz; i++) {
                const keyPtr = Module.ccall('cjsonx_wasm_get_child_key', 'number', ['number', 'number'], [this.idx, i]);
                const keyLen = Module.ccall('cjsonx_wasm_get_child_key_len', 'number', ['number', 'number'], [this.idx, i]);
                const key = Module.UTF8ToString(keyPtr, keyLen);
                const valIdx = Module.ccall('cjsonx_wasm_get_child_val', 'number', ['number', 'number'], [this.idx, i]);
                if (valIdx !== 4294967295) {
                    obj[key] = new Value(valIdx).toJS();
                } else {
                    obj[key] = null;
                }
            }
            return obj;
        }
        return null;
    }
}

function getRoot() {
    if (!isReady) throw new Error("wasm module not yet initialized!");
    const idx = Module.ccall('cjsonx_wasm_get_root', 'number', [], []);
    if (idx === 4294967295) return null;
    return new Value(idx);
}

module.exports = {
    parse,
    free,
    getError,
    getErrorOffset,
    dump,
    getRoot,
    Type,
    Value,
    ready: readyPromise,
    get isReady() { return isReady; }
};
