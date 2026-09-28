import sys
import os
import glob
import shutil
import subprocess

# ANSI colour escape codes
ANSI_RED = "\033[1;31m"
ANSI_GREEN = "\033[32m"
ANSI_YELLOW = "\033[1;33m"
ANSI_CLEAR = "\033[0m"

# Toolchain name
TOOLCHAIN_NAME = "OpenXeChain"
TOOLCHAIN_STEM = f"{ANSI_GREEN}{TOOLCHAIN_NAME}{ANSI_CLEAR}>"

# This script can be run from any directory
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# Windows lacks a POSIX shell for the autotools/script components, and cmd.exe
# has different quoting rules, so commands are run through bash (Git Bash /
# MSYS2) when it is available.  Paths are normalised to forward slashes,
# which all the tools involved (clang, cmake, ninja, make, bash) accept.
IS_WINDOWS = os.name == "nt"

def norm_path(path):
	return path.replace("\\", "/") if IS_WINDOWS else path

if IS_WINDOWS:
	try:
		import ctypes
		kernel32 = ctypes.windll.kernel32
		kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)
	except Exception:
		pass

# User-configurable variables (environment variables override these)
PREFIX = norm_path(os.path.realpath(os.environ.get("PREFIX", f"{SCRIPT_DIR}/sysroot"))) # Sysroot for the toolchain to be installed into
HOST_CC = os.environ.get("HOST_CC", "clang") # Host compiler to use (MUST BE CLANG)
HOST_CXX = os.environ.get("HOST_CXX", "clang++")
BUILD_TYPE = os.environ.get("BUILD_TYPE", "Release") # Debug level to build LLVM in
PARALLEL = os.environ.get("PARALLEL", str(os.cpu_count() or 1)) # Number of parallel make jobs to run
LLVM_LINK_JOBS = os.environ.get("LLVM_LINK_JOBS", "2") # Max parallel link jobs for LLVM (linking is memory-hungry)
CLEAN = os.environ.get("CLEAN", "0") # Set to 1 to wipe the build directories before building
COMPONENT = os.environ.get("COMPONENT", "") # Alternative to positional arguments

# Static variables
LLVM_TARGET = "ppc32-xbox360"
NEWLIB_TARGET = "ppc-xbox360"

# Build directories. Every component is built in its own directory so that
# re-running this script only rebuilds what actually changed, instead of
# rebuilding the entire toolchain from scratch every time.
BUILD_DIR = f"{SCRIPT_DIR}/build"
LLVM_BUILD_DIR = f"{BUILD_DIR}/llvm"
XECORELIB_BUILD_DIR = f"{BUILD_DIR}/xecorelib"
XECORELIB_STAGE_DIR = f"{XECORELIB_BUILD_DIR}/stage"
NEWLIB_BUILD_DIR = f"{BUILD_DIR}/newlib"
CRT_BUILD_DIR = f"{BUILD_DIR}/compiler-rt"
LIBCXX_BUILD_DIR = f"{BUILD_DIR}/libcxx"
LIBCXXABI_BUILD_DIR = f"{BUILD_DIR}/libcxxabi"
LIBUNWIND_BUILD_DIR = f"{BUILD_DIR}/libunwind"
SYNTHXEX_BUILD_DIR = f"{BUILD_DIR}/synthxex"
PTHREAD_BUILD_DIR = f"{BUILD_DIR}/pthread"
MESA_BUILD_DIR = f"{BUILD_DIR}/mesa"
SDL_BUILD_DIR = f"{BUILD_DIR}/sdl"

# Build log path
BUILD_LOG = f"{SCRIPT_DIR}/build.log"
with open(BUILD_LOG, "w", encoding="utf-8") as _log:
	pass  # Delete the old logs, if they exist

ALL_COMPONENTS = ["llvm", "xecorelib", "newlib", "crt", "libcxx", "pthread", "synthxex", "mesa", "sdl"]

BASH = shutil.which("bash")

def xe_print(string, end="\n"):
	print(f"{TOOLCHAIN_STEM} {string}", end=end)

def fail_build():
	print(f"{TOOLCHAIN_STEM}{ANSI_RED} Failed to build! Check {BUILD_LOG}.{ANSI_CLEAR}")
	sys.exit(1)

def run_cmd(cmd, cwd=None, quiet=False, check=True):
	if not quiet:
		xe_print(f"Running command: {cmd}")
	if IS_WINDOWS and BASH is not None:
		# Run through bash so quoting/glob semantics match a POSIX shell.
		ret = subprocess.call([BASH, "-lc", cmd], cwd=cwd)
	else:
		ret = subprocess.call(cmd, cwd=cwd, shell=True)
	if check and ret != 0:
		fail_build()
	return ret

# Sanitise the environment for the cross compiler: clang honours
# LIBRARY_PATH/C_INCLUDE_PATH/CPLUS_INCLUDE_PATH even in cross builds, so
# saved copies are kept and restored for the host-only parts.
CROSS_ENV_SAVED = False
OLD_LIBRARY_PATH = None
OLD_C_INCLUDE_PATH = None
OLD_CPLUS_INCLUDE_PATH = None

def clear_cross_env():
	global CROSS_ENV_SAVED
	global OLD_LIBRARY_PATH
	global OLD_C_INCLUDE_PATH
	global OLD_CPLUS_INCLUDE_PATH
	if not CROSS_ENV_SAVED:
		OLD_LIBRARY_PATH = os.environ.get("LIBRARY_PATH")
		OLD_C_INCLUDE_PATH = os.environ.get("C_INCLUDE_PATH")
		OLD_CPLUS_INCLUDE_PATH = os.environ.get("CPLUS_INCLUDE_PATH")
		CROSS_ENV_SAVED = True
	os.environ["LIBRARY_PATH"] = ""
	os.environ["C_INCLUDE_PATH"] = ""
	os.environ["CPLUS_INCLUDE_PATH"] = ""

