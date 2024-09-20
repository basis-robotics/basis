SCRIPT_DIR=$(cd $(dirname $0); pwd)
BASIS_ROOT=$SCRIPT_DIR/..

if [ "$(docker ps -a -q -f name=basis)" ]; then
    docker exec -it basis /bin/bash $@
else
    # Note: this relies on macos specific user mapping magic to mount with the proper permissions
    docker run $BASIS_DOCKER_ADDITIONAL_ARGS -v $BASIS_ROOT:/basis -v $BASIS_ROOT/../deterministic_replay:/deterministic_replay --privileged --name basis --rm -it basis-env-ros /bin/bash $@
fi