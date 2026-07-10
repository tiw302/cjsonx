/*
 * file: rust_example.rs
 * description: demonstrates safe rust idiomatic usage of the cjsonx ffi bindings.
 */

use cjsonx::{Document, JsonType};

fn main() {
    let json_data = r#"{
        "project": "cjsonx",
        "performance": 100.0,
        "supported_langs": ["c", "python", "js", "rust"]
    }"#;

    println!("--- parsing json via rust ffi ---");
    
    // 1. parse using safe abstraction (handles unsafe c-ffi underneath)
    let doc = Document::parse(json_data).expect("failed to parse");
    
    let root = doc.root();
    println!("root type: {:?}", root.get_type());
    
    if root.get_type() == JsonType::Object {
        // extract string safely
        if let Some(project_node) = root.get("project") {
            println!("project node found, type: {:?}", project_node.get_type());
            if let Some(project) = project_node.as_str() {
                println!("project name: {}", project);
            }
        } else {
            println!("project node NOT found");
        }

        // extract array
        if let Some(langs) = root.get("supported_langs") {
            println!("supported languages:");
            for i in 0..langs.len() {
                if let Some(lang_node) = langs.at(i) {
                    if let Some(lang) = lang_node.as_str() {
                        println!(" - {}", lang);
                    }
                }
            }
        }
    }
}
