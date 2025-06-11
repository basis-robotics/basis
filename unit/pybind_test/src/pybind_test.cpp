#include "pybind_test.h"



using namespace unit::pybind_test;

pybind_test::pybind_test(const Args &args,
                         const std::optional<std::string_view> &name_override)
    : unit::pybind_test::Base(args, name_override), pub(args.pub) {
  
   const char* venv = "/opt/basis/.venv";

    PyConfig config;
    PyConfig_InitPythonConfig(&config);

    // TODO: just use isolated=1?
    config.use_environment = 0;  // Ignore PYTHONPATH etc.
    config.user_site_directory = 0;
    // TODO: this isn't needed is it?
    // TODO: get the proper python location
    config.home = Py_DecodeLocale("/opt/basis/cache/uv/python/cpython-3.12.10-linux-aarch64-gnu/bin", NULL);
    config.prefix = Py_DecodeLocale(venv, NULL);
    config.exec_prefix = Py_DecodeLocale(venv, NULL);
    config.install_signal_handlers = 0;
    

    // "/opt/basis/cache/uv/python/cpython-3.12.10-linux-aarch64-gnu/lib/python3.12" 
    // "/opt/basis/cache/uv/python/cpython-3.12.10-linux-aarch64-gnu/lib/python3.12/lib-dynload"
    // "/opt/basis/.venv/lib/python3.12/site-packages"
  PyWideStringList_Append(&config.module_search_paths, Py_DecodeLocale("/opt/basis/cache/uv/python/cpython-3.12.10-linux-aarch64-gnu/lib/python3.12", NULL));
  PyWideStringList_Append(&config.module_search_paths, Py_DecodeLocale("/opt/basis/cache/uv/python/cpython-3.12.10-linux-aarch64-gnu/lib/python3.1/lib-dynload", NULL));

 PyWideStringList_Append(&config.module_search_paths, Py_DecodeLocale("/opt/basis/.venv/lib/python3.12/site-packages", NULL));
 config.module_search_paths_set = 1;  // <== prevents overwrite

//Py_SetPythonHome(Py_DecodeLocale("/opt/basis/.venv", nullptr));

std::cout << "initializing" << std::endl;
  Py_InitializeFromConfig(&config);
  std::cout << "blah blash" << std::endl;


  PyRun_SimpleStringFlags("print('Running on main thread')", NULL);

  py_saved_thread_state = PyEval_SaveThread();

  std::cout << "saved!" << std::endl;
  std::thread t = std::thread([this]() {
    PyGILState_STATE g = PyGILState_Ensure();


    PyRun_SimpleString("print('Kicking off code on a thread!')");

    PyRun_SimpleString(R"(
import sys
print(sys.path)
import basis.unit.pybind_test
import basis.unit.pybind_test.sub2
print(basis.unit.pybind_test.__file__)
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
