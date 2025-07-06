function(require VAR)
    if(NOT DEFINED ${VAR})
        message(FATAL_ERROR "${VAR} required")
    endif()
endfunction()
# This needs passed in due to `sudo` possibly not having it in $PATH
require(UV)
require(UV_PROJECT_NAME)
require(UV_PROJECT_VERSION)
require(UV_PYTHON_VERSION)
require(UV_INSTALLATION_VENV)
require(UV_INSTALLATION_VENV_CACHE)
execute_process(
    COMMAND
        ${UV} lock --check
    COMMAND_ERROR_IS_FATAL ANY)

execute_process(
    COMMAND
        ${UV} export --frozen --no-emit-workspace --no-hashes -o dist/requirements.txt
    COMMAND_ERROR_IS_FATAL ANY)
    
execute_process(
    COMMAND
        ${UV} build --wheel --all-packages
    COMMAND_ERROR_IS_FATAL ANY)

if(UV_INSTALLATION_VENV_CACHE)
    if(LINUX)
        # Setting --cache-dir has no effect on the used python interpreter
        # making it so that `sudo make install` gives an unexecutable binary
        # Workaround this issue 
        set(ENV{XDG_DATA_HOME} ${UV_INSTALLATION_VENV_CACHE})
    endif()
endif()

# Create a new venv with the correct python version
execute_process(
    COMMAND
        ${UV} venv ${UV_INSTALLATION_VENV} --python ${UV_PYTHON_VERSION}
    COMMAND_ERROR_IS_FATAL ANY)
set(ENV{VIRTUAL_ENV} ${UV_INSTALLATION_VENV})

# TODO: glob dist/*.whl
file(GLOB WHEELS dist/*.whl)

execute_process(
    COMMAND
        ${UV} pip install ${WHEELS} -c dist/requirements.txt
    COMMAND_ERROR_IS_FATAL ANY)