def restore_cross_env():
	if CROSS_ENV_SAVED:
		os.environ["LIBRARY_PATH"] = OLD_LIBRARY_PATH or ""
		os.environ["C_INCLUDE_PATH"] = OLD_C_INCLUDE_PATH or ""
		os.environ["CPLUS_INCLUDE_PATH"] = OLD_CPLUS_INCLUDE_PATH or ""

# Check to make sure all required dependencies are installed
def check_deps() -> bool:
	missing = 0
	# On Windows the POSIX-side tools (make, tar, gawk, ...) come from Git
	# Bash/MSYS2, which this script requires anyway (xecorelib/newlib run
	# shell scripts).  Only enforce the tools that are needed everywhere.
	required = ["clang", "ar", "git", "cmake", "make", "ninja", "bash"]
	if not IS_WINDOWS:
		required = ["clang", "ar", "git", "cmake", "make", "ninja", "python3", "bash",
		            "bzip2", "gzip", "grep", "xargs", "sed", "tar", "unzip", "zip", "gawk"]

	if IS_WINDOWS and BASH is None:
		print(f"{TOOLCHAIN_STEM}{ANSI_RED}bash not found on PATH - install Git Bash or MSYS2{ANSI_CLEAR}")
		missing = 1

	for tool in required:
		if shutil.which(tool) is None:
			missing = 1
			print(f"{TOOLCHAIN_STEM}{ANSI_RED}Missing {tool}!{ANSI_CLEAR}")
	# Zlib cannot be checked like this as it has no binaries, but it should have
	# been installed as a dependency of python3.
	try:
		import yaml  # pyyaml
	except ImportError:
		missing = 1
		print(f"{TOOLCHAIN_STEM}{ANSI_RED}Missing python-pyyaml!{ANSI_CLEAR}")

	if missing != 0:
		print(f"{TOOLCHAIN_STEM}{ANSI_RED}Dependencies are missing! Please install them.{ANSI_CLEAR}")
	return missing == 0

# ---------------------------------------------------------------------------
# Clang configuration scripts
#
# The cross compiler picks up default flags from clang.cfg/clang++.cfg next
# to the clang binary.  Each component appends its own section (Newlib,
# compiler-rt, libc++, pthread).  All helpers here are idempotent so that
# re-running any component never clobbers or duplicates another's section.
# ---------------------------------------------------------------------------
BASE_CFG_LINES = [
	"-Wno-main-return-type",
	"--sysroot=<CFGDIR>/..",
	"--rtlib=compiler-rt",
	"-fdeclspec",
	"-mlongcall",
]

def write_clang_cfg(path):
	"""Ensure the base options are present, preserving any sections that
	later components appended (Newlib/crt/libc++/pthread)."""
	kept = []
	if os.path.exists(path):
		with open(path, "r", encoding="utf-8") as f:
			existing_text = f.read()
		if existing_text.strip() == "":
			existing = []
		else:
			existing = [l.strip() for l in existing_text.splitlines()]
		# Drop any previous copy of the base lines (they may have been
		# written by build-toolchain.sh with or without blank lines), but
		# keep everything else in its original order, so the libc++ include
		# dir stays ahead of the Newlib dirs.
		kept = [l for l in existing if l not in BASE_CFG_LINES and l != ""]
	with open(path, "w", encoding="utf-8") as f:
		f.write("\n".join(BASE_CFG_LINES + kept) + "\n")

def prepend_clang_cfg(path, needle, line):
	"""Insert a line at the top of a config script unless it is present."""
	content = ""
	if os.path.exists(path):
		with open(path, "r", encoding="utf-8") as f:
			content = f.read()
		if needle in content:
			return
	with open(path, "w", encoding="utf-8") as f:
		f.write(line + "\n" + content)

def append_clang_cfg(path, needle, lines):
	"""Append lines to a config script unless they are present."""
	if os.path.exists(path):
		with open(path, "r", encoding="utf-8") as f:
			content = f.read()
		if needle in content:
			return
	with open(path, "a", encoding="utf-8") as f:
		f.write("\n".join(lines) + "\n")

