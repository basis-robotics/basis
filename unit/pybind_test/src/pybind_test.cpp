#include "pybind_test.h"
#include "basis/core/logging/macros.h"

#include <dlfcn.h>
#include <filesystem>
#include <stdexcept>

using namespace unit::pybind_test;

std::filesystem::path get_current_so_path() {
  Dl_info info;
  if (dladdr((const void *)&get_current_so_path, &info) && info.dli_fname) {
    return std::string(info.dli_fname);
  }
  return {};
}

pybind_test::pybind_test(const Args &args,
                         const std::optional<std::string_view> &name_override)
    : unit::pybind_test::Base(args, name_override), pub(args.pub) {

  const std::filesystem::path so_path = get_current_so_path();
  BASIS_LOG_INFO("Current so path: {}", so_path.string());

  std::filesystem::path search_dir = so_path;
  std::filesystem::path venv_dir;
  while (true) {
    search_dir = search_dir.parent_path();

    venv_dir = search_dir / ".venv";
    if (std::filesystem::is_directory(venv_dir)) {
      break;
    }
    BASIS_LOG_INFO("search dir path: {}", search_dir.string());
    if (search_dir == search_dir.root_path()) {
      // TODO: BASIS_FATAL?
      throw std::runtime_error("couldn't find .venv");
    }
  }
  BASIS_LOG_INFO("venv dir path: {}", venv_dir.string());
  const std::filesystem::path python_link = venv_dir / "bin" / "python";
  const std::filesystem::path python_canonical =
      std::filesystem::canonical(python_link);
  const std::string python_binary_name = python_canonical.filename();
  BASIS_LOG_INFO("python name {}", python_binary_name);
  std::cout << python_canonical << " " << python_canonical.parent_path()
            << "\n";

  const std::filesystem::path python_lib_dir =
      python_canonical.parent_path().parent_path() / "lib";
  const std::filesystem::path python_so_path =
      python_lib_dir / ("lib" + python_binary_name + ".so");

  BASIS_LOG_INFO("python lib path {}", python_so_path.string());
  dlopen(python_so_path.c_str(), RTLD_NOW | RTLD_GLOBAL);

  PyConfig config;
  PyConfig_InitPythonConfig(&config);

  // TODO: just use isolated=1?
  config.use_environment = 0; // Ignore PYTHONPATH etc.
  config.user_site_directory = 0;

  config.home = Py_DecodeLocale(python_canonical.parent_path().c_str(), NULL);
  config.prefix = Py_DecodeLocale(venv_dir.c_str(), NULL);
  config.exec_prefix = Py_DecodeLocale(venv_dir.c_str(), NULL);
  config.install_signal_handlers = 0;

  PyWideStringList_Append(
      &config.module_search_paths,
      Py_DecodeLocale((python_lib_dir / python_binary_name).c_str(), NULL));
  PyWideStringList_Append(
      &config.module_search_paths,
      Py_DecodeLocale(
          (python_lib_dir / python_binary_name / "lib-dynload").c_str(), NULL));

  PyWideStringList_Append(
      &config.module_search_paths,
      Py_DecodeLocale(
          (venv_dir / "lib" / python_binary_name / "site-packages").c_str(),
          NULL));
  config.module_search_paths_set = 1; // <== prevents overwrite

  Py_InitializeFromConfig(&config);

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
