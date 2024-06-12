import yaml
import jsonschema

SCHEMA_PATH = "/basis/unit/schema.yaml"
UNIT_PATH = "/basis/unit/example/example.yaml"


with open(SCHEMA_PATH) as f:
    schema = yaml.safe_load(f)

with open(UNIT_PATH) as f:
    unit = yaml.safe_load(f)
jsonschema.validate(instance=unit, schema=schema)


