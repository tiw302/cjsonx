fn main() {
    let src_path = if std::path::Path::new("c_src/cjsonx.c").exists() {
        "c_src/cjsonx.c"
    } else {
        "../src/cjsonx.c"
    };

    let include_path = if std::path::Path::new("c_include").exists() {
        "c_include"
    } else {
        "../include"
    };

    println!("cargo:rerun-if-changed={}", src_path);
    println!("cargo:rerun-if-changed={}", include_path);
    
    cc::Build::new()
        .file(src_path)
        .include(include_path)
        .define("CJSONX_IMPLEMENTATION", None)
        .compile("cjsonx");
}
