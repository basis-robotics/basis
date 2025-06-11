if(NOT DEFINED _UV_PYPROJECT_INCLUDED)
    set(_UV_PYPROJECT_INCLUDED TRUE)
    find_program(UV uv REQUIRED)

    # TODO: independent and global mode

    # find_program(UVX uvx REQUIRED)

    function(uv_initialize)
        set(options)
        set(oneValueArgs UV_LOCK_FILE)
        set(multiValueArgs)
        cmake_parse_arguments(PARSE_ARGV 0 arg
            "${options}" "${oneValueArgs}" "${multiValueArgs}"
        )
        if(DEFINED UV_INITIALIZED)
            return()
        endif()
        cmake_parse_arguments(
            PARSE_ARGV 0 COMPLEX_PREFIX "SINGLE;ANOTHER" "ONE_VALUE;ALSO_ONE_VALUE"
            "MULTI_VALUES;ANOTHER_MULTI_VALUES")
        

        set(UV_INITIALIZED TRUE)

        execute_process(
            COMMAND
            ${UV} venv --python ${BASIS_PYTHON_VERSION} --allow-existing
            COMMAND
            ${UV} python pin ${BASIS_PYTHON_VERSION}
            WORKING_DIRECTORY
            ${CMAKE_BINARY_DIR})
        
        # Ensure we always ignore whatever the shell's virtual env is and use the env defined in cmake
        set(ENV{VIRTUAL_ENV} ${CMAKE_BINARY_DIR}/.venv)

        # Allow the lock file to live in the 
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
            # if(IS_SYMLINK UV_ACTUAL_LOCK_FILE)
            #     file(READ_SYMLINK ${UV_ACTUAL_LOCK_FILE} UV_ACTUAL_LOCK_FILE_DEST)
            #     if(${UV_ACTUAL_LOCK_FILE_DEST})
            #     message(FATAL_ERROR ${UV_ACTUAL_LOCK_FILE_DEST})
            # else()

            # endif()
            #${CMAKE_BINARY_DIR}/uv.lock)
        endif()

        # TODO: uv pip install pyyaml jsonschema jinja2
        # or use uvx to execute?
        
        # Define and initialize global property once
        define_property(GLOBAL PROPERTY UV_PYTHON_TOMLS
            BRIEF_DOCS "Collected pyproject.toml files"
            FULL_DOCS "Accumulated pyproject.toml files from all subprojects")
        set_property(GLOBAL PROPERTY UV_PYTHON_TOMLS "")
    endfunction()
    # Define the function to add tomls
    function(uv_add_pyproject PROJECT)
        get_filename_component(PROJECT_DIR ${PROJECT} DIRECTORY)
        #set(PROJECT_PATH ${CMAKE_BINARY_DIR}/uv/${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_DIR})
        #file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT} DESTINATION ${PROJECT_PATH})
        #set_property(GLOBAL APPEND PROPERTY UV_PYTHON_TOMLS "${PROJECT_PATH}")
        set_property(GLOBAL APPEND PROPERTY UV_PYTHON_TOMLS "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_DIR}")
    endfunction()

    # Define the finalization logic
    function(_uv_internal_finish)
        # 


        set(OUTPUT ${CMAKE_BINARY_DIR}/pyproject.toml)

        file(WRITE ${OUTPUT} "")
        get_property(UV_PYTHON_TOMLS GLOBAL PROPERTY UV_PYTHON_TOMLS)

        set(UV_PROJECT_NAMES "")

        foreach(PATH IN LISTS UV_PYTHON_TOMLS)
            # get_filename_component(D ${PATH} DIRECTORY)
            message("${UV} --directory \"${PATH}\" version")
            execute_process(COMMAND ${UV} --directory "${PATH}" version OUTPUT_VARIABLE UV_RESULT COMMAND_ERROR_IS_FATAL ANY)
            string(REGEX MATCH "^[^ ]*" THIS_PROJECT_NAME "${UV_RESULT}")
            list(APPEND UV_PROJECT_NAMES "${THIS_PROJECT_NAME}")
        endforeach()

        # TODO: funnily enough, we could probably use jinja to generate this
        file(APPEND ${OUTPUT} "[project]\n")
        # TODO: pass this in
        file(APPEND ${OUTPUT} "name = \"basis_cmake\"\n")
        # TODO: pass this in
        file(APPEND ${OUTPUT} "version = \"0.1.1\"\n")
        file(APPEND ${OUTPUT} "dependencies = [\n")

        foreach(NAME IN LISTS UV_PROJECT_NAMES)
            file(APPEND ${OUTPUT} "  \"${NAME}\",\n")
        endforeach()


        file(APPEND ${OUTPUT} "]\n")
        file(APPEND ${OUTPUT} "\n")

        file(APPEND ${OUTPUT} "[build-system]\n")
        file(APPEND ${OUTPUT} "requires = [\"setuptools\"]\n")
        file(APPEND ${OUTPUT} "build-backend = \"setuptools.build_meta\"\n")
        file(APPEND ${OUTPUT} "\n")

        file(APPEND ${OUTPUT} "[tool.setuptools]\n")
        file(APPEND ${OUTPUT} "py-modules = []\n")
        file(APPEND ${OUTPUT} "\n")

        file(APPEND ${OUTPUT} "[tool.uv.sources]\n")
        foreach(NAME IN LISTS UV_PROJECT_NAMES)
            file(APPEND ${OUTPUT} "${NAME} = { workspace = true }\n")
        endforeach()

        file(APPEND ${OUTPUT} "[tool.uv.workspace]\n")
        file(APPEND ${OUTPUT} "members = [\n")
        foreach(PATH IN LISTS UV_PYTHON_TOMLS)
            message(STATUS "  ${PATH}")
            file(APPEND ${OUTPUT} "  \"${PATH}\",\n")
        endforeach()
        file(APPEND ${OUTPUT} "]")

        # todo: requires-python

        set(INSTALL_EDITABLE_COMMAND ${UV} pip install -e . -r "${OUTPUT}" )
        list(JOIN INSTALL_EDITABLE_COMMAND " " INSTALL_COMMAND_STR)
        message(${INSTALL_COMMAND_STR})
        # TODO make target?
        execute_process(COMMAND ${INSTALL_EDITABLE_COMMAND} COMMAND_ERROR_IS_FATAL ANY)


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


endif()