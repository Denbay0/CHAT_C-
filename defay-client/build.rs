fn main() {
    // Re-run build script if the logo changes
    println!("cargo:rerun-if-changed=assets/logo.png");
}
