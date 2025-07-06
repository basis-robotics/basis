include(Uv)

function(basis_uv_initialize PYPROJECT_FILE)
    # store the lock file in source, rather than ephemerally

    uv_initialize(UV_LOCK_FILE "uv.lock"
        INSTALL_DIR /opt/basis/venv
        PYTHON_VERSION ${BASIS_PYTHON_VERSION}
        UNMANAGED_PYPROJECT_FILE ${PYPROJECT_FILE}
        WORKSPACE_PACKAGE_NAME basis_cmake
        INSTALLATION_VENV /opt/basis/.venv
        INSTALLATION_VENV_CACHE /opt/basis/cache
        )


    # TODO: uv pip install pyyaml jsonschema jinja2
    # or use uvx to execute?
endfunction()