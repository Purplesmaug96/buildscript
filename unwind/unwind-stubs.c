//
// Itanium unwinding stubs for the xbox360 console.
//
// The libc++ ABI library (exception support) needs the _Unwind_* entry
// points that an unwinder would provide.  LLVM's libunwind cannot be built:
// the console compiler emits COFF objects while libunwind's configuration
// only accepts ELF targets (with the exception of classic PE/COFF hosts).
// These stubs fill the gap.  Unwinding is unsupported; any raise aborts
// through std::terminate as quickly as possible.
//

#include <stdint.h>

#include <xecore/xboxkrnl.h>

typedef void *_Unwind_Context;
typedef struct _Unwind_Exception _Unwind_Exception;

typedef enum {
  _URC_NO_REASON = 0,
  _URC_FOREIGN_EXCEPTION_CAUGHT = 1,
  _URC_FATAL_PHASE1_ERROR = 2,
  _URC_FATAL_PHASE2_ERROR = 3,
  _URC_END_OF_STACK = 5,
  _URC_HANDLER_FOUND = 6,
  _URC_INSTALL_CONTEXT = 7,
  _URC_CONTINUE_UNWIND = 8,
} _Unwind_Reason_Code;

typedef void (*_Unwind_Trace_Fn)(_Unwind_Context *context, void *arg);

struct _Unwind_Exception {
  uintptr_t exception_class;
  void (*exception_cleanup)(_Unwind_Reason_Code reason, _Unwind_Exception *e);
  uintptr_t private_1;
  uintptr_t private_2;
};

_Unwind_Reason_Code _Unwind_RaiseException(_Unwind_Exception *e)
{
  (void)e;
  DbgPrint("_Unwine_RaiseException: C++ `throw` attempted (unwinding unsupported); "
           "trap for diagnostics");
  __asm__ __volatile__("tw 31, 0, 0");
  return _URC_END_OF_STACK;
}

_Unwind_Reason_Code _Unwind_ForcedUnwind(_Unwind_Exception *e,
                                         void (*stop)(int, _Unwind_Exception *))
{
  (void)e;
  (void)stop;
  return _URC_END_OF_STACK;
}

void _Unwind_Resume(_Unwind_Exception *e)
{
  /* Real unwinding is unsupported on this platform.  This is a hard
   * failure point: a C++ exception escaped into code that expects a
   * handler, or terminate() was reached; either way the module cannot
   * continue saneily.  Say so, distinctly, instead of spinning forever
   * (the old silent loop made hangs indistinguishable from slow boots). */
  DbgPrint("_Unwind_Resume: _Unwind_Resume: C++ exception escaping without a "
           "matching handler");
  __asm__ __volatile__("tw 31, 0, 0"); /* trap; Xenia logs the fault */
  for (;;) {
  }
}

_Unwind_Reason_Code _Unwind_Backtrace(_Unwind_Trace_Fn fn, void *arg)
{
  (void)fn;
  (void)arg;
  return _URC_END_OF_STACK;
}

_Unwind_Reason_Code _Unwind_FindEnclosingFunction(void *pc, void **fptr)
{
  (void)pc;
  (void)fptr;
  return _URC_END_OF_STACK;
}

void _Unwind_DeleteException(_Unwind_Exception *e)
{
  (void)e;
}

uintptr_t _Unwind_GetIP(_Unwind_Context *context)
{
  (void)context;
  return 0;
}

void _Unwind_SetIP(_Unwind_Context *context, uintptr_t value)
{
  (void)context;
  (void)value;
}

uintptr_t _Unwind_GetCFA(_Unwind_Context *context)
{
  (void)context;
  return 0;
}

uintptr_t _Unwind_GetGR(_Unwind_Context *context, int index)
{
  (void)context;
  (void)index;
  return 0;
}

void _Unwind_SetGR(_Unwind_Context *context, int index, uintptr_t value)
{
  (void)context;
  (void)index;
  (void)value;
}

void *_Unwind_GetLanguageSpecificData(_Unwind_Context *context)
{
  (void)context;
  return 0;
}

uintptr_t _Unwind_GetRegionStart(_Unwind_Context *context)
{
  (void)context;
  return 0;
}

uintptr_t _Unwind_GetDataRelBase(_Unwind_Context *context)
{
  (void)context;
  return 0;
}

uintptr_t _Unwind_GetTextRelBase(_Unwind_Context *context)
{
  (void)context;
  return 0;
}