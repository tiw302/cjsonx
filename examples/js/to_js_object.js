/*
 * file: to_js_object.js
 * description: demonstrates how to quickly convert a parsed cjsonx dom
 * directly into a native javascript object (useful for wasm interop).
 */

const cjsonx = require('../../js/cjsonx.js');

async function main() {
    await cjsonx.ready;

    const jsonString = '{"user": {"id": 42, "name": "alice"}, "tags": ["admin", "staff"]}';

    console.log("parsing json into cjsonx dom...");
    const success = cjsonx.parse(jsonString);

    if (!success) {
        console.error("parse error:", cjsonx.getError());
        return;
    }

    const root = cjsonx.getRoot();

    // convert the flat c-memory dom back into a native v8 javascript object
    console.log("\nconverting root node to native js object:");
    const jsObj = root.toJS();
    
    console.log(jsObj);
    console.log(`\nuser id from native js object: ${jsObj.user.id}`);

    // you can also convert specific sub-trees
    const tagsNode = root.get("tags");
    const jsTags = tagsNode.toJS();
    
    console.log("\ntags array as native js object:");
    console.log(jsTags);

    cjsonx.free();
}

main().catch(console.error);
