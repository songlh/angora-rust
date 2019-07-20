/*
  The code is modified from AFL's LLVM mode.
  Angora did some minor modification on it, including:
  - add taint tracking arguments.
  - use angora's llvm passs.

   ------------------------------------------------

   Written by Laszlo Szekeres <lszekeres@google.com> and
              Michal Zalewski <lcamtuf@google.com>

   Copyright 2015, 2016 Google Inc. All rights reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

 */

#define ANGORA_MAIN

#include "alloc-inl.h"
#include "config.h"
#include "debug.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static u8 *obj_path;       /* Path to runtime libraries         */
static u8 **cc_params;     /* Parameters passed to the real CC  */
static u32 cc_par_cnt = 1; /* Param count, including argv0      */
static u8 clang_type = CLANG_FAST_TYPE;
static u8 is_cxx = 0;

/* Try to find the runtime libraries. If that fails, abort. */
static void find_obj(u8 *argv0) {

	u8 *slash, *tmp;
	slash = strrchr(argv0, '/');

	if (slash) {
		u8 *dir;
		*slash = 0;
		dir = ck_strdup(argv0);
		*slash = '/';

		tmp = alloc_printf("%s/angora-llvm-pass.so", dir);
		if (!access(tmp, R_OK)) {
			obj_path = dir;
			ck_free(tmp);
			return;
		}

		ck_free(tmp);
		ck_free(dir);
	}

	FATAL("Unable to find 'angora-llvm-pass.so'");
}

static void check_type(char *name) {
	u8 *use_fast = getenv("USE_FAST");
	u8 *use_dfsan = getenv("USE_DFSAN");
	u8 *use_track = getenv("USE_TRACK");
	u8 *use_pin = getenv("USE_PIN");
	if (use_fast) {
		clang_type = CLANG_FAST_TYPE;
	} else if (use_dfsan) {
		clang_type = CLANG_DFSAN_TYPE;
	} else if (use_track) {
		clang_type = CLANG_TRACK_TYPE;
	} else if (use_pin) {
		clang_type = CLANG_PIN_TYPE;
	}
	if (!strcmp(name, "angora-clang++")) {
		is_cxx = 1;
	}
}

static void add_angora_pass() {
	if (clang_type != CLANG_DFSAN_TYPE) {
		cc_params[cc_par_cnt++] = "-Xclang";
		cc_params[cc_par_cnt++] = "-load";
		cc_params[cc_par_cnt++] = "-Xclang";
		cc_params[cc_par_cnt++] = alloc_printf("%s/unfold-branch-pass.so", obj_path);
	}

	cc_params[cc_par_cnt++] = "-Xclang";
	cc_params[cc_par_cnt++] = "-load";
	cc_params[cc_par_cnt++] = "-Xclang";
	cc_params[cc_par_cnt++] = alloc_printf("%s/angora-llvm-pass.so", obj_path);

	if (clang_type == CLANG_DFSAN_TYPE) {
		cc_params[cc_par_cnt++] = "-mllvm";
		cc_params[cc_par_cnt++] = "-DFSanMode";
	} else if (clang_type == CLANG_TRACK_TYPE || clang_type == CLANG_PIN_TYPE) {
		cc_params[cc_par_cnt++] = "-mllvm";
		cc_params[cc_par_cnt++] = "-TrackMode";
	}

	cc_params[cc_par_cnt++] = "-mllvm";
	cc_params[cc_par_cnt++] = alloc_printf("-angora-dfsan-abilist=%s/angora_abilist.txt", obj_path);
	cc_params[cc_par_cnt++] = "-mllvm";
	cc_params[cc_par_cnt++] = alloc_printf("-angora-dfsan-abilist=%s/dfsan_abilist.txt", obj_path);
	cc_params[cc_par_cnt++] = "-mllvm";
	cc_params[cc_par_cnt++] = alloc_printf("-angora-exploitation-list=%s/exploitation_list.txt", obj_path);

	char *rule_list = getenv(TAINT_RULE_LIST_VAR);
	if (rule_list) {
		printf("rule_list : %s\n", rule_list);
		cc_params[cc_par_cnt++] = "-mllvm";
		cc_params[cc_par_cnt++] = alloc_printf("-angora-dfsan-abilist=%s", rule_list);
	}
}

