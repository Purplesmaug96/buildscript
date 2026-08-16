import sys
import os

# ANSI colour escape codes
ANSI_RED = "\033[1;31m"
ANSI_GREEN = "\033[32m"
ANSI_YELLOW = "\033[1;33m"
ANSI_CLEAR = "\033[0m"

# Toolchain name
TOOLCHAIN_NAME = "OpenXeChain"
TOOLCHAIN_STEM = f"{ANSI_GREEN}{TOOLCHAIN_NAME}{ANSI_CLEAR}>"

# User-configurable variables
PREFIX="./sysroot" # Sysroot for the toolchain to be installed into
HOST_CC="clang" # Host compiler to use (MUST BE CLANG)
HOST_CXX="clang++"
BUILD_TYPE="Release" # Debug level to build LLVM in
PARALLEL="4" # Number of parallel make jobs to run
LLVM_LINK_JOBS="2" # Max parallel link jobs for LLVM (linking is memory-hungry)
CLEAN="0" # Set to 1 to wipe the build directories before building

# Static variables
LLVM_TARGET="ppc32-xbox360"
NEWLIB_TARGET="ppc-xbox360"

# Build directories. Every component is built in its own directory so that
# re-running this script only rebuilds what actually changed, instead of
# rebuilding the entire toolchain from scratch every time.
BUILD_DIR="./build"
LLVM_BUILD_DIR=f"{BUILD_DIR}/llvm"
XECORELIB_BUILD_DIR=f"{BUILD_DIR}/xecorelib"
XECORELIB_STAGE_DIR=f"{XECORELIB_BUILD_DIR}/stage"
NEWLIB_BUILD_DIR=f"{BUILD_DIR}/newlib"
CRT_BUILD_DIR=f"{BUILD_DIR}/compiler-rt"
LIBCXX_BUILD_DIR=f"{BUILD_DIR}/libcxx"
LIBCXXABI_BUILD_DIR=f"{BUILD_DIR}/libcxxabi"
LIBUNWIND_BUILD_DIR=f"{BUILD_DIR}/libunwind"
SYNTHXEX_BUILD_DIR=f"{BUILD_DIR}/synthxex"
PTHREAD_BUILD_DIR=f"{BUILD_DIR}/pthread"
MESA_BUILD_DIR=f"{BUILD_DIR}/mesa"

ALL_COMPONENTS = ["llvm", "xecorelib", "newlib", "crt", "libcxx", "pthread", "synthxex", "mesa"]

def xe_print(string, end="\n"):
	print(f"{TOOLCHAIN_STEM} {string}", end=end)

def run_cmd(cmd, quiet=False):
	if not quiet:
		xe_print(f"Running command: {cmd}")
	os.system(cmd)

def build_component_llvm() -> int:
	xe_print("Building LLVM... (this may take a while)")
	LLVM_CMAKE_ARGS = f"""
-DCMAKE_C_COMPILER=\"{HOST_CC}\"
-DCMAKE_CXX_COMPILER=\"{HOST_CXX}\"
-DCMAKE_BUILD_TYPE=\"{BUILD_TYPE}\"
-DCMAKE_INSTALL_PREFIX=\"{PREFIX}\"
-DLLVM_ENABLE_PROJECTS=\"lld;clang\"
-DLLVM_TARGETS_TO_BUILD=PowerPC
-DLLVM_DEFAULT_TARGET_TRIPLE=\"{LLVM_TARGET}\"
-DLLVM_INSTALL_BINUTILS_SYMLINKS=true
-DLLVM_INSTALL_CCTOOLS_SYMLINKS=true
-DLLVM_INSTALL_TOOLCHAIN_ONLY=true
-DLLVM_INCLUDE_TESTS=false
-DLLVM_INCLUDE_BENCHMARKS=false
-DLLVM_INCLUDE_EXAMPLES=false
-DLLVM_INCLUDE_DOCS=false
-DLLVM_OPTIMIZED_TABLEGEN=true
-DLLVM_PARALLEL_LINK_JOBS=\"{LLVM_LINK_JOBS}\"
-DCLANG_ENABLE_STATIC_ANALYZER=false
-DCLANG_ENABLE_ARCMT=false
-G "Ninja"
    """.replace('\n', ' ').strip()

	# TODO: Port ccache check from build-toolchain.sh

	# TODO: Port lld check from build-toolchain.sh

	run_cmd(f"cmake -S ./llvm/llvm -B {LLVM_BUILD_DIR} {LLVM_CMAKE_ARGS}")

	xe_print("Building LLVM... (this may take a WHILE)")
	run_cmd(f"cmake --build {LLVM_BUILD_DIR} -j{PARALLEL}")

	xe_print("Installing LLVM... (this may take a while)")
	run_cmd(f"cmake --install {LLVM_BUILD_DIR}")

	xe_print("Cross compiler built and installed!")

	xe_print("Writing initial Clang configuration scripts...")

	with open(f"{PREFIX}/bin/clang.cfg", "w", encoding="utf-8") as f:
		f.write("""
-Wno-main-return-type\n
--sysroot=<CFGDIR>/..\n
--rtlib=compiler-rt\n
-fdeclspec\n
-mlongcall\n
		""")

	with open(f"{PREFIX}/bin/clang++.cfg", "w", encoding="utf-8") as f:
		f.write("""
-Wno-main-return-type\n
--sysroot=<CFGDIR>/..\n
--rtlib=compiler-rt\n
-fdeclspec\n
-mlongcall\n
		""")

	return 0