# ---------------------------------------------------------------------------
# Component: llvm - the cross compiler (clang + lld) itself
# ---------------------------------------------------------------------------
def build_component_llvm() -> int:
	xe_print("Configuring the cross compiler... (this may take a while)")
	LLVM_CMAKE_ARGS = [
		"-DCMAKE_C_COMPILER=\"{HOST_CC}\"",
		"-DCMAKE_CXX_COMPILER=\"{HOST_CXX}\"",
		"-DCMAKE_BUILD_TYPE=\"{BUILD_TYPE}\"",
		"-DCMAKE_INSTALL_PREFIX=\"{PREFIX}\"",
		"-DLLVM_ENABLE_PROJECTS=\"lld;clang\"",
		"-DLLVM_TARGETS_TO_BUILD=PowerPC",
		"-DLLVM_DEFAULT_TARGET_TRIPLE=\"{LLVM_TARGET}\"",
		"-DLLVM_INSTALL_BINUTILS_SYMLINKS=true",
		"-DLLVM_INSTALL_CCTOOLS_SYMLINKS=true",
		"-DLLVM_INSTALL_TOOLCHAIN_ONLY=true",
		"-DLLVM_INCLUDE_TESTS=false",
		"-DLLVM_INCLUDE_BENCHMARKS=false",
		"-DLLVM_INCLUDE_EXAMPLES=false",
		"-DLLVM_INCLUDE_DOCS=false",
		"-DLLVM_OPTIMIZED_TABLEGEN=true",
		"-DLLVM_PARALLEL_LINK_JOBS=\"{LLVM_LINK_JOBS}\"",
		"-DCLANG_ENABLE_STATIC_ANALYZER=false",
		"-DCLANG_ENABLE_ARCMT=false",
	]

	# Cache compiled objects with ccache if it is installed, to speed up rebuilds
	if shutil.which("ccache"):
		LLVM_CMAKE_ARGS.append("-DCMAKE_C_COMPILER_LAUNCHER=ccache")
		LLVM_CMAKE_ARGS.append("-DCMAKE_CXX_COMPILER_LAUNCHER=ccache")

	# Link with lld if it is installed, as it is much faster than the default linker
	if shutil.which("ld.lld"):
		LLVM_CMAKE_ARGS.append("-DLLVM_USE_LINKER=lld")

	run_cmd("cmake -S \"{SCRIPT_DIR}/llvm/llvm\" -B {LLVM_BUILD_DIR} {args} -G \"Ninja\"".format(
		SCRIPT_DIR=SCRIPT_DIR, LLVM_BUILD_DIR=LLVM_BUILD_DIR,
		args=" ".join(a.format(HOST_CC=HOST_CC, HOST_CXX=HOST_CXX, BUILD_TYPE=BUILD_TYPE,
		                       PREFIX=PREFIX, LLVM_TARGET=LLVM_TARGET,
		                       LLVM_LINK_JOBS=LLVM_LINK_JOBS) for a in LLVM_CMAKE_ARGS)))

	xe_print("Building the cross compiler... (this may take a WHILE)")
	run_cmd(f"cmake --build {LLVM_BUILD_DIR} -j{PARALLEL}")

	xe_print("Installing the cross compiler... (this may take a while)")
	run_cmd(f"cmake --install {LLVM_BUILD_DIR}")

	xe_print("Cross compiler built and installed!")

	# Define the default command line flags to be used by the cross-compiler.
	# Linkage to Newlib/xecorelib is also added to these files, but after it's
	# been built and installed.
	xe_print("Writing initial Clang configuration scripts...")

	write_clang_cfg(f"{PREFIX}/bin/clang.cfg")
	write_clang_cfg(f"{PREFIX}/bin/clang++.cfg")

	# Clear the environment variables the C/C++ compiler is sensitive to,
	# to avoid pollution. We restore them for the host-side components.
	clear_cross_env()
	return 0

# ---------------------------------------------------------------------------
# Component: xecorelib - the Xbox 360 kernel/XAM import libraries
# ---------------------------------------------------------------------------
def build_component_xecorelib() -> int:
	xe_print("Building and installing xecorelib.")

	# Run the xecorelib build script
	os.environ["PREFIX"] = PREFIX
	run_cmd(f"bash \"{SCRIPT_DIR}/xecorelib/install.sh\"", cwd=XECORELIB_BUILD_DIR)

	# Also install to a staging directory, to build Newlib with it
	os.environ["BINDIR"] = f"{PREFIX}/bin"
	os.environ["PREFIX"] = XECORELIB_STAGE_DIR
	run_cmd(f"bash \"{SCRIPT_DIR}/xecorelib/install.sh\"", cwd=XECORELIB_BUILD_DIR)
	xe_print("Built and installed xecorelib!")
	return 0

# ---------------------------------------------------------------------------
# Component: newlib - the libc for the console
# ---------------------------------------------------------------------------
def build_component_newlib() -> int:
	xe_print("Getting ready to build the Newlib C library.")

	# Configure Newlib
	xe_print("Configuring the Newlib C library... (this may take a while)")

	os.environ["CC"] = f"{PREFIX}/bin/clang -nostdlib -I{XECORELIB_STAGE_DIR}/include"
	os.environ["CPP"] = f"{PREFIX}/bin/clang-cpp"
	os.environ["LD"] = f"{PREFIX}/bin/lld-link"
	os.environ["AR"] = f"{PREFIX}/bin/llvm-ar"
	os.environ["AS"] = f"{PREFIX}/bin/llvm-as"
	os.environ["STRIP"] = f"{PREFIX}/bin/llvm-strip"
	os.environ["RANLIB"] = f"{PREFIX}/bin/llvm-ranlib"

	run_cmd(f"\"{SCRIPT_DIR}/newlib/newlib/configure\" "
	        f"--prefix=\"{PREFIX}\" "
	        f"--host=\"{NEWLIB_TARGET}\" "
	        f"--target=\"{NEWLIB_TARGET}\" "
	        f"--enable-newlib-supplied-syscalls=yes "
	        f"--enable-newlib-mb "
	        f"--enable-newlib-iconv", cwd=NEWLIB_BUILD_DIR)

	# Now build and install
	xe_print("Building the Newlib C library...")
	run_cmd(f"make -j{PARALLEL}", cwd=NEWLIB_BUILD_DIR)

	xe_print("Installing the Newlib C library...")
	run_cmd("make install", cwd=NEWLIB_BUILD_DIR)

	# Add Newlib/xecorelib linkage to the default compiler flags now that it is installed
	xe_print("Adding Newlib linkage to Clang configuration scripts.")

	newlib_section = [
		f"-isystem <CFGDIR>/../{NEWLIB_TARGET}/include",
		"-isystem <CFGDIR>/../include",
		f"-Wl,/libpath:<CFGDIR>/../{NEWLIB_TARGET}/lib,/libpath:<CFGDIR>/../lib",
		"-Wl,/defaultlib:xecorelib.a,/defaultlib:libc.a",
	]
	for cfg in [f"{PREFIX}/bin/clang.cfg", f"{PREFIX}/bin/clang++.cfg"]:
		append_clang_cfg(cfg, "-Wl,/defaultlib:xecorelib.a", newlib_section)

	xe_print("Newlib C library built and installed!")
	return 0

