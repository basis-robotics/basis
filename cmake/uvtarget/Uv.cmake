# Copyright (c) 2025 Kyle Franz / Basis Robotics
# This work is licensed under the terms of the MIT license.  

# uvtarget is a helpful utility to manage Python in CMake, powered by uv
# For more details, see the README

# Uv.cmake - main file to include, adds several helpers for managing python

include_guard(GLOBAL)

# Obviously we can't use this if we don't have uv installed
find_program(UV uv REQUIRED)

# uv_initialize, main entrypoint
# Call this once per CMake source tree to setup uvtarget
# If called multiple times, the first call "wins" - so take care if you're including
# something else that might also use uvtarget. Generally, you can only have one
# venv/python version/workspace, and the implementation reflects that.
function(uv_initialize)
    get_property(UVTARGET_INITIALIZED
             GLOBAL PROPERTY UVTARGET_INITIALIZED
             DEFINED)
    if(UVTARGET_INITIALIZED)
        # Ignore multiple calls to uv_initialize
        return()
    endif()

    define_property(GLOBAL PROPERTY UVTARGET_INITIALIZED)
        
    set(POSSIBLE_SINGLE_ARGS
        # Python version to use for the environment
        "PYTHON_VERSION"
        # File pointing to workspace pyproject, that we can write to (defaults to CMAKE_SOURCE_DIRECTORY/pyproject.toml)
        "MANAGED_PYPROJECT_FILE"
        # File pointing to workspace pyproject, that we will not touch (defaults to unset)
        "UNMANAGED_PYPROJECT_FILE"
        # Name of the generated workspace package
        "WORKSPACE_PACKAGE_NAME"
        # venv directory to install editable copy of package into
        "WORKSPACE_VENV"        
        # venv directory to install into (if empty, won't have an install step)
        "INSTALLATION_VENV"
        # Cache directory to use for venv
        "INSTALLATION_VENV_CACHE"
    )

    # TODO:
    # Allow passing --extra foobar OR --inexact
    # ADDITIONAL_SYNC_ARGS

    cmake_parse_arguments(PARSE_ARGV 0 UV
        "" "${POSSIBLE_SINGLE_ARGS}" ""
    )

    if(UV_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown argument(s) ${UV_UNPARSED_ARGUMENTS}")
    endif()

    # Set default for project version
    if(NOT DEFINED UV_PROJECT_VERSION)
        set(UV_PROJECT_VERSION 0.0.0)
    endif()

    # Figure out where we are storing the project
    if(DEFINED UV_UNMANAGED_PYPROJECT_FILE)
        if(DEFINED UV_MANAGED_PYPROJECT_FILE)
            message(FATAL_ERROR "Only one of MANAGED_PYPROJECT_FILE and UV_UNMANAGED_PYPROJECT_FILE must be set")
        endif()
        set(UV_PYPROJECT_FILE ${UV_UNMANAGED_PYPROJECT_FILE})
        message("Using unmanaged pyproject at ${UV_PYPROJECT_FILE}")
        set(UV_USING_MANAGED_PYPROJECT OFF)
    else()
        if(NOT DEFINED UV_MANAGED_PYPROJECT_FILE)
            set(UV_MANAGED_PYPROJECT_FILE "${CMAKE_CURRENT_SOURCE_DIRECTORY}/pyproject.toml")
        endif()
        set(UV_PYPROJECT_FILE ${UV_MANAGED_PYPROJECT_FILE})
        message("Using managed pyproject at ${UV_PYPROJECT_FILE}")
        set(UV_USING_MANAGED_PYPROJECT ON)
    endif()
    
    # Convert relative paths to absolute
    file(REAL_PATH ${UV_PYPROJECT_FILE} UV_PYPROJECT_FILE BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    if(NOT DEFINED UV_WORKSPACE_VENV)
        set(UV_WORKSPACE_VENV ./.venv)
    endif()

    # Ensure we always ignore whatever the shell's virtual env is and use the env defined in cmake
    file(REAL_PATH ${UV_WORKSPACE_VENV} UV_WORKSPACE_VENV BASE_DIRECTORY "${CMAKE_BINARY_DIRECTORY}")

    # Ensure that other invocations of uv (especially those outside this file) do the right thing
    set(ENV{VIRTUAL_ENV} ${UV_WORKSPACE_VENV})
    set(ENV{UV_PROJECT_ENVIRONMENT} ${UV_WORKSPACE_VENV})

    # create the venv - would normally be done by uv sync but 
    # we want to pin the python version ahead of time
    execute_process(
        COMMAND
            ${UV} venv --python ${UV_PYTHON_VERSION} --allow-existing
        WORKING_DIRECTORY
            ${CMAKE_BINARY_DIR}
        COMMAND_ERROR_IS_FATAL ANY)

    # Add target to sync all pyproject depends to our dev venv
    add_custom_target(uv_sync ALL COMMAND
        ${CMAKE_COMMAND} -E env 
            # Ensure we target the right environment
            UV_PROJECT_ENVIRONMENT=${UV_WORKSPACE_VENV_DIRECTORY}
            # Silence warning if the user's terminal is an a venv
            VIRTUAL_ENV=${UV_WORKSPACE_VENV_DIRECTORY}
        ${UV} sync --no-progress --project ${UV_PYPROJECT_FILE})

    # Set up globals to store collected dependencies
    define_property(GLOBAL PROPERTY UV_PYTHON_TOMLS
        BRIEF_DOCS "Collected pyproject.toml files"
        FULL_DOCS "Accumulated pyproject.toml files from all subprojects")
    set_property(GLOBAL PROPERTY UV_PYTHON_TOMLS "")

    define_property(GLOBAL PROPERTY UV_DEV_DEPENDENCIES
        BRIEF_DOCS "uv dev dependencies"
        FULL_DOCS "--dev dependencies, typically needed by helpers called via CMake")
    set_property(GLOBAL PROPERTY UV_DEV_DEPENDENCIES "")

    # Add a hook to write out the pyproject file
    # In the future, we can dynamically add to the pyproject with `uv add`
    # but for now there's no way to do so for workspace members
    # see https://github.com/astral-sh/uv/issues/14464

    # This is the "official" way to pass variables to a DEFER call
    # Variables appear to be evaluated when the function is called
    # rather than when the function is scheduled - so we have to bundle the whole
    # thing up as a big string.
    # https://cmake.org/cmake/help/latest/command/cmake_language.html#deferred-call-examples
    cmake_language(EVAL CODE "
    cmake_language(DEFER DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        CALL _uv_internal_finish
            ${UV_PYTHON_VERSION}
            ${UV_WORKSPACE_PACKAGE_NAME}
            ${UV_PROJECT_VERSION}
            ${UV_PYPROJECT_FILE}
            ${UV_USING_MANAGED_PYPROJECT})
    ")

    # Add the install step, if we specified an install location for the venv
    # Another bit of CMake oddness - there's once again no great way to pass arguments into
    # an install step, so once again we reach for a big string.
    # TODO: we could consider making some sort of target to just build wheels
    if(DEFINED UV_INSTALLATION_VENV)
        install(CODE "
            set(UV \"${UV}\")
            set(UV_PROJECT_VERSION \"${UV_PROJECT_VERSION}\")
            set(UV_PYTHON_VERSION \"${UV_PYTHON_VERSION}\")
            set(UV_INSTALLATION_VENV \"${UV_INSTALLATION_VENV}\")
            set(UV_INSTALLATION_VENV_CACHE \"${UV_INSTALLATION_VENV_CACHE}\")
            set(UV_PYPROJECT_FILE \"${UV_PYPROJECT_FILE}\")
            include(\"${CMAKE_CURRENT_SOURCE_DIR}/cmake/UvInstall.cmake\")
        ")
    endif()
endfunction()

# Add a pyrproject to a managed workspace
# TODO: warn if using unmanaged pyproject
function(uv_add_pyproject PROJECT)
    set_property(GLOBAL APPEND PROPERTY UV_PYTHON_TOMLS "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT}")
endfunction()

# Add a dev dependency to the workspace
# This works in a managed workspace, as long as you don't delete it after
function(uv_add_dev_dependency DEP)
    set_property(GLOBAL APPEND PROPERTY UV_DEV_DEPENDENCIES ${DEP})
endfunction()

# Deferred function to write to pyproject.toml
function(_uv_internal_finish
            UV_PYTHON_VERSION
            UV_WORKSPACE_PACKAGE_NAME
            UV_PROJECT_VERSION
            UV_PYPROJECT_FILE
            UV_USING_MANAGED_PYPROJECT)
    # If we're unmanaged, don't stomp all over the user's work
    if(UV_USING_MANAGED_PYPROJECT)
        get_property(UV_PYTHON_TOMLS GLOBAL PROPERTY UV_PYTHON_TOMLS)

        # Extract package names from pyproject.tomls
        set(UV_WORKSPACE_PACKAGE_NAMES "")
        foreach(PATH IN LISTS UV_PYTHON_TOMLS)
            get_filename_component(PROJECT_DIR ${PATH} DIRECTORY)
            execute_process(COMMAND ${UV} --directory "${PROJECT_DIR}" version OUTPUT_VARIABLE UV_RESULT COMMAND_ERROR_IS_FATAL ANY)
            # outputs in the form `basis-cmake 0.0.0`
            string(REGEX MATCH "^[^ ]*" THIS_WORKSPACE_PACKAGE_NAME "${UV_RESULT}")
            list(APPEND UV_WORKSPACE_PACKAGE_NAMES "${THIS_WORKSPACE_PACKAGE_NAME}")
        endforeach()

        # TODO: there's no way to add workspace members programatically
        # https://github.com/astral-sh/uv/issues/14464
        # execute_process(COMMAND ${UV} init
        #                     --name ${UV_WORKSPACE_PACKAGE_NAME}
        #                     --bare
        #                     --no-readme
        #                     --no-description
        #                     --lib
        #                     --author-from none
        #                     --python ${UV_PYTHON_VERSION}
        #                     --build-backend uv
        #                 COMMAND_ERROR_IS_FATAL ANY)
        # execute_process(COMMAND ${UV} version ${UV_PROJECT_VERSION}
        #                 COMMAND_ERROR_IS_FATAL ANY)
        get_filename_component(PYPROJECT_DIR ${UV_PYPROJECT_FILE} DIRECTORY)

        # uv requires a src/<package name> directory, even if we are purely
        # a workspace/namespace package. It sucks that this might write into
        # the user's source directory, but not much to be done right now.
        make_directory("${PYPROJECT_DIR}/src/${UV_WORKSPACE_PACKAGE_NAME}")

        # funnily enough, we could probably use jinja to generate this
        
        # Start a new file, overwriting whatever was there
        file(WRITE ${UV_PYPROJECT_FILE} "")
        # Basic project info
        file(APPEND ${UV_PYPROJECT_FILE} "[project]\n")
        file(APPEND ${UV_PYPROJECT_FILE} "name = \"${UV_WORKSPACE_PACKAGE_NAME}\"\n")
        file(APPEND ${UV_PYPROJECT_FILE} "requires-python = \">=${UV_PYTHON_VERSION}\"\n")
        file(APPEND ${UV_PYPROJECT_FILE} "version = \"${UV_PROJECT_VERSION}\"\n")
        file(APPEND ${UV_PYPROJECT_FILE} "dependencies = [\n")
        foreach(NAME IN LISTS UV_WORKSPACE_PACKAGE_NAMES)
            file(APPEND ${UV_PYPROJECT_FILE} "  \"${NAME}\",\n")
        endforeach()
        file(APPEND ${UV_PYPROJECT_FILE} "]\n")
        file(APPEND ${UV_PYPROJECT_FILE} "\n")

        # We're enforcing uv as the backend, we could support multiple backends in the future
        file(APPEND ${UV_PYPROJECT_FILE} "[build-system]\n")
        file(APPEND ${UV_PYPROJECT_FILE} "requires = [\n")
        file(APPEND ${UV_PYPROJECT_FILE} "  \"uv_build>=0.7.19,<0.8.0\",\n")
        file(APPEND ${UV_PYPROJECT_FILE} "]\n")
        file(APPEND ${UV_PYPROJECT_FILE} "build-backend = \"uv_build\"\n")
        file(APPEND ${UV_PYPROJECT_FILE} "\n")

        # Be a little more flexible about project structure
        file(APPEND ${UV_PYPROJECT_FILE} "[tool.uv.build-backend]\n")
        file(APPEND ${UV_PYPROJECT_FILE} "namespace = true\n")
        file(APPEND ${UV_PYPROJECT_FILE} "\n")

        # Specify where packages come from
        file(APPEND ${UV_PYPROJECT_FILE} "[tool.uv.sources]\n")
        foreach(NAME IN LISTS UV_WORKSPACE_PACKAGE_NAMES)
            file(APPEND ${UV_PYPROJECT_FILE} "${NAME} = { workspace = true }\n")
        endforeach()
        file(APPEND ${UV_PYPROJECT_FILE} "\n")
        file(APPEND ${UV_PYPROJECT_FILE} "[tool.uv.workspace]\n")
        file(APPEND ${UV_PYPROJECT_FILE} "members = [\n")
        foreach(PATH IN LISTS UV_PYTHON_TOMLS)
            get_filename_component(PROJECT_DIR ${PATH} DIRECTORY)
            file(APPEND ${UV_PYPROJECT_FILE} "  \"${PROJECT_DIR}\",\n")
        endforeach()
        file(APPEND ${UV_PYPROJECT_FILE} "]")
    endif()

    # This step is non-destructive so it gets to happen to all projects
    # TODO: we might want a way of turning it off, if including some library that adds
    # unwanted dev deps 
    get_property(UV_DEV_DEPENDENCIES GLOBAL PROPERTY UV_DEV_DEPENDENCIES)
    foreach(DEP IN LISTS UV_DEV_DEPENDENCIES)
        message("Adding python dependency ${DEP} ${UV_PYPROJECT_FILE}")
        execute_process(COMMAND ${UV} add --project ${UV_PYPROJECT_FILE} --dev ${DEP} COMMAND_ERROR_IS_FATAL ANY)
    endforeach()

    # We could depend on all pyproject tomls this way, but it wouldn't catch
    # references of references. Instead, just invoke uv every time
    # add_custom_target(uv_sync ALL
    #     DEPENDS ${CMAKE_BINARY_DIR}/.venv/some_marker)
    # add_custom_command(
    #     OUTPUT ${CMAKE_BINARY_DIR}/.venv/some_marker
    #     DEPENDS ${UV_PYTHON_TOMLS}
    #     COMMAND ${UV} sync --no-progress
    #     COMMAND touch ${CMAKE_BINARY_DIR}/.venv/some_marker)
    # execute_process(COMMAND ${INSTALL_EDITABLE_COMMAND} COMMAND_ERROR_IS_FATAL ANY)
endfunction()