static void add_angora_runtime() {
  // cc_params[cc_par_cnt++] = "-I/${HOME}/clang+llvm/include/c++/v1";
	if (clang_type == CLANG_FAST_TYPE) {
		cc_params[cc_par_cnt++] = alloc_printf("%s/libruntime_fast.a", obj_path);
	} else if (clang_type == CLANG_TRACK_TYPE || clang_type == CLANG_DFSAN_TYPE) {
		cc_params[cc_par_cnt++] = "-Wl,--whole-archive";
		cc_params[cc_par_cnt++] = alloc_printf("%s/DFSanRT.a", obj_path);
		cc_params[cc_par_cnt++] = "-Wl,--no-whole-archive";
		cc_params[cc_par_cnt++] = alloc_printf("-Wl,--dynamic-list=%s/DFSanRT.a.syms", obj_path);

		cc_params[cc_par_cnt++] = alloc_printf("%s/libruntime.a", obj_path);
		cc_params[cc_par_cnt++] = alloc_printf("%s/io-func.o", obj_path);
		cc_params[cc_par_cnt++] = alloc_printf("%s/stdalloc.o", obj_path);
		char *rule_obj = getenv(TAINT_CUSTOM_RULE_VAR);
		if (rule_obj) {
			cc_params[cc_par_cnt++] = rule_obj;
		}
	} else if (clang_type == CLANG_PIN_TYPE) {
		cc_params[cc_par_cnt++] = alloc_printf("%s/pin_stub.o", obj_path);
	}

	if (clang_type != CLANG_FAST_TYPE) {
    // cc_params[cc_par_cnt++] = "-pthread";
		if (!is_cxx) {
			cc_params[cc_par_cnt++] = "-lstdc++";
		}
		cc_params[cc_par_cnt++] = "-lrt";
	}

	cc_params[cc_par_cnt++] = "-Wl,--no-as-needed";
	cc_params[cc_par_cnt++] = "-Wl,--gc-sections"; // if darwin -Wl, -dead_strip
	cc_params[cc_par_cnt++] = "-ldl";
	cc_params[cc_par_cnt++] = "-lpthread";
	cc_params[cc_par_cnt++] = "-lm";
}

static void add_dfsan_pass() {
	if (clang_type == CLANG_TRACK_TYPE || clang_type == CLANG_DFSAN_TYPE) {
		cc_params[cc_par_cnt++] = "-Xclang";
		cc_params[cc_par_cnt++] = "-load";
		cc_params[cc_par_cnt++] = "-Xclang";
		cc_params[cc_par_cnt++] = alloc_printf("%s/DFSanPass.so", obj_path);
		cc_params[cc_par_cnt++] = "-mllvm";
		cc_params[cc_par_cnt++] = alloc_printf("-angora-dfsan-abilist2=%s/angora_abilist.txt", obj_path);
		cc_params[cc_par_cnt++] = "-mllvm";
		cc_params[cc_par_cnt++] = alloc_printf("-angora-dfsan-abilist2=%s/dfsan_abilist.txt", obj_path);
		char *rule_list = getenv(TAINT_RULE_LIST_VAR);
		if (rule_list) {
			cc_params[cc_par_cnt++] = "-mllvm";
			cc_params[cc_par_cnt++] = alloc_printf("-angora-dfsan-abilist2=%s", rule_list);
		}
	}
}

