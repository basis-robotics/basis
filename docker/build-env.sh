set -e
SCRIPT_DIR=$(cd $(dirname $0); pwd)
BASIS_ROOT=$SCRIPT_DIR/..

docker build --tag basis-env --target basis-env -f $BASIS_ROOT/docker/Dockerfile $@ $BASIS_ROOT 
docker build --tag basis-env-ros --target basis-env-ros -f $BASIS_ROOT/docker/Dockerfile $@ $BASIS_ROOT 