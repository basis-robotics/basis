#
mkdir src/generated/template
mkdir src/generated/include
mkdir src/generated/src

python3 /basis/python/unit/generate_unit.py simple_pub.unit.yaml  ./src/generated ./src/generated
python3 /basis/python/unit/generate_unit.py simple_sub.unit.yaml  ./src/generated ./src/generated