def build_component_xecorelib() -> int:
	if sys.platform == "linux":
		# Run the xecorelib build script
		run_cmd(f"PREFIX=\"{PREFIX}\" bash \"./xecorelib/install.sh\"")
		# Also install to a staging directory, to build Newlib with it
		run_cmd(f"BINDIR=\"{PREFIX}/bin\" PREFIX=\"{XECORELIB_STAGE_DIR}\" bash \"./xecorelib/install.sh\"")
	else:
		xe_print(f"{ANSI_RED}Unsupported host platform{ANSI_CLEAR}")
		return 1
	return 0

def build_component_newlib() -> int:
	xe_print(f"{ANSI_RED}ERROR: Unimplemented{ANSI_CLEAR}")
	return 1

def build_component_crt() -> int:
	xe_print(f"{ANSI_RED}ERROR: Unimplemented{ANSI_CLEAR}")
	return 1

def build_component_libcxx() -> int:
	xe_print(f"{ANSI_RED}ERROR: Unimplemented{ANSI_CLEAR}")
	return 1

def build_component_pthread() -> int:
	xe_print(f"{ANSI_RED}ERROR: Unimplemented{ANSI_CLEAR}")
	return 1

def build_component_synthxex() -> int:
	xe_print(f"{ANSI_RED}ERROR: Unimplemented{ANSI_CLEAR}")
	return 1

def build_component_mesa() -> int:
	xe_print(f"{ANSI_RED}ERROR: Unimplemented{ANSI_CLEAR}")
	return 1

def build_component(component) -> int:
	if component == "llvm":
		return build_component_llvm()
	elif component == "xecorelib":
		return build_component_xecorelib()
	elif component == "newlib":
		return build_component_newlib()
	elif component == "crt":
		return build_component_crt()
	elif component == "libcxx":
		return build_component_libcxx()
	elif component == "pthread":
		return build_component_pthread()
	elif component == "synthxex":
		return build_component_synthxex()
	elif component == "mesa":
		return build_component_mesa()
	else:
		xe_print(f"{ANSI_RED}Invalid component \"{component}\"{ANSI_CLEAR}")
		return 1

def main(argv, argc) -> int:
	xe_print(f"{ANSI_YELLOW}WARNING: This script is NOT reccomended; please use build-toolchain.sh if you can{ANSI_CLEAR}")

	COMPONENTS = []
	# Apparently argc is 1 because of "python build-toolchain.py"
	#                                 ^^^^^^^^^^^^^^^^^^^^^^^^^^^
	if (argc == 1):
		COMPONENTS = ["llvm", "xecorelib", "newlib", "crt", "libcxx", "pthread", "synthxex", "mesa"]
	else:
		for arg in argv:
			if arg == argv[0]: continue
			if arg in ALL_COMPONENTS:
				COMPONENTS.append(arg)
			else:
				xe_print(f"{ANSI_RED}Invalid component \"{arg}\"{ANSI_CLEAR}")
				return 1

	xe_print(f"Targets: ", end="")
	for component in COMPONENTS:
		print(f"{component}", end="")
		if not (component == COMPONENTS[len(COMPONENTS)-1]):
			print(", ", end="")
	print("")

	for component in COMPONENTS:
		xe_print(f"Building {component}...")
		ret = build_component(component)
		if ret != 0:
			xe_print(f"{ANSI_RED}build_component for {component} returned {ret} {ANSI_CLEAR}")
			return ret


if __name__ == "__main__":
	sys.exit(main(sys.argv, len(sys.argv)))