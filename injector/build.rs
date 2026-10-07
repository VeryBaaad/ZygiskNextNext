use std::fs;
use std::path::PathBuf;
use std::process::Command;

fn main() {
    println!("cargo:rerun-if-env-changed=ZNN_VERSION");
    let version = std::env::var("ZNN_VERSION")
        .ok()
        .filter(|v| !v.trim().is_empty())
        .unwrap_or_else(|| env!("CARGO_PKG_VERSION").to_string());
    println!("cargo:rustc-env=ZNN_VERSION={version}");

    bpf_object();
}

fn bpf_object() {
    let source = PathBuf::from("bpf/exec.bpf.c");
    let committed = PathBuf::from("bpf/exec.bpf.o");
    let destination = PathBuf::from(std::env::var("OUT_DIR").expect("OUT_DIR is set by cargo"))
        .join("exec.bpf.o");

    println!("cargo:rerun-if-changed={}", source.display());
    println!("cargo:rerun-if-changed={}", committed.display());
    println!("cargo:rerun-if-env-changed=BPF_CLANG");

    let fall_back = |reason: &str| {
        println!(
            "cargo:warning={reason}: embedding the committed {}",
            committed.display()
        );
        fs::copy(&committed, &destination)
            .unwrap_or_else(|error| panic!("cannot use {}: {error}", committed.display()));
    };

    let Some(clang) = find_bpf_clang() else {
        fall_back("no clang with a BPF backend found");
        return;
    };

    let compiled = Command::new(&clang)
        .args(["-O2", "-g", "-target", "bpf", "-c"])
        .arg(&source)
        .arg("-o")
        .arg(&destination)
        .status();
    match compiled {
        Ok(status) if status.success() => {}
        other => fall_back(&format!(
            "{clang} failed to compile the BPF source ({other:?})"
        )),
    }
}

fn find_bpf_clang() -> Option<String> {
    if let Ok(explicit) = std::env::var("BPF_CLANG")
        && !explicit.trim().is_empty()
    {
        return Some(explicit);
    }
    for candidate in [
        "clang", "clang-21", "clang-20", "clang-19", "clang-18", "clang-17", "clang-16",
    ] {
        let Ok(targets) = Command::new(candidate).args(["-print-targets"]).output() else {
            continue;
        };
        if !targets.status.success() {
            continue;
        }
        if String::from_utf8_lossy(&targets.stdout).contains("bpf") {
            return Some(candidate.to_owned());
        }
    }
    None
}
