#include <basis/core/time.h>
#include "basis_example.pb.h"

struct TimeTestInproc {
    basis::core::MonotonicTime time;

    mutable bool ran_conversion_function = false;

    std::shared_ptr<TimeTest> ToMessage() const {
        ran_conversion_function = true;
        return std::make_shared<TimeTest>();
    }
};