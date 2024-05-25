SCRIPT_DIR=$(cd $(dirname $0); pwd)
BASIS_ROOT=$SCRIPT_DIR/..

docker build --tag basis --target basis -f $BASIS_ROOT/docker/Dockerfile $@ $BASIS_ROOT 
docker build --tag basis-ros --target basis-ros -f $BASIS_ROOT/docker/Dockerfile $@ $BASIS_ROOT 