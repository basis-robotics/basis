/*

  This is the starting point for your Unit. Edit this directly and implement the
  missing methods!

*/

#include <memory>
#include <unit/pybind_test/unit_base.h>

#include "python_shim.h"

#include <Python.h>
#include <python3.8/frameobject.h>


#include <pybind11/embed.h>

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
  std::unique_ptr<pybind11::scoped_interpreter> py;
  bool pub = false;
};