/*

  This is the starting point for your Unit. Edit this directly and implement the
  missing methods!

*/

#include <memory>
#include <unit/pybind_test/unit_base.h>
// #define _DEBUG
// #define Py_DEBUG 1
#include <Python.h>
#define Py_BUILD_CORE 1
#include <internal/pycore_pystate.h>

#define X_PYTHON_API \
    X_PY(Py_InitializeEx) \
    X_PY(Py_Finalize) \
    X_PY(Py_IsInitialized) \
    X_PY(PyRun_SimpleStringFlags) \
    X_PY(PyEval_SaveThread) \
    X_PY(PyEval_RestoreThread) \
    X_PY(PyGILState_Ensure) \
    X_PY(PyGILState_Release) \
    X_PY(PyEval_InitThreads) \
 X_PY(PyEval_AcquireLock) \
 X_PY(PyThreadState_Swap) \
 X_PY(PyThreadState_New) \
 X_PY(PyEval_ReleaseLock) \
 X_PY(PyThreadState_Clear) \
 X_PY(PyThreadState_Delete) \
 X_PY(PyEval_AcquireThread) \
 X_PY(PyInterpreterState_Head) \
 X_PY(PyEval_ReleaseThread) \
 X_PY(PyThreadState_Get) \
 X_PY(PyGILState_GetThisThreadState)\
X_PY(_PyInterpreterState_Get) \
X_PY(_PyRuntime) \
X_PY(PyThread_tss_get) \
X_PY(PyThread_tss_set) \
X_PY(PyThread_tss_delete) \
X_PY(PyThread_tss_create) 
class pybind_test : public unit::pybind_test::Base {
public:
  pybind_test(const Args &args,
         const std::optional<std::string_view> &name_override = {});
  virtual unit::pybind_test::InprocTest::Output
  InprocTest(const unit::pybind_test::InprocTest::Input &input) override;
  virtual unit::pybind_test::InprocTestTrigger::Output
  InprocTestTrigger(const unit::pybind_test::InprocTestTrigger::Input &input) override;
 
  virtual unit::pybind_test::StereoMatch::Output
  StereoMatch(const unit::pybind_test::StereoMatch::Input &input) override;
  virtual unit::pybind_test::TimeTest::Output
  TimeTest(const unit::pybind_test::TimeTest::Input &input) override;
  virtual unit::pybind_test::ApproxTest::Output
  ApproxTest(const unit::pybind_test::ApproxTest::Input &input) override;


private:
  #define X_PY(f) decltype(::f)* f = nullptr;
  X_PYTHON_API
  #undef X_PY

  bool pub = false;

  PyThreadState *py_saved_thread_state;
};