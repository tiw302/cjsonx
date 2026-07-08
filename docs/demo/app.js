// wait for wasm
Module.onRuntimeInitialized = () => {
    document.getElementById('statusBar').innerText = "WASM Ready. Enter JSON and press Parse.";
    document.getElementById('resultBox').innerText = "";
    document.getElementById('statusBar').className = "status-bar status-success";
};

// templates
const templates = {
    valid: '{\n  "name": "cjsonx",\n  "fast": true,\n  "version": 1.0,\n  "tags": ["C11", "JSON", "WASM"]\n}',
    invalid: '{\n  "name": "cjsonx",\n  "fast": true,\n  "version": 1.0 \n  "tags": ["Missing comma!"]\n}',
    large: (() => {
        let arr = [];
        for (let i = 0; i < 1000; i++) {
            arr.push({ "id": i, "active": i % 2 === 0, "score": Math.random() });
        }
        return JSON.stringify(arr, null, 2);
    })(),
    nested: '{\n  "level1": {\n    "level2": {\n      "level3": [\n        {"id": 1, "value": "A"},\n        {"id": 2, "value": "B"}\n      ]\n    }\n  }\n}',
    unicode: '{\n  "emoji": "🚀✨",\n  "mixed": ["hello", 123, null, false, {"escaped": "Line1\\nLine2\\tTab"}],\n  "thai": "สวัสดีชาวโลก",\n  "japanese": "こんにちは世界",\n  "korean": "안녕 세상",\n  "chinese": "你好，世界",\n  "arabic": "مرحبا بالعالم",\n  "hebrew": "שלום עולם",\n  "russian": "Привет, мир",\n  "hindi": "नमस्ते दुनिया",\n  "greek": "Γειά σου Κόσμε",\n  "georgian": "გამარჯობა მსოფლიო",\n  "amharic": "ሰላም ልዑል",\n  "burmese": "မင်္ဂလာပါ ကမ္ဘာလောက",\n  "tibetan": "ཁམས་བཟང་འཛམ་གླིང་།",\n  "emoji_hello": "👋🌍✨🚀"\n}',
    numbers: '{\n  "description": "Eisel-Lemire & Safe Integer precision parsing",\n  "pi": 3.14159265358979323846,\n  "planck_length": 1.616255e-35,\n  "avogadro_number": 6.02214076e23,\n  "max_safe_integer": 9007199254740991,\n  "min_safe_integer": -9007199254740991,\n  "coordinate_x": -122.4194155,\n  "coordinate_y": 37.774929\n}'
};

document.getElementById('templateSelect').addEventListener('change', (e) => {
    const key = e.target.value;
    if (templates[key]) {
        document.getElementById('inputJson').value = templates[key];
        // auto-parse
        document.getElementById('parseBtn').click();
    }
});

// init load
document.getElementById('inputJson').value = templates['valid'];

document.getElementById('parseBtn').addEventListener('click', () => {
    try {
        const jsonStr = document.getElementById('inputJson').value;
        const statusBar = document.getElementById('statusBar');
        const resultBox = document.getElementById('resultBox');

        statusBar.innerText = "Parsing...";
        statusBar.className = "status-bar";
        resultBox.innerText = "Processing...";

        const t0 = performance.now();

        // alloc memory
        const len = Module.lengthBytesUTF8(jsonStr) + 1;
        const ptrJson = Module._malloc(len);
        Module.stringToUTF8(jsonStr, ptrJson, len);

        // parse json
        const isValid = Module.ccall('cjsonx_wasm_parse', 'number', ['number'], [ptrJson]);

        const t1 = performance.now();
        const duration = (t1 - t0).toFixed(2);

        if (isValid === 1) {
            statusBar.innerText = `Parse Successful! Time: ${duration} ms`;
            statusBar.className = "status-bar status-success";

            // dump ast
            const dumpMaxLen = 1024 * 1024; // 1mb max
            const ptrDump = Module._malloc(dumpMaxLen);
            Module.ccall('cjsonx_wasm_dump', 'number', ['number', 'number'], [ptrDump, dumpMaxLen]);
            const dumpStr = Module.UTF8ToString(ptrDump);
            resultBox.innerText = dumpStr;
            Module._free(ptrDump);
        } else {
            // get error
            const ptrErr = Module.ccall('cjsonx_wasm_get_error', 'number', [], []);
            const errStr = Module.UTF8ToString(ptrErr);
            const errOffset = Module.ccall('cjsonx_wasm_get_error_offset', 'number', [], []);

            statusBar.innerText = `Parse Error: ${errStr} at offset ${errOffset}. Time: ${duration} ms`;
            statusBar.className = "status-bar status-error";
            resultBox.innerText = "";
        }

        // clean up
        Module._free(ptrJson);
    } catch (e) {
        document.getElementById('statusBar').innerText = "Critical Error: WASM module crashed.";
        document.getElementById('statusBar').className = "status-bar status-error";
        console.error(e);
    }
});

