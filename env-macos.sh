# Apple clang lacks the RISC-V target, and Homebrew clang defaults to Apple's Mach-O ld
# instead of lld so source this first before running run.sh.
export CC=/opt/homebrew/opt/llvm/bin/clang
export EXTRA_CFLAGS="-fuse-ld=lld"
