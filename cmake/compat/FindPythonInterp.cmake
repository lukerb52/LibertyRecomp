# Compatibility for pinned dependencies still calling find_package(PythonInterp).
# CMake 3.27+ disables the built-in finder under CMP0148 NEW. Use modern
# interpreter discovery while exposing the variables expected by glslang.
set(_liberty_python_version_args)
if(PythonInterp_FIND_VERSION)
    list(APPEND _liberty_python_version_args "${PythonInterp_FIND_VERSION}")
    if(PythonInterp_FIND_VERSION_EXACT)
        list(APPEND _liberty_python_version_args EXACT)
    endif()
endif()
find_package(Python3 ${_liberty_python_version_args} QUIET COMPONENTS Interpreter)
unset(_liberty_python_version_args)

set(PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")
set(PYTHON_VERSION_STRING "${Python3_VERSION}")
set(PYTHON_VERSION_MAJOR "${Python3_VERSION_MAJOR}")
set(PYTHON_VERSION_MINOR "${Python3_VERSION_MINOR}")
set(PYTHON_VERSION_PATCH "${Python3_VERSION_PATCH}")
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PythonInterp
    REQUIRED_VARS Python3_Interpreter_FOUND PYTHON_EXECUTABLE
    VERSION_VAR PYTHON_VERSION_STRING)
set(PYTHONINTERP_FOUND "${PythonInterp_FOUND}")
