/*
 * file: error_handling.rs
 * description: demonstrates safe error handling and result matching
 * when parsing malformed json using the rust ffi bindings.
 */

use cjsonx::Document;

fn main() {
    // a malformed json string (missing closing bracket)
    let bad_json = r#"{
        "status": "error",
        "data": [1, 2, 3
    }"#;

    println!("--- attempting to parse malformed json ---");
    
    // 1. match the result instead of using .expect()
    match Document::parse(bad_json) {
        Ok(doc) => {
            println!("parsed successfully! (this shouldn't happen)");
            // use doc...
            let _root = doc.root();
        }
        Err(e) => {
            // gracefully handle the error
            println!("parse failed as expected.");
            println!("error message: {}", e);
        }
    }
}
