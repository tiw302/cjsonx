fn main() {
    println!("cargo:rerun-if-changed=../src/cjsonx.c");
    println!("cargo:rerun-if-changed=../include/");
    
    cc::Build::new()
        .file("../src/cjsonx.c")
        .include("../include")
        .define("CJSONX_IMPLEMENTATION", None)
        .compile("cjsonx");
}
