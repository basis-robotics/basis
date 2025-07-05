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
    set(options)
    set(oneValueArgs UV_LOCK_FILE UV_PYTHON_VERSION UV_PROJECT_NAME)
    set(multiValueArgs)
    # TODO: can we use "" instead of arg, and have it work magically with globals as well?
    # do we want that?
    cmake_parse_arguments(PARSE_ARGV 0 arg
        "${options}" "${oneValueArgs}" "${multiValueArgs}"
    )

    if(DEFINED UV_INITIALIZED)
        return()
    endif()
    set(UV_INITIALIZED TRUE)


    execute_process(
        COMMAND
        # create the venv - would normally be done by uv sync but we also want to install the python version
        ${UV} venv --python ${arg_UV_PYTHON_VERSION} --allow-existing
        # COMMAND
        # ${UV} python pin ${arg_UV_PYTHON_VERSION}
        WORKING_DIRECTORY
        ${CMAKE_BINARY_DIR})

    # Ensure we always ignore whatever the shell's virtual env is and use the env defined in cmake
    # TODO: should this be at the top of the file?
    set(ENV{VIRTUAL_ENV} ${CMAKE_BINARY_DIR}/.venv)

    if(NOT DEFINED arg_UV_PROJECT_VERSION)
        set(arg_UV_PROJECT_VERSION 0.0.0)
    endif()

    # TODO: move pyproject
    # Allow the lock file to live in the repo rather than in build/
    if(DEFINED arg_UV_LOCK_FILE)
        # ${CMAKE_CURRENT_SOURCE_DIR}/${arg_UV_LOCK_FILE}
        # Convert to absolute path
        cmake_path(ABSOLUTE_PATH arg_UV_LOCK_FILE)
        # Then relative to binary dir
        cmake_path(RELATIVE_PATH arg_UV_LOCK_FILE BASE_DIRECTORY ${CMAKE_BINARY_DIR})
        #
        message("new uv path ${arg_UV_LOCK_FILE}")
        set(UV_ACTUAL_LOCK_FILE ${CMAKE_BINARY_DIR}/uv.lock)

        # Note: we could do a dance and check the symlink location if it exists, or just overwrite whatever is there
        execute_process(COMMAND ln -s -f ${arg_UV_LOCK_FILE} ${UV_ACTUAL_LOCK_FILE} COMMAND_ERROR_IS_FATAL ANY)
    endif()

    # Define and initialize global property once
    define_property(GLOBAL PROPERTY UV_PYTHON_VERSION
        BRIEF_DOCS "Collected pyproject.toml files"
        FULL_DOCS "Accumulated pyproject.toml files from all subprojects")
    # TODO: check if this has changed and warn otherwise
    # Switching pythons doesn't currently work if you use FindPython anywhere
    # It appears to find the binary, but headers and shared objects aren't picked up
    set_property(GLOBAL PROPERTY UV_PYTHON_VERSION ${arg_UV_PYTHON_VERSION})

    define_property(GLOBAL PROPERTY UV_PYTHON_TOMLS
        BRIEF_DOCS "Collected pyproject.toml files"
        FULL_DOCS "Accumulated pyproject.toml files from all subprojects")
    set_property(GLOBAL PROPERTY UV_PYTHON_TOMLS "")

    define_property(GLOBAL PROPERTY UV_PROJECT_NAME
        BRIEF_DOCS "uv project name"
        FULL_DOCS "Name for generated pyproject.toml")
    set_property(GLOBAL PROPERTY UV_PROJECT_NAME ${arg_UV_PROJECT_NAME})

    define_property(GLOBAL PROPERTY UV_PROJECT_VERSION
        BRIEF_DOCS "uv project version"
        FULL_DOCS "Version for generated pyproject.toml")
    set_property(GLOBAL PROPERTY UV_PROJECT_VERSION ${arg_UV_PROJECT_VERSION})
    
    define_property(GLOBAL PROPERTY UV_DEV_DEPENDENCIES
        BRIEF_DOCS "uv dev dependencies"
        FULL_DOCS "--dev dependencies, typically needed by helpers called via CMake")
    set_property(GLOBAL PROPERTY UV_DEV_DEPENDENCIES "")
endfunction()
# Define the function to add tomls
function(uv_add_pyproject PROJECT)
    set_property(GLOBAL APPEND PROPERTY UV_PYTHON_TOMLS "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT}")
endfunction()

function(uv_add_dev_dependency DEP)
    set_property(GLOBAL APPEND PROPERTY UV_DEV_DEPENDENCIES ${DEP})
endfunction()

# Define the finalization logic
function(_uv_internal_finish)
    set(OUTPUT ${CMAKE_BINARY_DIR}/pyproject.toml)

    file(WRITE ${OUTPUT} "")
    get_property(UV_PYTHON_TOMLS GLOBAL PROPERTY UV_PYTHON_TOMLS)
    get_property(UV_PYTHON_VERSION GLOBAL PROPERTY UV_PYTHON_VERSION)
    get_property(UV_PROJECT_NAME GLOBAL PROPERTY UV_PROJECT_NAME)
    get_property(UV_PROJECT_VERSION GLOBAL PROPERTY UV_PROJECT_VERSION)
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


    # TODO pass in
    execute_process(COMMAND ${UV} add --dev pyyaml COMMAND_ERROR_IS_FATAL ANY)
    execute_process(COMMAND ${UV} add --dev jsonschema COMMAND_ERROR_IS_FATAL ANY)
    foreach(DEP IN LISTS UV_DEV_DEPENDENCIES)
        message("Adding python dependency ${DEP}")
        execute_process(COMMAND ${UV} add --dev ${DEP} COMMAND_ERROR_IS_FATAL ANY)
    endforeach()


    # set(INSTALL_EDITABLE_COMMAND ${UV} pip install -e . -r "${OUTPUT}" )
    # list(JOIN INSTALL_EDITABLE_COMMAND " " INSTALL_COMMAND_STR)
    # message(${INSTALL_COMMAND_STR})

    add_custom_target(uv_sync ALL COMMAND ${UV} sync --no-progress)

    # todo: run target

    # We could depend on all pyproject tomls this way, but it wouldn't catch
    # references of references. Instead, just invoke uv every time
    # add_custom_target(uv_sync ALL
    #     DEPENDS ${CMAKE_BINARY_DIR}/.venv/some_marker)
    # add_custom_command(
    #     OUTPUT ${CMAKE_BINARY_DIR}/.venv/some_marker
    #     DEPENDS ${UV_PYTHON_TOMLS}
    #     COMMAND ${UV} sync --no-progress
    #     COMMAND touch ${CMAKE_BINARY_DIR}/.venv/some_marker)


#        execute_process(COMMAND ${INSTALL_EDITABLE_COMMAND} COMMAND_ERROR_IS_FATAL ANY)

    install(CODE "execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cmake/install_uv.sh COMMAND_ERROR_IS_FATAL ANY)")
    # maybe when uv sync???

    # probably:
    # uv lock --frozen or whatever to ensure lock file is up to date
    # uv sync      ???
    # uv pip compile -> req --no-editable??

    # uv export --frozen --no-emit-workspace


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
cmake_language(DEFER DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} CALL _uv_internal_finish)



# install step
# 1. lock
# 2. uv export
# 3. create venv at DEST
# 4. uv pip install install --compile-bytecode --target DEST -r requirements.txt

