#!/usr/bin/env bash
set -e
shopt -s expand_aliases


INSTALLATION_VENV="/opt/basis/.venv"
UV=/home/basis/.local/bin/uv
alias uv="$UV"
PROJECT_NAME="basis_cmake"
# UV_PROJECT= ?
# Don't allow installing if our lockfile isn't up to date
uv lock --check 

# TODO: hashes? no hashes?
uv export --frozen --no-emit-workspace --no-hashes -o dist/requirements.txt

# TODO: this does the wrong thing w/sudo
uv build --wheel --all-packages

PROJECT_VERSION=$(echo "import importlib.metadata; print(importlib.metadata.version('${PROJECT_NAME}'))" | uv run -)

# Create a new venv with the correct python version
# TODO: handle wrong python version in install
# TODO: use of XDG_DATA_HOME isn't amazing here but i guess it works
XDG_DATA_HOME=/opt/basis/cache uv venv $INSTALLATION_VENV --python $(cat .python-version)

VIRTUAL_ENV=${INSTALLATION_VENV} uv pip install dist/*.whl -c dist/requirements.txt
# -c requirements-frozen.txt dist/foo-0.1.0-py3-none-any.whl  

#echo ""

#echo $DEPENDS

 

#uv
#uv pip compile
        # uv pip compile -> req --no-editable??

        # uv export --frozen --no-emit-workspace