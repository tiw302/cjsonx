// updated 2026-07-09
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk

#ifndef CJSONX_HPP
#define CJSONX_HPP

#include <stdexcept>
#include <string>

#include "cjsonx.h"

namespace cjsonx {

class Node {
    cjsonx_val_t val;

   public:
    Node(cjsonx_val_t v) : val(v) {}

    cjsonx_type_t type() const {
        return cjsonx_get_type(val);
    }

    bool is_null() const {
        return type() == CJSONX_NULL;
    }
    bool is_bool() const {
        return type() == CJSONX_BOOL;
    }
    bool is_number() const {
        return type() == CJSONX_NUMBER;
    }
    bool is_string() const {
        return type() == CJSONX_STRING;
    }
    bool is_array() const {
        return type() == CJSONX_ARRAY;
    }
    bool is_object() const {
        return type() == CJSONX_OBJECT;
    }

    bool as_bool() const {
        return cjsonx_bool(val);
    }
    double as_double() const {
        return cjsonx_num(val);
    }
    int64_t as_int() const {
        return cjsonx_int(val);
    }

    std::string as_string() const {
        return std::string(cjsonx_str(val), cjsonx_str_len(val));
    }

    Node operator[](const std::string& key) const {
        if (!is_object()) throw std::runtime_error("Node is not an object");
        return Node(cjsonx_get(val, key.c_str()));
    }
    
    Node operator[](const char* key) const {
        if (!is_object()) throw std::runtime_error("Node is not an object");
        return Node(cjsonx_get(val, key));
    }

    Node operator[](size_t index) const {
        if (!is_array()) throw std::runtime_error("Node is not an array");
        return Node(cjsonx_get_index(val, index));
    }

    Node pointer(const std::string& path) const {
        return Node(cjsonx_pointer_get(val, path.c_str()));
    }
    
    Node pointer(const char* path) const {
        return Node(cjsonx_pointer_get(val, path));
    }

    size_t size() const {
        return cjsonx_size(val);
    }

    cjsonx_val_t raw_value() const {
        return val;
    }

    // Iterator wrapper for C++11 range-based for loops
    struct Iterator {
        cjsonx_iter_t iter;
        bool valid;

        Iterator() : valid(false) {}
        Iterator(cjsonx_val_t v) {
            iter = cjsonx_iter_init(v);
            valid = cjsonx_iter_next(&iter);
        }

        bool operator!=(const Iterator& other) const { return valid != other.valid; }
        Iterator& operator++() {
            valid = cjsonx_iter_next(&iter);
            return *this;
        }

        struct KV {
            cjsonx_val_t k, v;
            std::string key() const { return std::string(cjsonx_str(k), cjsonx_str_len(k)); }
            Node value() const { return Node(v); }
            // Implicit conversion for array iteration: `for (Node item : array)`
            operator Node() const { return Node(v); }
        };

        KV operator*() const {
            return KV{iter.key, iter.value};
        }
    };

    Iterator begin() const {
        if (!is_array() && !is_object()) return Iterator();
        return Iterator(val);
    }
    
    Iterator end() const {
        return Iterator();
    }
};

class Document {
    cjsonx_doc_t* doc;

   public:
    Document(cjsonx_doc_t* d) : doc(d) {}
    ~Document() {
        if (doc) cjsonx_doc_free(doc);
    }

    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

    Document(Document&& other) noexcept : doc(other.doc) {
        other.doc = nullptr;
    }

    Document& operator=(Document&& other) noexcept {
        if (this != &other) {
            if (doc) cjsonx_doc_free(doc);
            doc = other.doc;
            other.doc = nullptr;
        }
        return *this;
    }

    bool is_valid() const {
        return doc && doc->is_valid;
    }
    std::string error_message() const {
        return doc ? cjsonx_error_string(doc->error) : "Null document";
    }

    Node root() const {
        return Node(doc->root);
    }
};

inline Document parse(const std::string& json) {
    return Document(cjsonx_parse(json.data(), json.size()));
}

inline Document parse(const char* json, size_t len) {
    return Document(cjsonx_parse(json, len));
}

}  // namespace cjsonx

#endif  // cjsonx_hpp
