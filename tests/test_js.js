/*
 * test_js.js - integration tests for javascript/webassembly bindings.
 * verifies wasm module initialization, dom traversal, and proper
 * error handling when parsing json from a node.js environment.
 */

const cjsonx = require('../js/cjsonx.js');

async function runTests() {
    console.log("Waiting for WASM module to initialize...");
    await cjsonx.ready;
    console.log("WASM module initialized successfully!\n");

    let passed = 0;
    let failed = 0;

    function assert(cond, msg) {
        if (cond) {
            passed++;
            console.log(`[✓] PASS: ${msg}`);
        } else {
            failed++;
            console.error(`[✗] FAIL: ${msg}`);
        }
    }

    // test 1: simple parse
    try {
        const ok = cjsonx.parse('{"name": "cjsonx", "fast": true, "version": 1.1}');
        assert(ok === true, "Parse simple valid JSON");

        const dumped = cjsonx.dump(1024);
        assert(dumped.includes('"name": "cjsonx"'), "Dumping parses key-value correctly");
        assert(dumped.includes('"fast": true'), "Dumping parses boolean correctly");
        assert(dumped.includes('"version": 1.1'), "Dumping parses float correctly");

        cjsonx.free();
    } catch (e) {
        assert(false, "Simple parse test threw: " + e.message);
    }

    // test 2: invalid json
    try {
        const ok = cjsonx.parse('{"incomplete": ');
        assert(ok === false, "Parse invalid JSON returns false");

        const err = cjsonx.getError();
        const offset = cjsonx.getErrorOffset();
        assert(err.length > 0, "Error message is populated: " + err);
        assert(offset > 0, "Error offset is populated: " + offset);

        cjsonx.free();
    } catch (e) {
        assert(false, "Invalid parse test threw: " + e.message);
    }

    // test 3: dom querying and tojs()
    try {
        const jsonStr = '{"a": [10, 20, {"b": "hello"}], "c": true, "d": null}';
        const ok = cjsonx.parse(jsonStr);
        assert(ok === true, "Parse complex JSON for querying");

        const root = cjsonx.getRoot();
        assert(root !== null, "getRoot returns value");
        assert(root.type === cjsonx.Type.OBJECT, "root type is object");
        assert(root.size === 3, "root size is 3");

        const a = root.get("a");
        assert(a !== null, "get 'a' returns value");
        assert(a.type === cjsonx.Type.ARRAY, "'a' type is array");
        assert(a.size === 3, "'a' size is 3");

        const item0 = a.getIndex(0);
        assert(item0.num === 10, "array index 0 value is 10");

        const pointerVal = root.pointer("/a/2/b");
        assert(pointerVal !== null, "pointer resolution");
        assert(pointerVal.str === "hello", "pointer value is 'hello'");

        // test conversion to native js object
        const nativeObj = root.toJS();
        assert(JSON.stringify(nativeObj) === JSON.stringify(JSON.parse(jsonStr)), "toJS converts correctly to native object");

        cjsonx.free();
    } catch (e) {
        assert(false, "DOM Querying test threw: " + e.message);
    }

    console.log(`\nTests completed: ${passed} passed, ${failed} failed.`);
    if (failed > 0) {
        process.exit(1);
    }
}

runTests().catch(err => {
    console.error("Test suite failed: ", err);
    process.exit(1);
});
