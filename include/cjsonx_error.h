// updated 2026-06-13
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk
#ifndef CJSONX_ERROR_H
#define CJSONX_ERROR_H

// ███████ ██████  ██████   ██████  ██████
// ██      ██   ██ ██   ██ ██    ██ ██   ██
// █████   ██████  ██████  ██    ██ ██████
// ██      ██   ██ ██   ██ ██    ██ ██   ██
// ███████ ██   ██ ██   ██  ██████  ██   ██
//
// >>error handling


// error codes

typedef enum {
    CJSONX_SUCCESS = 0,                     // parsed successfully
    CJSONX_ERROR_OOM,                       // out of memory
    CJSONX_ERROR_EMPTY_INPUT,               // input is empty or whitespace only
    CJSONX_ERROR_UNTERMINATED_STRING,       // string is not closed with a quote
    CJSONX_ERROR_INVALID_CONTROL_CHAR,      // raw control character inside string
    CJSONX_ERROR_INVALID_ESCAPE,            // invalid escape sequence inside string
    CJSONX_ERROR_INVALID_NUMBER,            // number format is invalid
    CJSONX_ERROR_INVALID_KEYWORD,           // keyword true, false, or null is misspelled
    CJSONX_ERROR_MAX_DEPTH,                 // maximum nesting depth exceeded
    CJSONX_ERROR_MISSING_COMMA,             // missing comma between elements
    CJSONX_ERROR_MISSING_COLON,             // missing colon after key
    CJSONX_ERROR_TRAILING_COMMA,            // trailing comma is not allowed
    CJSONX_ERROR_UNEXPECTED_TOKEN,          // found unexpected token
    CJSONX_ERROR_UNCLOSED_CONTAINER,        // array or object is not closed
    CJSONX_ERROR_TRAILING_GARBAGE,          // extra data found after root value
    CJSONX_ERROR_INVALID_UTF8               // string contains invalid utf-8 sequences
} cjsonx_error_t;

// convert error code to string
static inline const char* cjsonx_error_string(cjsonx_error_t err) {
    switch (err) {
        case CJSONX_SUCCESS:
            return "success";
        case CJSONX_ERROR_OOM:
            return "out of memory";
        case CJSONX_ERROR_EMPTY_INPUT:
            return "empty input or whitespace only";
        case CJSONX_ERROR_UNTERMINATED_STRING:
            return "unterminated string (missing closing quote)";
        case CJSONX_ERROR_INVALID_CONTROL_CHAR:
            return "invalid raw control character in string";
        case CJSONX_ERROR_INVALID_ESCAPE:
            return "invalid escape sequence inside string";
        case CJSONX_ERROR_INVALID_NUMBER:
            return "invalid json number format";
        case CJSONX_ERROR_INVALID_KEYWORD:
            return "invalid keyword (expected true, false, or null)";
        case CJSONX_ERROR_MAX_DEPTH:
            return "nesting depth limit exceeded";
        case CJSONX_ERROR_MISSING_COMMA:
            return "missing comma separator";
        case CJSONX_ERROR_MISSING_COLON:
            return "missing colon separator";
        case CJSONX_ERROR_TRAILING_COMMA:
            return "trailing comma is not allowed";
        case CJSONX_ERROR_UNEXPECTED_TOKEN:
            return "unexpected token";
        case CJSONX_ERROR_UNCLOSED_CONTAINER:
            return "unclosed container";
        case CJSONX_ERROR_TRAILING_GARBAGE:
            return "trailing garbage after root value";
        case CJSONX_ERROR_INVALID_UTF8:
            return "invalid utf-8 sequence in string";
        default:
            return "unknown error";
    }
}

#endif // cjsonx_error_h
