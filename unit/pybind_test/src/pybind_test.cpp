#include "pybind_test.h"

#include <dlfcn.h>

#include "python_shim.h"

using namespace unit::pybind_test;

pthread_shim_table_t relocations = {
    .pthread_getspecific = &::pthread_getspecific,
    .pthread_setspecific = &::pthread_setspecific,
    .pthread_key_create = &::pthread_key_create,
    .pthread_key_delete = &::pthread_key_delete,
    .pthread_create = &::pthread_create,
    .setlocale = &::setlocale,
};

pybind_test::pybind_test(const Args &args,
                         const std::optional<std::string_view> &name_override)
    : unit::pybind_test::Base(args, name_override), pub(args.pub) {
  void *pyhandle = dlmopen(LM_ID_NEWLM, "libpython_shim.so",
                           RTLD_LAZY | RTLD_LOCAL | RTLD_DEEPBIND);

  auto shim_table_ptr =
      (pthread_shim_table_t **)dlsym(pyhandle, "shim_pthread_table");
  *shim_table_ptr = &relocations;

#define X_PY(f)                                                                \
  this->f = reinterpret_cast<decltype(this->f)>(dlsym(pyhandle, #f));          \
  if (!this->f) {                                                              \
    const char *error = dlerror();                                             \
    BASIS_LOG_FATAL("error while getting '" #f "': {}'", error);               \
  }
  X_PYTHON_API
#undef X_PY

  Py_InitializeEx(0);

  PyRun_SimpleStringFlags("print('Running on main thread')", NULL);

  py_saved_thread_state = PyEval_SaveThread();

  std::cout << "saved!" << std::endl;
  std::thread t = std::thread([this]() {
    PyGILState_STATE g = PyGILState_Ensure();

    PyRun_SimpleString("print('Kicking off code on a thread!')");

    PyRun_SimpleString(R"(
import threading
import time

import numpy as np

np.array(50)

def do():
  while True:
      time.sleep(1)
      print('This is a callback from a Python thread.')
      
t = threading.Thread(target = do)
t.daemon = True
t.start()
)");
    PyGILState_Release(g);
  });
  t.join();
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
