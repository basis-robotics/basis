#include "pybind_test.h"
#include "basis/core/logging/macros.h"
#include "internal/pycore_pystate.h"
// #include "internal/pycore_pystate.h"
// #include "pystate.h"
#include <link.h>

#include <memory>

#include <dlfcn.h>

#include "plthook.h"

using namespace unit::pybind_test;
extern "C" int safe_isdigit(int c) { return c >= '0' && c <= '9'; }
static std::thread t;
extern "C" const unsigned short** __ctype_b_loc();
extern "C" struct lconv* localeconv();

pybind_test::pybind_test(const Args &args,
                         const std::optional<std::string_view> &name_override)
    : unit::pybind_test::Base(args, name_override), pub(args.pub) {

  // 2. Initialize Python in the global lib (sets up TLS in host thread)
  void *pyhandle =
      dlmopen(LM_ID_NEWLM, "/usr/lib/aarch64-linux-gnu/libpython3.8d.so",
              RTLD_LAZY | RTLD_LOCAL | RTLD_DEEPBIND);
  // void *pyhandle =
  //     dlmopen(LM_ID_NEWLM, "libpython3.8.so",
  //             RTLD_LAZY | RTLD_LOCAL | RTLD_DEEPBIND);

  plthook_t *plthook;
  plthook_open_by_handle(&plthook, pyhandle); // your dlmopen handle
  // Replace thread local storage so that new threads can get the correct python state
  plthook_replace(plthook, "pthread_getspecific", (void *)pthread_getspecific,
                  NULL);
  plthook_replace(plthook, "pthread_setspecific", (void *)pthread_setspecific,
                  NULL);
  plthook_replace(plthook, "pthread_key_create", (void *)pthread_key_create,
                  NULL);
  plthook_replace(plthook, "pthread_key_delete", (void *)pthread_key_delete,
                  NULL);
  // Fix threading.thread
  plthook_replace(plthook, "pthread_create", (void *)pthread_create,
                  NULL);
  // Fix weird crash on other threads with locales

plthook_replace(plthook, "__ctype_b_loc", (void*)__ctype_b_loc, nullptr);
plthook_replace(plthook, "localeconv", (void*)localeconv, nullptr);
  plthook_open(&plthook, "/usr/lib/python3.8/lib-dynload/_decimal.cpython-38-aarch64-linux-gnu.so");
plthook_replace(plthook, "__ctype_b_loc", (void*)__ctype_b_loc, nullptr);
plthook_close(plthook);

plthook_open(&plthook, "/usr/lib/aarch64-linux-gnu/libmpdec.so.2.4.2");
plthook_replace(plthook, "__ctype_b_loc", (void*)__ctype_b_loc, nullptr);
plthook_close(plthook);
  
  /*
  plthook_replace(plthook, "__ctype_b_loc", (void *)__ctype_b_loc, NULL);
using setlocale_fn_t = char* (*)(int, const char*);
auto setlocale_fn = (setlocale_fn_t)dlsym(pyhandle, "setlocale");
if (setlocale_fn) {
    setlocale_fn(LC_ALL, "C");
}
auto __ctype_b_loc_fn = (const unsigned short** (*)())dlsym(pyhandle, "__ctype_b_loc");
const unsigned short** table_ptr = __ctype_b_loc_fn();
printf("__ctype_b_loc()[61] = 0x%x\n", table_ptr ? (*table_ptr)[61] : 0);
*/
// plthook_replace(plthook, "localtime_r", (void*)localtime_r, nullptr);
// plthook_replace(plthook, "tzset", (void*)tzset, nullptr);
  // dlsym symbols

  //    void* pyhandle = dlmopen(LM_ID_NEWLM,
  //    "/usr/lib/aarch64-linux-gnu/libpython3.8d.so", RTLD_NOW | RTLD_LOCAL |
  //    RTLD_DEEPBIND);

#define X_PY(f)                                                                \
  this->f = reinterpret_cast<decltype(this->f)>(dlsym(pyhandle, #f));          \
  if (!this->f) {                                                              \
    const char *error = dlerror();                                             \
    BASIS_LOG_FATAL("error while getting '" #f "': {}'",                       \
                    error ? error : " none?! ");                               \
  }
  X_PYTHON_API
#undef X_PY
  
  std::cout << "thread state" << _PyRuntime->gilstate.autoTSSkey._is_initialized
            << std::endl;
  Py_InitializeEx(0);
  
  std::cout << "thread state" << _PyRuntime->gilstate.autoTSSkey._is_initialized
            << std::endl;
  std::cout << "thread state"
            << PyThread_tss_get(&_PyRuntime->gilstate.autoTSSkey) << std::endl;

  PyRun_SimpleStringFlags("print('Running on main thread')", NULL);

  PyThreadState *main_tstate = PyEval_SaveThread();

  std::cout << "saved!" << std::endl;
  t = std::thread([&, this]() {
    PyGILState_STATE g = PyGILState_Ensure();


{
  auto handle = pyhandle;
  auto localeconv_fn = (struct lconv* (*)())dlsym(handle, "localeconv");
auto fprintf_fn = (int (*)(FILE*, const char*, ...))dlsym(handle, "fprintf");
auto stderr_ptr = (FILE**)dlsym(handle, "stderr");

struct lconv* lc = localeconv_fn();
fprintf_fn(*stderr_ptr, "decimal_point = '%s'\n", lc->decimal_point);

}


    PyRun_SimpleString("print('hi!')");
    PyRun_SimpleString(R"(
import threading
import time
#import numpy
import datetime
from decimal import Decimal
print("dec:")
print(Decimal('123.45'))
print("done")
from dateutil import parser
from datetime import datetime
from datetime import datetime, timezone
print(datetime.now(timezone.utc))
import decimal
print(decimal.__file__)

#iso_datetime_tz = "2023-10-27T12:34:56+02:00"
#datetime_obj_tz_dateutil = parser.parse(iso_datetime_tz)
#print(datetime_obj_tz_dateutil)
def do():
    while True:
        time.sleep(1)
        print('This is a callback from a Python thread.')
        
t = threading.Thread(target = do)
t.daemon = True
t.start()
    )");
    PyGILState_Release(g);
    std::cout << "release" << std::endl;
  });
  t.join();
  std::cout << "joined" << std::endl;
}

std::atomic<int> t_count{};
InprocTest::Output pybind_test::InprocTest(const InprocTest::Input &input) {
  BASIS_LOG_INFO("Got an inproc trigger");
  PyGILState_STATE g = PyGILState_Ensure();
  t_count++;
  BASIS_LOG_INFO("Current t_count {}", (int)t_count);
  PyRun_SimpleStringFlags("print('Got a trigger from another unit')", NULL);
  t_count--;
  PyGILState_Release(g);

  return {};
}

InprocTestTrigger::Output
pybind_test::InprocTestTrigger(const InprocTestTrigger::Input &input) {
  // static_assert(false, "Implement me");
  PyGILState_STATE g = PyGILState_Ensure();

  PyRun_SimpleStringFlags("print('InprocTestTrigger()')", NULL);
  if (pub) {
    PyRun_SimpleStringFlags("print('Sending a trigger to another unit()')",
                            NULL);
    PyGILState_Release(g);

    return {std::make_shared<std::string>("test")};
  }
  PyGILState_Release(g);

  return {};
}

StereoMatch::Output pybind_test::StereoMatch(const StereoMatch::Input &input) {
  // static_assert(false, "Implement me");
  return {};
}
unit::pybind_test::TimeTest::Output
pybind_test::TimeTest(const unit::pybind_test::TimeTest::Input &input) {
  // static_assert(false, "Implement me");
  return {};
}
ApproxTest::Output pybind_test::ApproxTest(const ApproxTest::Input &input) {
  // static_assert(false, "Implement me");
  return {};
}
