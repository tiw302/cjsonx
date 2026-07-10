/*
 * file: json_pointer.js
 * description: demonstrates how to use rfc 6901 json pointers in javascript.
 */

const cjsonx = require('../../js/cjsonx.js');

async function main() {
    await cjsonx.ready;

    const jsonString = '{"database": {"connections": [{"host": "127.0.0.1", "port": 5432}]}}';
    
    const success = cjsonx.parse(jsonString);
    if (!success) {
        console.error("parse error:", cjsonx.getError());
        return;
    }

    const root = cjsonx.getRoot();

    // query a specific path directly
    const path = "/database/connections/0/port";
    const portNode = root.pointer(path);
    
    if (portNode) {
        console.log(`pointer '${path}' found!`);
        console.log(`value: ${portNode.num}`);
    } else {
        console.log(`pointer '${path}' not found.`);
    }

    // query an invalid path
    const invalidPath = "/database/config";
    const configNode = root.pointer(invalidPath);
    
    if (!configNode) {
        console.log(`pointer '${invalidPath}' correctly returned null.`);
    }

    cjsonx.free();
}

main().catch(console.error);
