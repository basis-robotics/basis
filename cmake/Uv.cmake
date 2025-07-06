include_guard(GLOBAL)

find_program(UV uv REQUIRED)

#warning: `VIRTUAL_ENV=.venv` does not match the project environment path `/basis/.venv` and will be ignored; use `--active` to target the active environment instead
# TODO: more newlines

function(uv_initialize)
    # TODO: 
    #   WORKSPACE_VENV_DIRECTORY
    
    set(POSSIBLE_ARGS
        # Remove me
        "LOCK_FILE"
        # Python version to use for the environment
        "PYTHON_VERSION"
        # File pointing to workspace pyproject, that we can write to (defaults to CMAKE_SOURCE_DIRECTORY/pyproject.toml)
        "MANAGED_PYPROJECT_FILE"
        # File pointing to workspace pyproject, that we will not touch (defaults to unset)
        "UNMANAGED_PYPROJECT_FILE"
        # Name of the generated workspace package
        "WORKSPACE_PACKAGE_NAME"

        "WORKSPACE_ENVIRONMENT"
        # venv directory to install into (if empty, won't have an install step)
        "INSTALLATION_VENV"
        # Cache directory to use for venv
        "INSTALLATION_VENV_CACHE"

        
    )
    # TODO: can we use "" instead of arg, and have it work magically with globals as well?
    # do we want that?
    cmake_parse_arguments(PARSE_ARGV 0 UV
        "" "${POSSIBLE_ARGS}" ""
    )
    # TODO: this probably doesn't work
    if(DEFINED UV_INITIALIZED)
        return()
    endif()
    set(UV_INITIALIZED TRUE)

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
        # TODO: use REAL_PATH else
    endif()
    file(REAL_PATH ${UV_PYPROJECT_FILE} UV_PYPROJECT_FILE BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")


    # Ensure we always ignore whatever the shell's virtual env is and use the env defined in cmake
    # TODO: should this be at the top of the file?
    file(REAL_PATH ./.venv WORKSPACE_VENV_DIRECTORY BASE_DIRECTORY "${CMAKE_BINARY_DIRECTORY}")

    set(ENV{VIRTUAL_ENV} ${WORKSPACE_VENV_DIRECTORY})
    set(ENV{UV_PROJECT_ENVIRONMENT} ${WORKSPACE_VENV_DIRECTORY})

    add_custom_target(uv_sync ALL COMMAND
        ${CMAKE_COMMAND} -E env UV_PROJECT_ENVIRONMENT=${WORKSPACE_VENV_DIRECTORY} 
        ${UV} sync --no-progress --project ${UV_PYPROJECT_FILE})

    execute_process(
        COMMAND
            # create the venv - would normally be done by uv sync but we also want to install the python version
            ${UV} venv --python ${UV_PYTHON_VERSION} --allow-existing
        WORKING_DIRECTORY
            ${CMAKE_BINARY_DIR}
        COMMAND_ERROR_IS_FATAL ANY)

    if(NOT DEFINED UV_PROJECT_VERSION)
        set(UV_PROJECT_VERSION 0.0.0)
    endif()

    # Allow the lock file to live in the repo rather than in build/
    # if(DEFINED UV_LOCK_FILE)
    #     # ${CMAKE_CURRENT_SOURCE_DIR}/${UV_LOCK_FILE}
    #     # Convert to absolute path
    #     cmake_path(ABSOLUTE_PATH UV_LOCK_FILE)
    #     # Then relative to binary dir
    #     cmake_path(RELATIVE_PATH UV_LOCK_FILE BASE_DIRECTORY ${CMAKE_BINARY_DIR})
    #     #
    #     message("new uv path ${UV_LOCK_FILE}")
    #     set(UV_ACTUAL_LOCK_FILE ${CMAKE_BINARY_DIR}/uv.lock)

    #     # Note: we could do a dance and check the symlink location if it exists, or just overwrite whatever is there
    #     execute_process(COMMAND ln -s -f ${UV_LOCK_FILE} ${UV_ACTUAL_LOCK_FILE} COMMAND_ERROR_IS_FATAL ANY)
    # endif()

    # Define and initialize global property once
    define_property(GLOBAL PROPERTY UV_PYTHON_TOMLS
        BRIEF_DOCS "Collected pyproject.toml files"
        FULL_DOCS "Accumulated pyproject.toml files from all subprojects")
    set_property(GLOBAL PROPERTY UV_PYTHON_TOMLS "")
    
    define_property(GLOBAL PROPERTY UV_DEV_DEPENDENCIES
        BRIEF_DOCS "uv dev dependencies"
        FULL_DOCS "--dev dependencies, typically needed by helpers called via CMake")
    set_property(GLOBAL PROPERTY UV_DEV_DEPENDENCIES "")

    if(NOT DEFINED UV_PYPROJECT_FILE)
        set(UV_PYPROJECT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml")
    endif()

    cmake_language(EVAL CODE "
    cmake_language(DEFER DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} 
        CALL _uv_internal_finish ${UV_PYTHON_VERSION} ${UV_WORKSPACE_PACKAGE_NAME} ${UV_PROJECT_VERSION} ${UV_PYPROJECT_FILE} ${UV_USING_MANAGED_PYPROJECT})
    ")

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

# Define the function to add sub projects
function(uv_add_pyproject PROJECT)
    set_property(GLOBAL APPEND PROPERTY UV_PYTHON_TOMLS "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT}")
endfunction()

function(uv_add_dev_dependency DEP)
    set_property(GLOBAL APPEND PROPERTY UV_DEV_DEPENDENCIES ${DEP})
endfunction()

# Define the finalization logic
function(_uv_internal_finish UV_PYTHON_VERSION UV_WORKSPACE_PACKAGE_NAME UV_PROJECT_VERSION UV_PYPROJECT_FILE UV_USING_MANAGED_PYPROJECT)    
    if(UV_USING_MANAGED_PYPROJECT)
        file(WRITE ${UV_PYPROJECT_FILE} "")
        get_property(UV_PYTHON_TOMLS GLOBAL PROPERTY UV_PYTHON_TOMLS)
        get_property(UV_DEV_DEPENDENCIES GLOBAL PROPERTY UV_DEV_DEPENDENCIES)

        set(UV_WORKSPACE_PACKAGE_NAMES "")

        foreach(PATH IN LISTS UV_PYTHON_TOMLS)
            get_filename_component(PROJECT_DIR ${PATH} DIRECTORY)

            message("${UV} --directory \"${PROJECT_DIR}\" version")
            execute_process(COMMAND ${UV} --directory "${PROJECT_DIR}" version OUTPUT_VARIABLE UV_RESULT COMMAND_ERROR_IS_FATAL ANY)
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

        make_directory("${PYPROJECT_DIR}/src/${UV_WORKSPACE_PACKAGE_NAME}")
        # TODO: funnily enough, we could probably use jinja to generate this
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

        file(APPEND ${UV_PYPROJECT_FILE} "[build-system]\n")
        file(APPEND ${UV_PYPROJECT_FILE} "requires = [\n")
        file(APPEND ${UV_PYPROJECT_FILE} "  \"uv_build>=0.7.19,<0.8.0\",\n")
        file(APPEND ${UV_PYPROJECT_FILE} "]\n")

        file(APPEND ${UV_PYPROJECT_FILE} "build-backend = \"uv_build\"\n")
        file(APPEND ${UV_PYPROJECT_FILE} "\n")

        file(APPEND ${UV_PYPROJECT_FILE} "[tool.uv.build-backend]\n")
        file(APPEND ${UV_PYPROJECT_FILE} "namespace = true\n")

        file(APPEND ${UV_PYPROJECT_FILE} "[tool.uv.sources]\n")
        foreach(NAME IN LISTS UV_WORKSPACE_PACKAGE_NAMES)
            file(APPEND ${UV_PYPROJECT_FILE} "${NAME} = { workspace = true }\n")
        endforeach()

        file(APPEND ${UV_PYPROJECT_FILE} "[tool.uv.workspace]\n")
        file(APPEND ${UV_PYPROJECT_FILE} "members = [\n")
        foreach(PATH IN LISTS UV_PYTHON_TOMLS)
            get_filename_component(PROJECT_DIR ${PATH} DIRECTORY)

            message(STATUS "  ${PROJECT_DIR}")
            file(APPEND ${UV_PYPROJECT_FILE} "  \"${PROJECT_DIR}\",\n")
        endforeach()
        file(APPEND ${UV_PYPROJECT_FILE} "]")

        foreach(DEP IN LISTS UV_DEV_DEPENDENCIES)
            message("Adding python dependency ${DEP} ${UV_PYPROJECT_FILE}")
            execute_process(COMMAND ${UV} add --project ${UV_PYPROJECT_FILE} --dev ${DEP} COMMAND_ERROR_IS_FATAL ANY)
        endforeach()

    endif()
    

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

    # install(CODE "
    #     set(UV \"${UV}\")
    #     set(UV_WORKSPACE_PACKAGE_NAME \"${UV_WORKSPACE_PACKAGE_NAME}\")
    #     set(UV \"${UV}\")
    #     set(UV \"${UV}\")
    #     include(\"${CMAKE_CURRENT_SOURCE_DIR}/cmake/Uv_Install.cmake\")
    # ")

    # uv export --no-emit-workspace --no-hashes -o requirements-frozen.txt
    # uv build
    # make other env
    # https://github.com/astral-sh/uv/issues/8729
    # pip install -c requirements-frozen.txt dist/foo-0.1.0-py3-none-any.whl
endfunction()


# add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/pyproject.toml
#     COMMAND "echo 'hi'")
# add_custom_target(uv_finalize ALL DEPENDS ${CMAKE_BINARY_DIR}/pyproject.toml)
# TODO: should do this with a target instead




# install step
# 1. lock
# 2. uv export
# 3. create venv at DEST
# 4. uv pip install install --compile-bytecode --target DEST -r requirements.txt

