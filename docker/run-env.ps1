$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$BASIS_ROOT = Split-Path -Parent $SCRIPT_DIR
$PARENT_OF_BASIS_ROOT = Split-Path -Parent $BASIS_ROOT
$DETERMINISTIC_REPLAY_PATH = Join-Path $PARENT_OF_BASIS_ROOT "deterministic_replay"
$EXAMPLES_PATH = Join-Path $PARENT_OF_BASIS_ROOT "basis-examples"

# Check if there is a Docker container named 'basis'
$output = docker ps -a -q -f "name=^/basis$"

if ($output) {
    docker exec -it basis /bin/bash @args
} else {
    # Note: this relies on macOS-specific user mapping magic to mount with the proper permissions
    docker run $env:BASIS_DOCKER_ADDITIONAL_ARGS `
        -v "${BASIS_ROOT}:/basis" `
        -v "${DETERMINISTIC_REPLAY_PATH}:/deterministic_replay" `
        -v "${EXAMPLES_PATH}:/basis-examples" `
        --privileged --name basis --rm -it basis-env-ros /bin/bash @args
}
