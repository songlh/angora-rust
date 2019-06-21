ROOT=/home/songlh/workspace/rust/angora-rust
STUB=${ROOT}/tests/mini-rust
BIN=${ROOT}/bin

build()
{
	rustc -g -C codegen-units=1 main.rs
}

build_llvm()
{
	rustc -g -C codegen-units=1 --emit=llvm-bc --emit=link main.rs
}

clean()
{
	rm -rf main *.bc *.ll 
}

test_debuginfo() 
{
	opt -load ${BIN}/rust-debuginfo-pass.so -print-Rust-DebugInfo ./main.bc > /dev/null
}

case $1 in
	build) build;;
	build_llvm) build_llvm;;
	clean)	clean;;
	test_debuginfo) test_debuginfo;;
	*) echo "Please enter a valid option!"; exit;;
esac