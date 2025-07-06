include_guard(GLOBAL)

find_program(UV uv REQUIRED)

# TODO: allow passing in extras/groups to venv
# TODO: ability to split off a chunk of the project into a separate pyproject
# find_program(UVX uvx REQUIRED)
# TODO: allow adding build only deps

function(uv_initialize)
    # TODO: 
    #   CREATE_VENV
    #   VENV_DIRECTORY
    #   MANAGE_PYPROJECT 
    #   PYPROJECT_DIRECTORY
    #   
    
    # TODO: can we use "" instead of arg, and have it work magically with globals as well?
    # do we want that?
    cmake_parse_arguments(PARSE_ARGV 0 UV
        "" "LOCK_FILE;PYTHON_VERSION;PROJECT_NAME;INSTALLATION_VENV;INSTALLATION_VENV_CACHE" ""
    )
    # TODO: this probably doesn't work
    if(DEFINED UV_INITIALIZED)
        return()
    endif()
    set(UV_INITIALIZED TRUE)

    execute_process(
        COMMAND
            # create the venv - would normally be done by uv sync but we also want to install the python version
            ${UV} venv --python ${UV_PYTHON_VERSION} --allow-existing
        WORKING_DIRECTORY
            ${CMAKE_BINARY_DIR}
        COMMAND_ERROR_IS_FATAL ANY)

    # Ensure we always ignore whatever the shell's virtual env is and use the env defined in cmake
    # TODO: should this be at the top of the file?
    set(ENV{VIRTUAL_ENV} ${CMAKE_BINARY_DIR}/.venv)

    if(NOT DEFINED UV_PROJECT_VERSION)
        set(UV_PROJECT_VERSION 0.0.0)
    endif()

    # TODO: move pyproject
    # Allow the lock file to live in the repo rather than in build/
    if(DEFINED UV_LOCK_FILE)
        # ${CMAKE_CURRENT_SOURCE_DIR}/${UV_LOCK_FILE}
        # Convert to absolute path
        cmake_path(ABSOLUTE_PATH UV_LOCK_FILE)
        # Then relative to binary dir
        cmake_path(RELATIVE_PATH UV_LOCK_FILE BASE_DIRECTORY ${CMAKE_BINARY_DIR})
        #
        message("new uv path ${UV_LOCK_FILE}")
        set(UV_ACTUAL_LOCK_FILE ${CMAKE_BINARY_DIR}/uv.lock)

        # Note: we could do a dance and check the symlink location if it exists, or just overwrite whatever is there
        execute_process(COMMAND ln -s -f ${UV_LOCK_FILE} ${UV_ACTUAL_LOCK_FILE} COMMAND_ERROR_IS_FATAL ANY)
    endif()

    # Define and initialize global property once
    define_property(GLOBAL PROPERTY UV_PYTHON_TOMLS
        BRIEF_DOCS "Collected pyproject.toml files"
        FULL_DOCS "Accumulated pyproject.toml files from all subprojects")
    set_property(GLOBAL PROPERTY UV_PYTHON_TOMLS "")
    
    define_property(GLOBAL PROPERTY UV_DEV_DEPENDENCIES
        BRIEF_DOCS "uv dev dependencies"
        FULL_DOCS "--dev dependencies, typically needed by helpers called via CMake")
    set_property(GLOBAL PROPERTY UV_DEV_DEPENDENCIES "")


    cmake_language(EVAL CODE "
    cmake_language(DEFER DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} 
        CALL _uv_internal_finish ${UV_PYTHON_VERSION} ${UV_PROJECT_NAME} ${UV_PROJECT_VERSION})
    ")

    if(DEFINED UV_INSTALLATION_VENV)
    install(CODE "
        set(UV \"${UV}\")
        set(UV_PROJECT_NAME \"${UV_PROJECT_NAME}\")        
        set(UV_PROJECT_VERSION \"${UV_PROJECT_VERSION}\")
        set(UV_PYTHON_VERSION \"${UV_PYTHON_VERSION}\")
        set(UV_INSTALLATION_VENV \"${UV_INSTALLATION_VENV}\")
        set(UV_INSTALLATION_VENV_CACHE \"${UV_INSTALLATION_VENV_CACHE}\")
        include(\"${CMAKE_CURRENT_SOURCE_DIR}/cmake/Uv_Install.cmake\")
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
function(_uv_internal_finish UV_PYTHON_VERSION UV_PROJECT_NAME UV_PROJECT_VERSION)
    set(OUTPUT ${CMAKE_BINARY_DIR}/pyproject.toml)

    file(WRITE ${OUTPUT} "")
    get_property(UV_PYTHON_TOMLS GLOBAL PROPERTY UV_PYTHON_TOMLS)
    get_property(UV_DEV_DEPENDENCIES GLOBAL PROPERTY UV_DEV_DEPENDENCIES)

    set(UV_PROJECT_NAMES "")

    foreach(PATH IN LISTS UV_PYTHON_TOMLS)
        get_filename_component(PROJECT_DIR ${PATH} DIRECTORY)

        message("${UV} --directory \"${PROJECT_DIR}\" version")
        execute_process(COMMAND ${UV} --directory "${PROJECT_DIR}" version OUTPUT_VARIABLE UV_RESULT COMMAND_ERROR_IS_FATAL ANY)
        string(REGEX MATCH "^[^ ]*" THIS_PROJECT_NAME "${UV_RESULT}")
        list(APPEND UV_PROJECT_NAMES "${THIS_PROJECT_NAME}")
    endforeach()

    # TODO: there's no way to add workspace members programatically
    # https://github.com/astral-sh/uv/issues/14464
    # execute_process(COMMAND ${UV} init
    #                     --name ${UV_PROJECT_NAME}
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
    make_directory("src/${UV_PROJECT_NAME}")
    # TODO: funnily enough, we could probably use jinja to generate this
    file(APPEND ${OUTPUT} "[project]\n")
    file(APPEND ${OUTPUT} "name = \"${UV_PROJECT_NAME}\"\n")
    file(APPEND ${OUTPUT} "requires-python = \">=${UV_PYTHON_VERSION}\"\n")
    file(APPEND ${OUTPUT} "version = \"${UV_PROJECT_VERSION}\"\n")
    file(APPEND ${OUTPUT} "dependencies = [\n")
    foreach(NAME IN LISTS UV_PROJECT_NAMES)
        file(APPEND ${OUTPUT} "  \"${NAME}\",\n")
    endforeach()

    file(APPEND ${OUTPUT} "]\n")
    file(APPEND ${OUTPUT} "\n")

    file(APPEND ${OUTPUT} "[build-system]\n")
    file(APPEND ${OUTPUT} "requires = [\n")
    file(APPEND ${OUTPUT} "  \"uv_build>=0.7.19,<0.8.0\",\n")
    file(APPEND ${OUTPUT} "]\n")

    file(APPEND ${OUTPUT} "build-backend = \"uv_build\"\n")
    file(APPEND ${OUTPUT} "\n")

    file(APPEND ${OUTPUT} "[tool.uv.build-backend]\n")
    file(APPEND ${OUTPUT} "namespace = true\n")

    file(APPEND ${OUTPUT} "[tool.uv.sources]\n")
    foreach(NAME IN LISTS UV_PROJECT_NAMES)
        file(APPEND ${OUTPUT} "${NAME} = { workspace = true }\n")
    endforeach()

    file(APPEND ${OUTPUT} "[tool.uv.workspace]\n")
    file(APPEND ${OUTPUT} "members = [\n")
    foreach(PATH IN LISTS UV_PYTHON_TOMLS)
        get_filename_component(PROJECT_DIR ${PATH} DIRECTORY)

        message(STATUS "  ${PROJECT_DIR}")
        file(APPEND ${OUTPUT} "  \"${PROJECT_DIR}\",\n")
    endforeach()
    file(APPEND ${OUTPUT} "]")

    foreach(DEP IN LISTS UV_DEV_DEPENDENCIES)
        message("Adding python dependency ${DEP}")
        execute_process(COMMAND ${UV} add --dev ${DEP} COMMAND_ERROR_IS_FATAL ANY)
    endforeach()


    add_custom_target(uv_sync ALL COMMAND ${UV} sync --no-progress)

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
    #     set(UV_PROJECT_NAME \"${UV_PROJECT_NAME}\")
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

