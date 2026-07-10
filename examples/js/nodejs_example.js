/*
 * file: nodejs_example.js
 * description: demonstrates how to use the cjsonx webassembly module 
 * within node.js for high-speed json processing.
 */

const cjsonx = require('../../js/cjsonx.js');

async function main() {
    // 1. wait for the wasm module to initialize
    await cjsonx.ready;
    console.log("--- cjsonx wasm engine initialized ---\n");

    const jsonString = JSON.stringify({
        sensor: "temperature",
        readings: [23.4, 23.5, 23.6, 23.8],
        active: true
    });

    console.log("parsing json:");
    console.log(jsonString);

    // 2. parse json using the wasm engine
    const success = cjsonx.parse(jsonString);

    if (!success) {
        console.error("error:", cjsonx.getError());
        return;
    }

    // 3. traverse the dom
    const root = cjsonx.getRoot();
    const sensorName = root.get("sensor").str;
    const isActive = root.get("active").bool;

    console.log(`\nresults:`);
    console.log(`sensor: ${sensorName}`);
    console.log(`active: ${isActive}`);

    // 4. read array values safely
    const readings = root.get("readings");
    const len = readings.size;
    
    console.log(`readings count: ${len}`);
    for (let i = 0; i < len; i++) {
        console.log(`  -> ${readings.getIndex(i).num}`);
    }

    // 5. free the wasm memory manually (best practice for webassembly)
    cjsonx.free();
}

main().catch(console.error);