# ---------------------------------------------------------------------------
# Component: crt - compiler-rt builtins
# ---------------------------------------------------------------------------
def build_component_crt() -> int:
	# Configure compiler-rt
	# We need to override the compiler checks, otherwise CMake will attempt to
	# build test programs, which won't work, as it'll try to link compiler-rt,
	# which is not yet installed.
	# LLVM package discovery is disabled, as compiler-rt would pick up the host's
	# LLVM (or its own LTO shared library from the build tree), which cannot be
	# imported on a target platform with no dynamic linking support.
	xe_print("Configuring compiler-rt...")
	run_cmd("cmake -S \"{SCRIPT_DIR}/llvm/compiler-rt\" -B {CRT_BUILD_DIR} "
	        "-DCMAKE_INSTALL_PREFIX=\"{PREFIX}\" "
	        "-DCMAKE_SYSTEM_NAME=\"Generic\" "
	        "-DCMAKE_CROSSCOMPILING=true "
	        "-DCMAKE_C_COMPILER=\"{PREFIX}/bin/clang\" "
	        "-DCMAKE_CXX_COMPILER=\"{PREFIX}/bin/clang++\" "
	        "-DCMAKE_AR=\"{PREFIX}/bin/llvm-ar\" "
	        "-DCMAKE_LINKER=\"{PREFIX}/bin/lld-link\" "
	        "-DCMAKE_RANLIB=\"{PREFIX}/bin/llvm-ranlib\" "
	        "-DCMAKE_SYSROOT=\"{PREFIX}\" "
	        "-DCMAKE_C_COMPILER_WORKS=true "
	        "-DCMAKE_CXX_COMPILER_WORKS=true "
	        "-DCMAKE_C_COMPILER_TARGET=\"ppc32-xbox360\" "
	        "-DCOMPILER_RT_BUILD_BUILTINS=true "
	        "-DCOMPILER_RT_DEFAULT_TARGET_ONLY=true "
	        "-DCOMPILER_RT_BUILD_SANITIZERS=false "
	        "-DCOMPILER_RT_BUILD_XRAY=false "
	        "-DCOMPILER_RT_BUILD_LIBFUZZER=false "
	        "-DCOMPILER_RT_BUILD_PROFILE=false "
	        "-DCOMPILER_RT_STANDALONE_BUILD=true "
	        "-DCOMPILER_RT_BUILTINS_ENABLE_PIC=false "
	        "-DCOMPILER_RT_EXCLUDE_ATOMIC_BUILTIN=false "
	        "-DCOMPILER_RT_BAREMETAL_BUILD=true "
	        "-DCOMPILER_RT_INCLUDE_TESTS=false "
	        "-DCMAKE_DISABLE_FIND_PACKAGE_LLVM=true "
	        "-G \"Ninja\"".format(SCRIPT_DIR=SCRIPT_DIR, CRT_BUILD_DIR=CRT_BUILD_DIR, PREFIX=PREFIX))

	# Now build and install
	xe_print("Building compiler-rt...")
	run_cmd(f"cmake --build {CRT_BUILD_DIR} -j{PARALLEL}")
	xe_print("Installing compiler-rt...")
	run_cmd(f"cmake --install {CRT_BUILD_DIR}")

	# Add compiler-rt linkage to Clang config scripts
	xe_print("Adding compiler-rt linkage to Clang configuration scripts.")

	crt_section = [
		"-Wl,/libpath:<CFGDIR>/../lib/generic",
		"-Wl,/defaultlib:libclang_rt.builtins-powerpc.a",
	]
	for cfg in [f"{PREFIX}/bin/clang.cfg", f"{PREFIX}/bin/clang++.cfg"]:
		append_clang_cfg(cfg, "defaultlib:libclang_rt.builtins-powerpc.a", crt_section)

	xe_print("Compiler-rt built and installed!")

	# The cross-compiled components are done; let the host see its own environment again.
	restore_cross_env()
	return 0

