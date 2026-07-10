use std::ffi::{c_char, c_void, CString};
use std::str;

/// represents the type of a json node.
#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum JsonType {
    Null = 0,
    Bool = 1,
    Number = 2,
    String = 3,
    Array = 4,
    Object = 5,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct cjsonx_val_t {
    pub internal: [u64; 2],
}

#[repr(C)]
pub struct cjsonx_doc_t {
    pub root: cjsonx_val_t,
    pub nodes: *mut c_void,
    pub node_count: usize,
    pub node_capacity: usize,
    pub original_json: *const c_char,
    pub json_len: usize,
    pub is_valid: bool,
    pub error: i32,
    pub error_offset: usize,
    // we don't need the rest of the fields as we only access up to `error_offset`
}

extern "C" {
    fn cjsonx_parse_copy(json: *const c_char, len: usize) -> *mut cjsonx_doc_t;
    fn cjsonx_doc_free(doc: *mut cjsonx_doc_t);
    fn cjsonx_get_type(val: cjsonx_val_t) -> JsonType;
    fn cjsonx_num(val: cjsonx_val_t) -> f64;
    fn cjsonx_bool(val: cjsonx_val_t) -> bool;
    fn cjsonx_str(val: cjsonx_val_t) -> *const c_char;
    fn cjsonx_str_len(val: cjsonx_val_t) -> usize;
    fn cjsonx_get(val: cjsonx_val_t, key: *const c_char) -> cjsonx_val_t;
    fn cjsonx_get_index(val: cjsonx_val_t, idx: usize) -> cjsonx_val_t;
    fn cjsonx_size(val: cjsonx_val_t) -> usize;
}

/// represents a parsed json document.
/// manages the memory of the underlying cjsonx arena.
pub struct Document {
    doc: *mut cjsonx_doc_t,
}

impl Drop for Document {
    fn drop(&mut self) {
        unsafe {
            if !self.doc.is_null() {
                cjsonx_doc_free(self.doc);
            }
        }
    }
}

/// represents a single json value/node in the document tree.
/// this is a lightweight handle that does not own memory.
pub struct Node {
    val: cjsonx_val_t,
}

impl Document {
    /// parses a json string and returns a document.
    /// returns an error if the json is invalid or memory allocation fails.
    pub fn parse(json: &str) -> Result<Self, &'static str> {
        let doc = unsafe { cjsonx_parse_copy(json.as_ptr() as *const c_char, json.len()) };
        if doc.is_null() {
            return Err("Failed to allocate document or parse error");
        }

        unsafe {
            if !(*doc).is_valid {
                cjsonx_doc_free(doc);
                return Err("Invalid JSON");
            }
        }

        Ok(Document { doc })
    }

    /// returns the root node of the parsed document.
    pub fn root(&self) -> Node {
        unsafe {
            Node {
                val: (*self.doc).root,
            }
        }
    }
}

impl Node {
    /// gets the type of the current node.
    pub fn get_type(&self) -> JsonType {
        unsafe { cjsonx_get_type(self.val) }
    }

    /// returns the node as a string slice if it is a string.
    pub fn as_str(&self) -> Option<&str> {
        if self.get_type() != JsonType::String {
            return None;
        }
        unsafe {
            let ptr = cjsonx_str(self.val);
            let len = cjsonx_str_len(self.val);
            let slice = std::slice::from_raw_parts(ptr as *const u8, len);
            str::from_utf8(slice).ok()
        }
    }

    /// returns the node as a float if it is a number.
    pub fn as_f64(&self) -> Option<f64> {
        if self.get_type() != JsonType::Number {
            return None;
        }
        unsafe { Some(cjsonx_num(self.val)) }
    }

    /// returns the node as a boolean if it is a bool.
    pub fn as_bool(&self) -> Option<bool> {
        if self.get_type() != JsonType::Bool {
            return None;
        }
        unsafe { Some(cjsonx_bool(self.val)) }
    }

    /// looks up a child node by key if this node is an object.
    pub fn get(&self, key: &str) -> Option<Node> {
        if self.get_type() != JsonType::Object {
            return None;
        }
        let c_key = CString::new(key).unwrap();
        let child = unsafe { cjsonx_get(self.val, c_key.as_ptr()) };
        if unsafe { cjsonx_get_type(child) } == JsonType::Null {
            None // actually we should check if it exists, assuming Null type implies missing for simplicity
        } else {
            Some(Node { val: child })
        }
    }

    /// retrieves an element at the specified index if this node is an array.
    pub fn at(&self, index: usize) -> Option<Node> {
        if self.get_type() != JsonType::Array {
            return None;
        }
        if index >= self.len() {
            return None;
        }
        let child = unsafe { cjsonx_get_index(self.val, index) };
        Some(Node { val: child })
    }

    /// returns the number of elements in an array or object.
    /// returns 0 for other types.
    pub fn len(&self) -> usize {
        unsafe { cjsonx_size(self.val) }
    }
}
