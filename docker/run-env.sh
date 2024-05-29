SCRIPT_DIR=$(cd $(dirname $0); pwd)
BASIS_ROOT=$SCRIPT_DIR/..

if [ "$(docker ps -a -q -f name=basis)" ]; then
    docker exec -it basis /bin/bash $@
else
    # Note: this relies on macos specific user mapping magic to mount with the proper permissions
    docker run -v $BASIS_ROOT:/basis --name basis --rm -it basis-env /bin/bash $@
fi