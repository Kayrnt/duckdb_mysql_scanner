use build_script::cargo_rerun_if_changed;
use std::path::PathBuf;
use std::{env, path::Path};

fn main() {
    let duckdb_root = Path::new(&env::var("CARGO_MANIFEST_DIR").unwrap())
        .join("duckdb")
        .canonicalize()
        .expect("duckdb source root");

    let header = "src/wrapper.hpp";

    #[cfg(feature = "statically_linked")]
    {
        use build_script::{cargo_rustc_link_lib, cargo_rustc_link_search};
        cargo_rustc_link_lib("duckdb");
        cargo_rustc_link_search(duckdb_root.join("build/debug/src"));
        cargo_rustc_link_search(duckdb_root.join("build/release/src"));
    }

    // Tell cargo to invalidate the built crate whenever the wrapper changes
    cargo_rerun_if_changed(header);

    // The bindgen::Builder is the main entry point
    // to bindgen, and lets you build up options for
    // the resulting bindings.
    let duckdb_include = duckdb_root.join("src/include");
    let bindings = bindgen::Builder::default()
        .header(header)
        .clang_arg("-xc++")
        .clang_arg("-I")
        .clang_arg(duckdb_include.to_string_lossy())
        .derive_debug(true)
        .derive_default(true)
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        .generate()
        .expect("Unable to generate bindings");

    // Write the bindings to the $OUT_DIR/bindings.rs file.
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");

    cc::Build::new()
        .include(duckdb_include)
        .flag_if_supported("-Wno-unused-parameter")
        .flag_if_supported("-Wno-redundant-move")
        .flag_if_supported("-std=c++17")
        .cpp(true)
        .file("src/wrapper.cpp")
        .compile("duckdb_extension_framework");
}
