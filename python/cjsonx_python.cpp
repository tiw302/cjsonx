#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <stdexcept>
#include <string>
#include <vector>

#include "../include/cjsonx.h"

namespace py = pybind11;

// forward declarations
struct PyDocument;
struct PyValue;

struct PyDocument {
    cjsonx_doc_t* doc;
    bool own_doc;
    std::string json_copy; // keep a copy of the json string alive!

    PyDocument(cjsonx_doc_t* d, bool own = true) : doc(d), own_doc(own) {}
    PyDocument(cjsonx_doc_t* d, std::string&& json, bool own = true) 
        : doc(d), own_doc(own), json_copy(std::move(json)) {}
    ~PyDocument() {
        if (doc && own_doc) {
            cjsonx_doc_free(doc);
        }
    }
};

struct PyValue {
    py::object doc_ref; // keep-alive reference to the PyDocument
    cjsonx_val_t val;

    PyValue(py::object ref, cjsonx_val_t v) : doc_ref(ref), val(v) {}

    cjsonx_type_t get_type() const {
        return cjsonx_get_type(val);
    }

    bool is_null() const {
        return cjsonx_is_null(val);
    }

    bool get_bool() const {
        if (get_type() != CJSONX_BOOL) {
            throw std::runtime_error("value is not a boolean");
        }
        return cjsonx_bool(val);
    }

    double get_num() const {
        if (get_type() != CJSONX_NUMBER) {
            throw std::runtime_error("value is not a number");
        }
        return cjsonx_num(val);
    }

    int64_t get_int() const {
        if (get_type() != CJSONX_NUMBER) {
            throw std::runtime_error("value is not a number");
        }
        return cjsonx_int(val);
    }

    std::string get_str() const {
        if (get_type() != CJSONX_STRING) {
            throw std::runtime_error("value is not a string");
        }
        const char* s = cjsonx_str(val);
        size_t len = cjsonx_str_len(val);
        return std::string(s, len);
    }

    size_t size() const {
        return cjsonx_size(val);
    }

    PyValue get(const std::string& key) {
        if (get_type() != CJSONX_OBJECT) {
            throw std::runtime_error("value is not an object");
        }
        cjsonx_val_t v = cjsonx_get_len(val, key.c_str(), key.length());
        if (!cjsonx_valid(v)) {
            throw py::key_error("key '" + key + "' not found");
        }
        return PyValue(doc_ref, v);
    }

    PyValue get_index(size_t index) {
        if (get_type() != CJSONX_ARRAY) {
            throw std::runtime_error("value is not an array");
        }
        if (index >= size()) {
            throw py::index_error("index out of range");
        }
        cjsonx_val_t v = cjsonx_get_index(val, index);
        return PyValue(doc_ref, v);
    }

    PyValue pointer(const std::string& path) {
        cjsonx_val_t v = cjsonx_pointer_get(val, path.c_str());
        if (!cjsonx_valid(v)) {
            throw std::runtime_error("json pointer '" + path + "' could not be resolved");
        }
        return PyValue(doc_ref, v);
    }
};

PYBIND11_MODULE(cjsonx, m) {
    m.doc() = "cjsonx: high performance JSON parser C11 bindings";

    // expose json type enum
    py::enum_<cjsonx_type_t>(m, "Type")
        .value("NULL", CJSONX_NULL)
        .value("BOOL", CJSONX_BOOL)
        .value("NUMBER", CJSONX_NUMBER)
        .value("STRING", CJSONX_STRING)
        .value("ARRAY", CJSONX_ARRAY)
        .value("OBJECT", CJSONX_OBJECT)
        .export_values();

    // expose PyDocument class
    py::class_<PyDocument, std::shared_ptr<PyDocument>>(m, "Document")
        .def_property_readonly("is_valid", [](const PyDocument& self) {
            return self.doc ? self.doc->is_valid : false;
        })
        .def_property_readonly("error", [](const PyDocument& self) {
            if (!self.doc) return "empty document";
            return cjsonx_error_string(self.doc->error);
        })
        .def_property_readonly("error_offset", [](const PyDocument& self) {
            return self.doc ? self.doc->error_offset : 0;
        })
        .def_property_readonly("root", [](py::object self) {
            auto& doc = self.cast<PyDocument&>();
            if (!doc.doc || !doc.doc->is_valid) {
                throw std::runtime_error("invalid document");
            }
            return PyValue(self, doc.doc->root);
        });

    // expose PyValue class
    py::class_<PyValue>(m, "Value")
        .def_property_readonly("type", &PyValue::get_type)
        .def_property_readonly("is_null", &PyValue::is_null)
        .def("get_bool", &PyValue::get_bool)
        .def("get_num", &PyValue::get_num)
        .def("get_int", &PyValue::get_int)
        .def("get_str", &PyValue::get_str)
        .def("size", &PyValue::size)
        .def("get", &PyValue::get)
        .def("get_index", &PyValue::get_index)
        .def("pointer", &PyValue::pointer)
        .def("__len__", &PyValue::size)
        .def("__getitem__", [](PyValue& self, const std::string& key) {
            return self.get(key);
        })
        .def("__getitem__", [](PyValue& self, size_t index) {
            return self.get_index(index);
        })
        .def("__contains__", [](PyValue& self, const std::string& key) {
            if (self.get_type() != CJSONX_OBJECT) return false;
            cjsonx_val_t v = cjsonx_get_len(self.val, key.c_str(), key.length());
            return cjsonx_valid(v);
        })
        .def("keys", [](PyValue& self) {
            if (self.get_type() != CJSONX_OBJECT) {
                throw std::runtime_error("value is not an object");
            }
            std::vector<std::string> res;
            cjsonx_iter_t iter = cjsonx_iter_init(self.val);
            while (cjsonx_iter_next(&iter)) {
                res.push_back(std::string(cjsonx_str(iter.key), cjsonx_str_len(iter.key)));
            }
            return res;
        })
        .def("values", [](PyValue& self) {
            std::vector<PyValue> res;
            if (self.get_type() == CJSONX_OBJECT || self.get_type() == CJSONX_ARRAY) {
                cjsonx_iter_t iter = cjsonx_iter_init(self.val);
                while (cjsonx_iter_next(&iter)) {
                    res.push_back(PyValue(self.doc_ref, iter.value));
                }
            }
            return res;
        })
        .def("items", [](PyValue& self) {
            if (self.get_type() != CJSONX_OBJECT) {
                throw std::runtime_error("value is not an object");
            }
            std::vector<std::pair<std::string, PyValue>> res;
            cjsonx_iter_t iter = cjsonx_iter_init(self.val);
            while (cjsonx_iter_next(&iter)) {
                res.push_back({
                    std::string(cjsonx_str(iter.key), cjsonx_str_len(iter.key)),
                    PyValue(self.doc_ref, iter.value)
                });
            }
            return res;
        });

    // expose module level parsing functions
    m.def("parse", [](const std::string& json) {
        auto py_doc = std::make_shared<PyDocument>(nullptr, std::string(json), true);
        py_doc->doc = cjsonx_parse(py_doc->json_copy.c_str(), py_doc->json_copy.length());
        if (!py_doc->doc) {
            throw std::runtime_error("failed to allocate document");
        }
        return py_doc;
    });

    m.def("read_file", [](const std::string& path) {
        cjsonx_doc_t* d = cjsonx_read_file(path.c_str());
        if (!d) {
            throw std::runtime_error("failed to read or parse file: " + path);
        }
        return std::make_shared<PyDocument>(d, true);
    });
}
