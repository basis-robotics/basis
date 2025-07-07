function(basis_uv_initialize PYPROJECT_FILE)
    include(FetchContent)
    FetchContent_Declare(
        uvtarget
        GIT_REPOSITORY https://github.com/basis-robotics/uvtarget
        GIT_TAG main
    )
    FetchContent_MakeAvailable(uvtarget)

    include(${uvtarget_SOURCE_DIR}/Uv.cmake)
    uv_initialize(
        PYTHON_VERSION ${BASIS_PYTHON_VERSION}
        MANAGED_PYPROJECT_FILE ${PYPROJECT_FILE}
        WORKSPACE_PACKAGE_NAME basis_cmake
        INSTALLATION_VENV /opt/basis/.venv
        INSTALLATION_VENV_CACHE /opt/basis/cache
        )
endfunction()