# ---------------------------------------------------------------------------
# Component: libcxx - C++ standard library (libc++ + libc++abi + libunwind)
#
# Mesa's GLSL front-end and many other components are C++, so the console
# needs a C++ standard library: LLVM's libc++, its ABI library (exceptions
# and RTTI) and Itanium unwinding entry points.  LLVM's own libunwind cannot
# be built (the console emits COFF objects, libunwind only supports ELF
# targets), so a small stub implementation ships instead - see
# "${SCRIPT_DIR}/unwind/unwind-stubs.c".
# ---------------------------------------------------------------------------
def setup_libcxx() -> int:
	libcxx_lib = f"{PREFIX}/lib/libc++.a"
	libcxxabi_lib = f"{PREFIX}/lib/libc++abi.a"
	libunwind_lib = f"{PREFIX}/{NEWLIB_TARGET}/lib/libunwind.a"
	cxx_include = f"{PREFIX}/include/c++/v1"

	# The pthread component runs *after* libcxx in ALL_COMPONENTS, but the
	# -D_POSIX_* feature macros it writes into the Clang config scripts are
	# needed to even compile libc++: Newlib's <pthread.h> hides every
	# prototype behind _POSIX_THREADS, and chrono.cpp needs _POSIX_TIMERS for
	# clock_gettime (else: "Monotonic clock not implemented on this
	# platform").  Write them here so the config is right no matter what
	# order the components are built in.  Only the macros - the
	# -Wl,/defaultlib: lines stay with the pthread component so the static
	# link order is unchanged.  Idempotent.
	setup_pthread_cfg_macros()

	# Wire the C++ library into the Clang config scripts as early as
	# possible: clang++ configs are applied in the same order as the files
	# on disk, so the libc++ include dir must come *before* the Newlib
	# dirs.  Config-file flags are processed before any command-line flags,
	# so without this the Newlib <ctype.h> would shadow libc++'s.
	for cfg in [f"{PREFIX}/bin/clang.cfg", f"{PREFIX}/bin/clang++.cfg"]:
		prepend_clang_cfg(cfg, "include/c++/v1", "-isystem <CFGDIR>/../include/c++/v1")
		append_clang_cfg(cfg, "stdlib=libc++", [
			"-stdlib=libc++",
			"-Wl,/defaultlib:libc++.a,/defaultlib:libc++abi.a,/defaultlib:libunwind.a",
		])

	# The unwinding stubs replace LLVM's libunwind: they compile with the
	# console toolchain without any configuration.
	need_unwind = not os.path.exists(libunwind_lib)
	if not need_unwind:
		cutoff = os.path.getmtime(libunwind_lib)
		for src in glob.glob(f"{SCRIPT_DIR}/unwind/*.c"):
			if os.path.getmtime(src) > cutoff:
				need_unwind = True
				break

	if need_unwind:
		xe_print("Building unwinding stubs...")
		run_cmd(f"\"{PREFIX}/bin/clang\" --target=\"{LLVM_TARGET}\" "
		        f"-c -O2 -ffreestanding "
		        f"\"{SCRIPT_DIR}/unwind/unwind-stubs.c\" "
		        f"-o \"{BUILD_DIR}/unwind-stubs.o\"")
		run_cmd(f"\"{PREFIX}/bin/llvm-ar\" rcs \"{libunwind_lib}\" \"{BUILD_DIR}/unwind-stubs.o\"")
		run_cmd(f"\"{PREFIX}/bin/llvm-ranlib\" \"{libunwind_lib}\"")

	if os.path.exists(libcxxabi_lib) and os.path.exists(libcxx_lib) and \
	   os.path.exists(f"{cxx_include}/string"):
		return 0

	xe_print("Configuring libc++...")
	run_cmd("cmake -S \"{SCRIPT_DIR}/llvm/libcxx\" -B {LIBCXX_BUILD_DIR} "
	        "-DCMAKE_TOOLCHAIN_FILE=\"{SCRIPT_DIR}/cmake/ppc-xbox360-toolchain.cmake\" "
	        "-DCMAKE_INSTALL_PREFIX=\"{PREFIX}\" "
	        "-DCMAKE_BUILD_TYPE=\"{BUILD_TYPE}\" "
	        "-DLIBCXX_ENABLE_SHARED=OFF "
	        "-DLIBCXX_ENABLE_STATIC=ON "
	        "-DLIBCXX_USE_COMPILER_RT=ON "
	        "-DLIBCXX_ENABLE_THREADS=ON "
	        "-DLIBCXX_HAS_PTHREAD_API=ON "
	        "-DLIBCXX_ENABLE_MONOTONIC_CLOCK=ON "
	        "-DLIBCXX_CXX_ABI=libcxxabi "
	        "-DLIBCXX_CXX_ABI_INCLUDE_PATHS=\"{SCRIPT_DIR}/llvm/libcxxabi/include\" "
	        "-DCMAKE_CXX_FLAGS=\"-ffreestanding -isystem {SCRIPT_DIR}/llvm/libcxxabi/include\" "
	        "-DLIBCXX_INCLUDE_TESTS=OFF "
	        "-DLIBCXX_INCLUDE_BENCHMARKS=OFF "
	        "-DLIBCXX_ABI_VERSION=2 "
	        "-G \"Ninja\"".format(SCRIPT_DIR=SCRIPT_DIR, LIBCXX_BUILD_DIR=LIBCXX_BUILD_DIR,
	                             PREFIX=PREFIX, BUILD_TYPE=BUILD_TYPE))

	run_cmd(f"cmake --build {LIBCXX_BUILD_DIR} -j{PARALLEL}")

	# The IWYU mapping file is generated by a ninja dependency, but that can
	# race with the install step on this fast machine.  Generate it up front
	# if it is missing.
	if not os.path.exists(f"{LIBCXX_BUILD_DIR}/include/c++/v1/libcxx.imp"):
		run_cmd(f"\"{sys.executable}\" \"{SCRIPT_DIR}/llvm/libcxx/utils/generate_iwyu_mapping.py\" "
		        f"-o \"{LIBCXX_BUILD_DIR}/include/c++/v1/libcxx.imp\"")

	run_cmd(f"cmake --install {LIBCXX_BUILD_DIR}")

	xe_print("Configuring libc++abi...")
	run_cmd("cmake -S \"{SCRIPT_DIR}/llvm/libcxxabi\" -B {LIBCXXABI_BUILD_DIR} "
	        "-DCMAKE_TOOLCHAIN_FILE=\"{SCRIPT_DIR}/cmake/ppc-xbox360-toolchain.cmake\" "
	        "-DCMAKE_INSTALL_PREFIX=\"{PREFIX}\" "
	        "-DCMAKE_BUILD_TYPE=\"{BUILD_TYPE}\" "
	        "-DLIBCXXABI_ENABLE_SHARED=OFF "
	        "-DLIBCXXABI_ENABLE_STATIC=ON "
	        "-DLIBCXXABI_USE_COMPILER_RT=ON "
	        "-DLIBCXXABI_USE_LLVM_UNWINDER=OFF "
	        "-DLIBCXXABI_INCLUDE_TESTS=OFF "
	        "-DLIBCXXABI_LIBCXX_INCLUDES=\"{PREFIX}/include/c++/v1\" "
	        "-DCMAKE_CXX_FLAGS=\"-ffreestanding -isystem {PREFIX}/include/c++/v1\" "
	        "-G \"Ninja\"".format(SCRIPT_DIR=SCRIPT_DIR, LIBCXXABI_BUILD_DIR=LIBCXXABI_BUILD_DIR,
	                             PREFIX=PREFIX, BUILD_TYPE=BUILD_TYPE))

	run_cmd(f"cmake --build {LIBCXXABI_BUILD_DIR} -j{PARALLEL}")
	run_cmd(f"cmake --install {LIBCXXABI_BUILD_DIR}")

	xe_print("C++ standard library built and installed!")
	return 0