// copy to clipboard (DOM)
document.getElementById('btnCopy').addEventListener('click', () => {
    const resultText = document.getElementById('resultBox').innerText;
    if (resultText && !resultText.startsWith('Please wait')) {
        navigator.clipboard.writeText(resultText).then(() => {
            const btn = document.getElementById('btnCopy');
            const originalText = btn.innerText;
            btn.innerText = "Copied!";
            setTimeout(() => { btn.innerText = originalText; }, 1500);
        });
    }
});

// copy to clipboard (Snippet)
document.getElementById('btnCopySnippet').addEventListener('click', () => {
    const codeText = document.getElementById('codeBox').innerText;
    if (codeText) {
        navigator.clipboard.writeText(codeText).then(() => {
            const btn = document.getElementById('btnCopySnippet');
            const originalText = btn.innerText;
            btn.innerText = "Copied!";
            setTimeout(() => { btn.innerText = originalText; }, 1500);
        });
    }
});

// snippets
const codeSnippets = {
    c: `#define CJSONX_IMPLEMENTATION
#include "cjsonx.h"
#include <stdio.h>

int main() {
    const char* json = "{\\"message\\": \\"Hello World\\"}";

    // parse json
    cjsonx_doc_t* doc = cjsonx_parse(json, strlen(json));

    if (doc && doc->is_valid) {
        cjsonx_val_t root = doc->root;
        cjsonx_val_t msg = cjsonx_get(root, "message");

        if (cjsonx_get_type(msg) == CJSONX_STRING) {
            printf("%.*s\\n", (int)cjsonx_str_len(msg), cjsonx_str(msg));
        }

        cjsonx_doc_free(doc); // free memory
    } else {
        printf("Parse Error: %s\\n", cjsonx_error_string(doc->error));
    }

    return 0;
}`,
    cpp: `#define CJSONX_IMPLEMENTATION
#include "cjsonx.h"
#include <iostream>
#include <string_view>

int main() {
    std::string_view json = R"({"message": "Hello C++ World"})";

    auto* doc = cjsonx_parse(json.data(), json.size());
    if (doc && doc->is_valid) {
        cjsonx_val_t msg = cjsonx_get(doc->root, "message");

        if (cjsonx_get_type(msg) == CJSONX_STRING) {
            std::string_view str(cjsonx_str(msg), cjsonx_str_len(msg));
            std::cout << str << "\\n";
        }

        cjsonx_doc_free(doc);
    }
    return 0;
}`,
    wasm: `// install via npm
// npm install @tiw302/cjsonx

const cjsonx = require('@tiw302/cjsonx');

async function run() {
    await cjsonx.ready;

    const ok = cjsonx.parse('{"name": "alice", "scores": [10, 20, 30]}');
    if (ok) {
        const root  = cjsonx.getRoot();

        // get a string field
        const name   = root.get('name').str;  // 'alice'

        // index into an array
        const first  = root.get('scores').getIndex(0).num; // 10

        // json pointer path
        const third  = root.pointer('/scores/2').num; // 30

        // convert entire dom to plain js object
        const obj    = root.toJS();

        cjsonx.free();
    }
}`
};

document.getElementById('langSelect').addEventListener('change', (e) => {
    document.getElementById('codeBox').innerText = codeSnippets[e.target.value];
});
// default
document.getElementById('codeBox').innerText = codeSnippets['c'];