static void edit_params(u32 argc, char **argv) {

	u8 fortify_set = 0, asan_set = 0, x_set = 0, maybe_linking = 1, bit_mode = 0;
	u8 *name;

	cc_params = ck_alloc((argc + 128) * sizeof(u8 *));

// https: // bugs.llvm.org/show_bug.cgi?id=39321
#if LLVM_VERSION_CODE >= LLVM_VERSION(5, 0)
	setenv("LD_PRELOAD", alloc_printf("%s/llvm-catch-dlclose.so", obj_path), 1);
#endif

	name = strrchr(argv[0], '/');
	if (!name) {	
    	name = argv[0];
    }
	else {
    	name++;
    }
	check_type(name);

 	if (is_cxx) {
		u8 *alt_cxx = getenv("ANGORA_CXX");
		cc_params[0] = alt_cxx ? alt_cxx : (u8 *)"clang++";
	} else {
		u8 *alt_cc = getenv("ANGORA_CC");
		cc_params[0] = alt_cc ? alt_cc : (u8 *)"clang";
	}
  /* Detect stray -v calls from ./configure scripts. */
	if (argc == 1 && !strcmp(argv[1], "-v")) {
		maybe_linking = 0;
	}

	while (--argc) {
		u8 *cur = *(++argv);
		// FIXME
		if (!strcmp(cur, "-O1") || !strcmp(cur, "-O2") || !strcmp(cur, "-O3")) {
			continue;
		}
		
		if (!strcmp(cur, "-m32")) {
			bit_mode = 32;
		}

		if (!strcmp(cur, "-m64")) {
			bit_mode = 64;
		}

		if (!strcmp(cur, "-x")) {
			x_set = 1;
		}

		if (!strcmp(cur, "-c") || !strcmp(cur, "-S") || !strcmp(cur, "-E")) {
			maybe_linking = 0;
		}

		if (!strcmp(cur, "-fsanitize=address") || !strcmp(cur, "-fsanitize=memory")) {
			asan_set = 1;
		}

		if (strstr(cur, "FORTIFY_SOURCE")) {
			fortify_set = 1;
		}

		if (!strcmp(cur, "-shared")) {
			maybe_linking = 0;
		}

		if (!strcmp(cur, "-Wl,-z,defs") || !strcmp(cur, "-Wl,--no-undefined")) {
			continue;
		}

		cc_params[cc_par_cnt++] = cur;
	}

	add_angora_pass();
	add_dfsan_pass();

	cc_params[cc_par_cnt++] = "-pie";
	cc_params[cc_par_cnt++] = "-fpic";
	cc_params[cc_par_cnt++] = "-Qunused-arguments";


	if (getenv("ANGORA_HARDEN")) {
		cc_params[cc_par_cnt++] = "-fstack-protector-all";

		if (!fortify_set) {
			cc_params[cc_par_cnt++] = "-D_FORTIFY_SOURCE=2";
		}
	}

	if (!asan_set && clang_type == CLANG_FAST_TYPE) {
		// We did not test Angora on asan and msan..
		if (getenv("ANGORA_USE_ASAN")) {

			if (getenv("ANGORA_USE_MSAN")) {
				FATAL("ASAN and MSAN are mutually exclusive");
			}

			if (getenv("ANGORA_HARDEN")) {
				FATAL("ASAN and ANGORA_HARDEN are mutually exclusive");
			}

			cc_params[cc_par_cnt++] = "-U_FORTIFY_SOURCE";
			cc_params[cc_par_cnt++] = "-fsanitize=address";

		} else if (getenv("ANGORA_USE_MSAN")) {

			if (getenv("ANGORA_USE_ASAN")) {
				FATAL("ASAN and MSAN are mutually exclusive");
			}

			if (getenv("ANGORA_HARDEN")) {
				FATAL("MSAN and ANGORA_HARDEN are mutually exclusive");
			}

			cc_params[cc_par_cnt++] = "-U_FORTIFY_SOURCE";
			cc_params[cc_par_cnt++] = "-fsanitize=memory";
		}
	}

	if (!getenv("ANGORA_DONT_OPTIMIZE")) {
		cc_params[cc_par_cnt++] = "-g";
		cc_params[cc_par_cnt++] = "-O3";
		cc_params[cc_par_cnt++] = "-funroll-loops";
	}

 

	if (is_cxx) {
		// FIXME: or use the same header
		// cc_params[cc_par_cnt++] = "-I/path-to-llvm/include/c++/v1";
		if (clang_type == CLANG_FAST_TYPE) {
			cc_params[cc_par_cnt++] = alloc_printf("-L%s/libcxx_fast%s/", obj_path, bit_mode==32?"_x86":"");
			cc_params[cc_par_cnt++] = "-stdlib=libc++";
			cc_params[cc_par_cnt++] = "-Wl,--start-group";
			cc_params[cc_par_cnt++] = "-lc++abifast";
			cc_params[cc_par_cnt++] = "-lc++abi";
			cc_params[cc_par_cnt++] = "-Wl,--end-group";
		}
		else if (clang_type == CLANG_TRACK_TYPE) {
			cc_params[cc_par_cnt++] = alloc_printf("-L%s/libcxx_track%s/", obj_path, bit_mode==32?"_x86":"");
			cc_params[cc_par_cnt++] = "-stdlib=libc++";
			cc_params[cc_par_cnt++] = "-Wl,--start-group";
			cc_params[cc_par_cnt++] = "-lc++abitrack";
			cc_params[cc_par_cnt++] = "-lc++abi";
			cc_params[cc_par_cnt++] = "-Wl,--end-group";
		}
	}

	if (maybe_linking) {

		if (x_set) {
			cc_params[cc_par_cnt++] = "-x";
			cc_params[cc_par_cnt++] = "none";
		}

		add_angora_runtime();

		/*switch (bit_mode) {
			case 0:
				break;
			case 32:
				// if (access(cc_params[cc_par_cnt - 1], R_OK)) 
				FATAL("-m32 is not supported by your compiler");
				break;

			case 64:
				// if (access(cc_params[cc_par_cnt - 1], R_OK)) 
				FATAL("-m64 is not supported by your compiler");
				break;
		}*/
	}

	cc_params[cc_par_cnt] = NULL;
}

/* Main entry point */

int main(int argc, char **argv) {

	if (argc < 2) {

		SAYF("\n"
			"This is a helper application for angora-fuzz. It serves as a drop-in "
			"replacement\n"
			"for clang, letting you recompile third-party code with the required "
			"runtime\n"
			"instrumentation. A common use pattern would be one of the "
			"following:\n\n"

			"  CC=%s/angora-clang ./configure\n"
			"  CXX=%s/angora-clang++ ./configure\n\n"

			"In contrast to the traditional angora-clang tool, this version is "
			"implemented as\n"
			"an LLVM pass and tends to offer improved performance with slow "
			"programs.\n\n"

			"You can specify custom next-stage toolchain via ANGORA_CC and "
			"ANGORA_CXX. Setting\n"
			"ANGORA_HARDEN enables hardening optimizations in the compiled "
			"code.\n\n",
			"xx", "xx");

		exit(1);
	}

	find_obj(argv[0]);

	edit_params(argc, argv);
  
	for (int i = 0; i < cc_par_cnt; i++) {
		printf("%s ", cc_params[i]);
	}
	printf("\n");
  
 	
// 	return 0;

	execvp(cc_params[0], (char **)cc_params);

	FATAL("Oops, failed to execute '%s' - check your PATH", cc_params[0]);

	return 0;
}