def build_component_libcxx() -> int:
	return setup_libcxx()

# ---------------------------------------------------------------------------
# Component: synthxex - host-side XEX packager
# ---------------------------------------------------------------------------
def build_component_synthxex() -> int:
	xe_print("Getting ready to build SynthXEX.")

	# Configure SynthXEX
	xe_print("Configuring SynthXEX...")
	run_cmd("cmake -S \"{SCRIPT_DIR}/synthxex\" -B {SYNTHXEX_BUILD_DIR} "
	        "-DCMAKE_C_COMPILER=\"{HOST_CC}\" "
	        "-DCMAKE_CXX_COMPILER=\"{HOST_CXX}\" "
	        "-DCMAKE_BUILD_TYPE=\"{BUILD_TYPE}\" "
	        "-DCMAKE_INSTALL_PREFIX=\"{PREFIX}\" "
	        "-G \"Ninja\"".format(SCRIPT_DIR=SCRIPT_DIR, SYNTHXEX_BUILD_DIR=SYNTHXEX_BUILD_DIR,
	                             HOST_CC=HOST_CC, HOST_CXX=HOST_CXX,
	                             BUILD_TYPE=BUILD_TYPE, PREFIX=PREFIX))

	# Now build and install
	xe_print("Building SynthXEX...")
	run_cmd(f"cmake --build {SYNTHXEX_BUILD_DIR} -j{PARALLEL}")
	xe_print("Installing SynthXEX...")
	run_cmd(f"cmake --install {SYNTHXEX_BUILD_DIR}")

	xe_print("SynthXEX built and installed!")
	return 0

# ---------------------------------------------------------------------------
# Component: pthread - single-threaded POSIX threads shim
#
# The Xbox 360 kernel has no thread API for games, so threads cannot be
# spawned from C.  This shim satisfies the pthread symbols Mesa references
# at link time (its c11 threads wrapper and util code compile against
# full POSIX).  It also defines the POSIX feature macros Newlib needs to
# declare the pthread types/functions, and links libpthread.a into every
# image via the Clang config scripts - exactly what the Mesa CMake port
# expects from "the toolchain" (see cmake/mesa-checks.cmake).
# ---------------------------------------------------------------------------
# The POSIX feature macros Newlib's <pthread.h> and libc++'s chrono.cpp need
# to see the pthread prototypes and clock_gettime().  These are plain -D
# flags with no dependency on the shim library, so they are kept separate
# from the link line: setup_libcxx() writes them before libc++ is compiled
# (the pthread component runs after libcxx), while -Wl,/defaultlib: must stay
# where the pthread component puts it.  Idempotent across re-runs.
PTHREAD_FEATURE_MACROS = [
	"-D_POSIX_THREADS",
	"-D_POSIX_BARRIERS",
	"-D_POSIX_READER_WRITER_LOCKS",
	"-D_POSIX_TIMEOUTS",
	"-D_UNIX98_THREAD_MUTEX_ATTRIBUTES",
	"-D_POSIX_CLOCK_SELECTION",
	"-D_POSIX_MONOTONIC_CLOCK",
	"-D_POSIX_TIMERS",
]

def setup_pthread_cfg_macros() -> int:
	for cfg in [f"{PREFIX}/bin/clang.cfg", f"{PREFIX}/bin/clang++.cfg"]:
		append_clang_cfg(cfg, "-D_POSIX_THREADS", PTHREAD_FEATURE_MACROS)
	return 0

def setup_pthread_shim() -> int:
	pthread_lib = f"{PREFIX}/{NEWLIB_TARGET}/lib/libpthread.a"
	cfg_line = "libpthread.a"

	# Compile and archive the shim if it is stale or missing.  The shim is
	# tiny, so recompiling it whenever it changed under the source dir is fine.
	need_build = not os.path.exists(pthread_lib)
	if not need_build:
		cutoff = os.path.getmtime(pthread_lib)
		for pat in [f"{SCRIPT_DIR}/pthread/*.c", f"{SCRIPT_DIR}/pthread/*.S"]:
			for src in glob.glob(pat):
				if os.path.getmtime(src) > cutoff:
					need_build = True
					break

	if need_build:
		xe_print("Building the single-threaded pthread shim.")

		os.makedirs(PTHREAD_BUILD_DIR, exist_ok=True)
		for obj in glob.glob(f"{PTHREAD_BUILD_DIR}/*.o"):
			os.unlink(obj)

		for src in sorted(glob.glob(f"{SCRIPT_DIR}/pthread/*.c")):
			out = os.path.join(PTHREAD_BUILD_DIR, os.path.splitext(os.path.basename(src))[0] + ".o")
			run_cmd(f"\"{PREFIX}/bin/clang\" -c -O2 -ffreestanding "
			        f"-I\"{SCRIPT_DIR}/pthread\" \"{src}\" -o \"{out}\"")
		for src in sorted(glob.glob(f"{SCRIPT_DIR}/pthread/*.S")):
			out = os.path.join(PTHREAD_BUILD_DIR, os.path.splitext(os.path.basename(src))[0] + ".o")
			run_cmd(f"\"{PREFIX}/bin/clang\" -c -O2 -ffreestanding "
			        f"-I\"{SCRIPT_DIR}/pthread\" \"{src}\" -o \"{out}\"")

		os.makedirs(f"{PREFIX}/{NEWLIB_TARGET}/lib", exist_ok=True)
		run_cmd(f"\"{PREFIX}/bin/llvm-ar\" rcs \"{pthread_lib}\" \"{PTHREAD_BUILD_DIR}\"/*.o")
		run_cmd(f"\"{PREFIX}/bin/llvm-ranlib\" \"{pthread_lib}\"")

		# Install the headers the shim supplies on top of Newlib's (syslog).
		if os.path.exists(f"{SCRIPT_DIR}/pthread/syslog.h"):
			shutil.copy2(f"{SCRIPT_DIR}/pthread/syslog.h",
			             f"{PREFIX}/{NEWLIB_TARGET}/include/syslog.h")

	# The feature macros first (setup_libcxx() may already have written
	# them), then the libpthread/libm default link.  Idempotent across
	# re-runs.
	setup_pthread_cfg_macros()
	for cfg in [f"{PREFIX}/bin/clang.cfg", f"{PREFIX}/bin/clang++.cfg"]:
		append_clang_cfg(cfg, "defaultlib:libpthread.a",
		                 [f"-Wl,/defaultlib:{cfg_line}"])
		append_clang_cfg(cfg, "defaultlib:libm.a",
		                 ["-Wl,/defaultlib:libm.a"])

	return 0

