/*
 * file: error_handling.js
 * description: demonstrates how to detect syntax errors and print error offsets
 * using the cjsonx wasm engine in javascript.
 */

const cjsonx = require('../../js/cjsonx.js');

async function main() {
    await cjsonx.ready;

    // malformed json string
    const brokenJson = '{"name": "cjsonx", "valid": false, }'; // trailing comma

    console.log("attempting to parse malformed json...\n");

    const success = cjsonx.parse(brokenJson);

    if (!success) {
        const errorMsg = cjsonx.getError();
        const errorOffset = cjsonx.getErrorOffset();
        
        console.log("error detected!");
        console.log(`message: ${errorMsg}`);
        console.log(`offset:  ${errorOffset}\n`);

        // point to the error
        console.log("json snippet:");
        console.log(brokenJson);
        let pointer = "";
        for (let i = 0; i < errorOffset; i++) pointer += " ";
        pointer += "^";
        console.log(pointer);
    } else {
        console.log("parsed successfully");
        cjsonx.free();
    }
}

main().catch(console.error);
