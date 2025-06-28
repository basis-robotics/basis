include(Uv)

function(basis_uv_initialize)
    # store the lock file in source, rather than ephemerally

    uv_initialize(UV_LOCK_FILE "uv.lock"
        INSTALL_DIR /opt/basis/venv
        UV_PYTHON_VERSION ${BASIS_PYTHON_VERSION}
        UV_PROJECT_NAME basis_cmake)


    # TODO: uv pip install pyyaml jsonschema jinja2
    # or use uvx to execute?
endfunction()