def build_component_pthread() -> int:
	ret = setup_pthread_shim()
	if ret == 0:
		xe_print("pthread shim built and installed!")
	return ret

# ---------------------------------------------------------------------------
# Component: mesa - the xbox360 (softpipe) GL backend, cross-compiled for the
# console and installed into the sysroot (libxbox360.a, GL/gl.h,
# xbox360/xbox360_api.h).  This is what enables the samples/gl_* programs.
# ---------------------------------------------------------------------------
def build_component_mesa() -> int:
	xe_print("Getting ready to build Mesa (xbox360 backend).")

	# Mesa's GL stack is C++ (the GLSL front-end), so install the C++ standard
	# library first (no-op if it is already installed).
	setup_libcxx()
	setup_pthread_shim()

	# Mesa targets the console, so the cross compiler's environment must be
	# clean too (clang honours LIBRARY_PATH/C_INCLUDE_PATH even when the
	# target is ppc32-xbox360).
	clear_cross_env()

	xe_print("Configuring Mesa...")
	run_cmd("cmake -S \"{SCRIPT_DIR}/mesa\" -B {MESA_BUILD_DIR} "
	        "-DCMAKE_TOOLCHAIN_FILE=\"{SCRIPT_DIR}/cmake/ppc-xbox360-toolchain.cmake\" "
	        "-DCMAKE_INSTALL_PREFIX=\"{PREFIX}\" "
	        "-DXECHAIN_SYSROOT=\"{PREFIX}\" "
	        "-DCMAKE_BUILD_TYPE=\"{BUILD_TYPE}\" "
	        "-DMESA_OP_GALLIUM_DRIVERS=\"softpipe;xbox360\" "
	        "-DMESA_OP_GLX=disabled "
	        "-DMESA_OP_EGL=OFF "
	        "-DMESA_OP_LLVM=disabled "
	        "-DMESA_OP_SPIRV_TOOLS=disabled "
	        "-DMESA_OP_ZSTD=disabled "
	        "-DX360_RING_REV=\"{X360_RING_REV}\" "
	        "-DX360_RING_ROT=\"{X360_RING_ROT}\" "
	        "-DX360_CPU_PROBE_VSX=\"{X360_CPU_PROBE_VSX}\" "
	        "-G \"Ninja\"".format(SCRIPT_DIR=SCRIPT_DIR, MESA_BUILD_DIR=MESA_BUILD_DIR,
	                             PREFIX=PREFIX, BUILD_TYPE=BUILD_TYPE,
	                             X360_RING_REV=os.environ.get("X360_RING_REV", "OFF"),
	                             X360_RING_ROT=os.environ.get("X360_RING_ROT", "0"),
	                             X360_CPU_PROBE_VSX=os.environ.get("X360_CPU_PROBE_VSX", "OFF")))

	# Build and install
	xe_print("Building Mesa...")
	run_cmd(f"cmake --build {MESA_BUILD_DIR} --target xbox360_merged -- -j{PARALLEL}")
	xe_print("Installing Mesa...")
	run_cmd(f"cmake --install {MESA_BUILD_DIR}")

	xe_print("Mesa built and installed!")
	return 0

