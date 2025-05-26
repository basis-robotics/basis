/*

  This is the starting point for your Unit. Edit this directly and implement the
  missing methods!

*/

#include <unit/pybind_test/unit_base.h>
#include <Python.h>

#define X_PYTHON_API \
    X_PY(Py_InitializeEx) \
    X_PY(Py_Finalize) \
    X_PY(Py_IsInitialized) \
    X_PY(PyRun_SimpleStringFlags) \
    X_PY(PyEval_SaveThread) \
    X_PY(PyEval_RestoreThread) \
    X_PY(PyGILState_Ensure) \
    X_PY(PyGILState_Release)
    
class pybind_test : public unit::pybind_test::Base {
public:
  pybind_test(const Args &args,
         const std::optional<std::string_view> &name_override = {});
  virtual unit::pybind_test::InprocTest::Output
  InprocTest(const unit::pybind_test::InprocTest::Input &input) override;
  virtual unit::pybind_test::InprocTestTrigger::Output
  InprocTestTrigger(const unit::pybind_test::InprocTestTrigger::Input &input) override;
 
private:
  #define X_PY(f) decltype(::f)* f = nullptr;
  X_PYTHON_API
  #undef X_PY

  bool pub = false;

  PyThreadState *py_saved_thread_state;
};