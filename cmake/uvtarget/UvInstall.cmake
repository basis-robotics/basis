# Copyright (c) 2025 Kyle Franz / Basis Robotics
# This work is licensed under the terms of the MIT license.  

# uvtarget is a helpful utility to manage Python in CMake, powered by uv
# For more details, see the README

# UvInstall.cmake - do not include() this file directly,
# it's meant to be invoked by CMake's install step

# Ensure that all required variables are passed in
function(require VAR)
    if(NOT DEFINED ${VAR})
        message(FATAL_ERROR "${VAR} required")
    endif()
endfunction()

# This needs passed in due to `sudo` possibly not having it in $PATH
require(UV)
require(UV_PROJECT_VERSION)
require(UV_PYTHON_VERSION)
require(UV_INSTALLATION_VENV)
require(UV_INSTALLATION_VENV_CACHE)
require(UV_PYPROJECT_FILE)

# 1. Check to make sure we aren't in some weird state.
#    The previous build should have run `uv sync` and updated the lockfile.
#    Not strictly needed, but can't hurt.
execute_process(
    COMMAND
        ${UV} lock --check --project ${UV_PYPROJECT_FILE}
    COMMAND_ERROR_IS_FATAL ANY)

# 2. Export all dependencies we have that aren't in the workspace
execute_process(
    COMMAND
        ${UV} export
            # No need to update uv.lock
            --frozen
            # Don't include this workspace
            --no-emit-workspace
            # we may want to take these requirements and install them on another platform
            --no-hashes
            --output-file dist/requirements.txt
            --project ${UV_PYPROJECT_FILE}
            # TODO: --no-index? --no-editable?
    COMMAND_ERROR_IS_FATAL ANY)

# 3. Build wheels of all packages in the workspace
execute_process(
    COMMAND
        ${UV} build --wheel --all-packages  --project ${UV_PYPROJECT_FILE} --out-dir=dist
    COMMAND_ERROR_IS_FATAL ANY)

if(UV_INSTALLATION_VENV_CACHE)
    if(LINUX)
        # Setting --cache-dir has no effect on the used python interpreter
        # making it so that `sudo make install` gives an unexecutable binary
        # Workaround this issue by using an env variable instead
        set(ENV{XDG_DATA_HOME} ${UV_INSTALLATION_VENV_CACHE})
    endif()
endif()

# 4. Create a new venv with the correct python version
execute_process(
    COMMAND
        ${UV} venv ${UV_INSTALLATION_VENV} --python ${UV_PYTHON_VERSION}
    COMMAND_ERROR_IS_FATAL ANY)
set(ENV{VIRTUAL_ENV} ${UV_INSTALLATION_VENV})

# 5. Gather all built wheels from previous step
file(GLOB WHEELS dist/*.whl)

# 6. Install all wheels, pinning dependencies to the versions in the lockfile
# TODO: we could also support plain ole' pip install,
# or "no install" ie just output to dist/
execute_process(
    COMMAND
        ${UV} pip install ${WHEELS} -c dist/requirements.txt
    COMMAND_ERROR_IS_FATAL ANY)
