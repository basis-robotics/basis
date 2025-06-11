/*

  This is the starting point for your Unit. Edit this directly and implement the
  missing methods!

*/

#include <Python.h>
#include <unit/pybind_test/unit_base.h>


class pybind_test : public unit::pybind_test::Base {
public:
  pybind_test(const Args &args,
              const std::optional<std::string_view> &name_override = {});
  virtual unit::pybind_test::InprocTest::Output
  InprocTest(const unit::pybind_test::InprocTest::Input &input) override;
  virtual unit::pybind_test::InprocTestTrigger::Output InprocTestTrigger(
      const unit::pybind_test::InprocTestTrigger::Input &input) override;

private:
  bool pub = false;

  PyThreadState *py_saved_thread_state;
};