include(uvtarget/Uv)

function(basis_uv_initialize PYPROJECT_FILE)
    uv_initialize(
        PYTHON_VERSION ${BASIS_PYTHON_VERSION}
        UNMANAGED_PYPROJECT_FILE ${PYPROJECT_FILE}
        WORKSPACE_PACKAGE_NAME basis_cmake
        INSTALLATION_VENV /opt/basis/.venv
        INSTALLATION_VENV_CACHE /opt/basis/cache
        )
endfunction()
