#include "pybind_test.h"
#include "basis/core/logging/macros.h"

#include <memory>
//#include <pybind11/pybind11.h>
#include <dlfcn.h>
#include <type_traits>
#include "python_shim.h"
using namespace unit::pybind_test;




#define X_PY(o) decltype(o) o;
X_PYTHON_STATICS
#undef X_PY

PyAPI_FUNC(PyObject *) PyObject_CallFunctionObjArgs(PyObject *callable,
                                                    ...) {
                                                     return nullptr; 
                                                    }

PyAPI_FUNC(PyObject *) PyErr_Format(
    PyObject *exception,
    const char *format,   /* ASCII-encoded string  */
    ...
    ) {
      return nullptr;
    }


template<typename R, typename... Args>
struct FunctionTraitsBase
{
   using RetType = R;
   using ArgTypes = std::tuple<Args...>;
   static constexpr std::size_t ArgCount = sizeof...(Args);
   template<std::size_t N>
   using NthArg = std::tuple_element_t<N, ArgTypes>;
};

template<typename F> struct FunctionTraits;

template<typename R, typename... Args>
struct FunctionTraits<R(*)(Args...)>
    : FunctionTraitsBase<R, Args...>
{
  using Pointer = R(*)(Args...);
  constexpr static bool IsNoexcept = false;
};

template<typename R, typename... Args>
struct FunctionTraits<R(*)(Args...) noexcept>
    : FunctionTraitsBase<R, Args...>
{
  using Pointer = R(*)(Args...);
  constexpr static bool IsNoexcept = true;
};


// Main shimFunction template, handles function pointers with args deduction
template <auto Func, typename... Args>
auto shimFunction(Args&&...) {
    using FuncType = decltype(Func);  // Get the function pointer type
    std::cout << "shimFunction" << std::endl;
    // Deduce the return type from the function signature
    using R = typename std::invoke_result<FuncType, Args...>;

    if constexpr (std::is_void_v<R>) {
        // For void return type, we return nothing
        return;
    } else {
        // Otherwise, return a default-constructed value of the correct return type
        return R{};
    }
}


// Forwarder struct that unpacks the tuple's types
template <auto Func, typename Tuple, std::size_t... I>
struct ForwarderImpl {
    static constexpr auto f = &shimFunction<Func, std::tuple_element_t<I, Tuple>...>;
};

// Forwarder alias to hide index sequence
template <auto Func, typename Tuple>
using Forwarder = ForwarderImpl<Func, Tuple, std::make_index_sequence<std::tuple_size<Tuple>::value>::value>;

#if 0
#define WRAP_FUNCTION(func_name) \
  extern "C" auto shim_ ## func_name = ForwarderImpl<&:: func_name, FunctionTraits<decltype(:: func_name)*>::ArgTypes>::f; \
  \
  asm ( \
    #func_name  ":\n" \
    "    b shim_" #func_name " \n" \
  );
#endif
#define WRAP_FUNCTION(func_name) \
  extern "C" auto shim_ ## func_name = ForwarderImpl<&:: func_name, FunctionTraits<decltype(:: func_name)*>::ArgTypes>::f; \
  asm ( \
    #func_name ":\n" \
    "    ldr x16, =shim_" #func_name "\n"  /* Load address of shim_##func_name */ \
    "    ldr x16, [x16]\n"                 /* Dereference to get function pointer */ \
    "    br x16\n"                         /* Jump to the function pointer */ \
  );

namespace shim {

#define X_PY(f) WRAP_FUNCTION(f)
X_PYTHON_API
}

static int foobar = 0;

pybind_test::pybind_test(const Args &args,
         const std::optional<std::string_view> &name_override)
      : unit::pybind_test::Base(args, name_override) {

    void* pyhandle = dlmopen(LM_ID_NEWLM, "libpython3.8.so", RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
#if 0
    #define SHIM_PY(f) auto f = reinterpret_cast<decltype(::f)*>(dlsym(pyhandle, #f));     if (!f) { \
      std::cerr << #f << " dlerror: " << dlerror() << std::endl; \
    } while(false)

    // ie
    // auto Py_IsInitialized = reinterpret_cast<decltype(::Py_IsInitialized)*>(dlsym(pyhandle, "Py_IsInitialized"));
    SHIM_PY(Py_InitializeEx);
    SHIM_PY(Py_Finalize);
    SHIM_PY(Py_IsInitialized);
    SHIM_PY(PyRun_SimpleStringFlags);

    Py_InitializeEx(0);
    assert(Py_IsInitialized());
    PyRun_SimpleStringFlags("print('Hello from Python!')", NULL);

    //Py_Finalize();
  #endif


    py = std::make_unique<pybind11::scoped_interpreter>();

//    BASIS_LOG_INFO("About to call python interpreter function");
    pybind11::print("Hello, World!");

    // todo: check python interpeter for signals
}

InprocTest::Output pybind_test::InprocTest(const InprocTest::Input &input) {
  BASIS_LOG_INFO("Got an inproc trigger");
  //static_assert(false, "Implement me");
  return {};
}

InprocTestTrigger::Output pybind_test::InprocTestTrigger(const InprocTestTrigger::Input &input) {
  //static_assert(false, "Implement me");
  if(pub) { 
    return {std::make_shared<std::string>("test")};
  }
  return {};
}

StereoMatch::Output pybind_test::StereoMatch(const StereoMatch::Input &input) {
  //static_assert(false, "Implement me");
  return {};
}
unit::pybind_test::TimeTest::Output pybind_test::TimeTest(const unit::pybind_test::TimeTest::Input &input) {
  //static_assert(false, "Implement me");
  return {};
}
ApproxTest::Output pybind_test::ApproxTest(const ApproxTest::Input &input) {
  //static_assert(false, "Implement me");
  return {};
}
