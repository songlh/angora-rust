PREFIX     = ../bin
PROGS      = ${PREFIX}/angora-clang ${PREFIX}/unfold-branch-pass.so ${PREFIX}/rust-debuginfo-pass.so

all: test_deps $(PROGS) rule_list dfsan_lib

include Makefile.common

rule_list:
	cp ./custom/angora_abilist.txt ${PREFIX}/.
	cp ./custom/exploitation_list.txt ${PREFIX}/.


dfsan_lib:
	cd dfsan && make

${PREFIX}/angora-clang: angora-clang.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
	ln -sf angora-clang ${PREFIX}/angora-clang++

${PREFIX}/unfold-branch-pass.so: unfold-branch-pass.cc config.h
	$(CXX) $(CLANG_CFL) -shared $< -o $@ $(CLANG_LFL)

${PREFIX}/rust-debuginfo-pass.so: rust-debuginfo-pass.cc
	$(CXX) $(CLANG_CFL) -shared $< -o $@ $(CLANG_LFL)

.NOTPARALLEL: clean

clean:
	rm -rf ${PREFIX}/*
	cd dfsan && make clean
