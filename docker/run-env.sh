SCRIPT_DIR=$(cd $(dirname $0); pwd)
BASIS_ROOT=$SCRIPT_DIR/..

# Note: this relies on macos specific user mapping magic to mount with the proper permissions
docker run -v $BASIS_ROOT:/basis --name basis -it basis-env /bin/bash $@