# ---------------------------------------------------------------------------
# Component: sdl - SDL3, cross-compiled for the console and installed into the
# sysroot (lib/libSDL3.a, include/SDL3).  The bundled xbox360 video driver
# wraps the scanout + Mesa (libxbox360.a) backend and the xbox360 timer
# backend uses KeQuerySystemTime.  Only the video (xbox360 driver), renderer
# (software) and timers subsystems are enabled.  This is what enables the
# samples/sdl_* programs.
# ---------------------------------------------------------------------------
def build_component_sdl() -> int:
	xe_print("Getting ready to build SDL3 (xbox360 driver).")

	# SDL3 targets the console, so the cross compiler's environment must be
	# clean too (clang honours LIBRARY_PATH/C_INCLUDE_PATH even when the
	# target is ppc32-xbox360).
	clear_cross_env()

	xe_print("Configuring SDL3...")
	run_cmd("cmake -S \"{SCRIPT_DIR}/sdl\" -B {SDL_BUILD_DIR} "
	        "-DCMAKE_TOOLCHAIN_FILE=\"{SCRIPT_DIR}/cmake/ppc-xbox360-toolchain.cmake\" "
	        "-DCMAKE_INSTALL_PREFIX=\"{PREFIX}\" "
	        "-DXECHAIN_SYSROOT=\"{PREFIX}\" "
	        "-DCMAKE_BUILD_TYPE=\"{BUILD_TYPE}\" "
	        "-DSDL_SHARED=OFF "
	        "-DSDL_STATIC=ON "
	        "-DSDL_TEST_LIBRARY=OFF "
	        "-DSDL_EXAMPLES=OFF "
	        "-DSDL_AUDIO=OFF "
	        "-DSDL_VIDEO=ON "
	        "-DSDL_RENDER=ON "
	        "-DSDL_JOYSTICK=OFF "
	        "-DSDL_HAPTIC=OFF "
	        "-DSDL_HIDAPI=OFF "
	        "-DSDL_CAMERA=OFF "
	        "-DSDL_SENSOR=OFF "
	        "-DSDL_POWER=OFF "
	        "-DSDL_GPU=OFF "
	        "-G \"Ninja\"".format(SCRIPT_DIR=SCRIPT_DIR, SDL_BUILD_DIR=SDL_BUILD_DIR,
	                             PREFIX=PREFIX, BUILD_TYPE=BUILD_TYPE))

	# Build and install
	xe_print("Building SDL3...")
	run_cmd(f"cmake --build {SDL_BUILD_DIR} -j{PARALLEL}")
	xe_print("Installing SDL3...")
	run_cmd(f"cmake --install {SDL_BUILD_DIR}")

	xe_print("SDL3 built and installed!")
	return 0

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
	elif component == "sdl":
		return build_component_sdl()
	else:
		xe_print(f"{ANSI_RED}Unknown component \"{component}\"! Valid components: "
		         f"llvm xecorelib newlib crt libcxx pthread synthxex mesa sdl{ANSI_CLEAR}")
		return 1

def main(argv, argc) -> int:
	# Components to build. Each positional argument names a component to build:
	#   python3 build-toolchain.py llvm newlib mesa
	# With no arguments (and no COMPONENT variable), all components are built in
	# dependency order.
	COMPONENTS = []
	if argc > 1:
		COMPONENTS = argv[1:]
	elif COMPONENT != "":
		COMPONENTS = [COMPONENT]
	else:
		COMPONENTS = list(ALL_COMPONENTS)

	for arg in COMPONENTS:
		if arg not in ALL_COMPONENTS:
			xe_print(f"{ANSI_RED}Invalid component \"{arg}\"{ANSI_CLEAR}")
			return 1

	xe_print(f"Targets: ", end="")
	for i, component in enumerate(COMPONENTS):
		print(f"{component}", end="")
		if i != len(COMPONENTS) - 1:
			print(", ", end="")
	print("")

	for submodule in ["newlib", "llvm", "synthxex", "xecorelib", "mesa", "sdl"]:
		if not os.path.isdir(submodule):
			print(f"{TOOLCHAIN_STEM}{ANSI_RED}Submodules are missing! "
			      f"Please re-clone this repository with --recursive. "
				  f"Or: 'git submodule update --init --recursive' should also do the trick.{ANSI_CLEAR}")
			return 1

	if CLEAN == "1":
		xe_print("Wiping build directories.")
		shutil.rmtree(BUILD_DIR, ignore_errors=True)

	# Create the sysroot and build directories, if they don't already exist
	xe_print(f"Creating sysroot directory \"{PREFIX}\".")
	os.makedirs(PREFIX, exist_ok=True)
	for directory in [LLVM_BUILD_DIR, XECORELIB_BUILD_DIR, XECORELIB_STAGE_DIR,
	                  NEWLIB_BUILD_DIR, CRT_BUILD_DIR, LIBCXX_BUILD_DIR,
	                  LIBCXXABI_BUILD_DIR, LIBUNWIND_BUILD_DIR, SYNTHXEX_BUILD_DIR,
	                  PTHREAD_BUILD_DIR, MESA_BUILD_DIR, SDL_BUILD_DIR]:
		os.makedirs(directory, exist_ok=True)

	# Make sure all required dependencies are installed
	xe_print("Checking if required dependencies are installed.")
	if not check_deps():
		return 1

	for component in COMPONENTS:
		xe_print(f"Building {component}...")
		ret = build_component(component)
		if ret != 0:
			xe_print(f"{ANSI_RED}ERROR: build_component for {component} returned {ret} {ANSI_CLEAR}")
			return ret

	# Finished
	xe_print(f"{ANSI_GREEN}{TOOLCHAIN_NAME} has been built successfully.{ANSI_CLEAR}")
	xe_print(f"{ANSI_GREEN}Current installed location: {PREFIX}.{ANSI_CLEAR}")
	xe_print(f"{ANSI_GREEN}This toolchain is portable, it can be moved to any path and still work.{ANSI_CLEAR}")
	xe_print(f"{ANSI_GREEN}Ensure C_INCLUDE_PATH, CPLUS_INCLUDE_PATH, and LIBRARY_PATH{ANSI_CLEAR}")
	xe_print(f"{ANSI_GREEN}are set blank when using the toolchain, or you may encounter{ANSI_CLEAR}")
	xe_print(f"{ANSI_GREEN}interference from the host libraries.{ANSI_CLEAR}")
	xe_print(f"{ANSI_GREEN}Build directories are kept in {BUILD_DIR}, so re-running this{ANSI_CLEAR}")
	xe_print(f"{ANSI_GREEN}script will only rebuild what changed. Set CLEAN=1 to wipe them.{ANSI_CLEAR}")
	return 0

if __name__ == "__main__":
	sys.exit(main(sys.argv, len(sys.argv)))
