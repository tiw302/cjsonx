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

    // Test 1: Simple Parse
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

    // Test 2: Invalid JSON
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

    console.log(`\nTests completed: ${passed} passed, ${failed} failed.`);
    if (failed > 0) {
        process.exit(1);
    }
}

runTests().catch(err => {
    console.error("Test suite failed: ", err);
    process.exit(1);
});
