#
# python3 /basis/python/unit/generate_unit.py simple_pub.unit.yaml  ./generated .
mkdir src/template
mkdir src/include

python3 /basis/python/unit/generate_unit.py simple_pub.unit.yaml  ./src/generated .
python3 /basis/python/unit/generate_unit.py simple_sub.unit.yaml  ./src/generated .