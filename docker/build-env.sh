set -e
SCRIPT_DIR=$(cd $(dirname $0); pwd)
BASIS_ROOT=$SCRIPT_DIR/..

BASIS_ENABLE_ROS=${BASIS_ENABLE_ROS:1}

docker build --tag basis-env --target basis-env -f $BASIS_ROOT/docker/Dockerfile $@ $BASIS_ROOT 

if ${BASIS_ENABLE_ROS}; then
    docker build --tag basis-env-ros --target basis-env-ros -f $BASIS_ROOT/docker/Dockerfile $@ $BASIS_ROOT 
fi