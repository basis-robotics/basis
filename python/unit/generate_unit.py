import yaml
import jsonschema
import jinja2
import itertools

import os 
dir_path = os.path.dirname(os.path.realpath(__file__))

jinja_dir = dir_path + "/jinja/"

SCHEMA_PATH = '/basis/unit/schema.yaml'
UNIT_PATH = '/basis/unit/example/example.yaml'

OUTPUT_DIR = '/basis/build/tmp/'

# Load the files
with open(SCHEMA_PATH) as f:
    schema = yaml.safe_load(f)

with open(UNIT_PATH) as f:
    unit = yaml.safe_load(f)

# Validate with the schema
jsonschema.validate(instance=unit, schema=schema)

jinja_env = jinja2.Environment(loader=jinja2.PackageLoader("generate_unit"),
                               undefined=jinja2.StrictUndefined)

for handler_name, handler in unit['handlers'].items():
    handler.setdefault('inputs', {})
    handler.setdefault('outputs', {})
    for topic_name, io in itertools.chain(handler['inputs'].items(), handler['outputs'].items()):
        cpp_topic_name = topic_name.lstrip('/').replace('/', '_')
        io['cpp_topic_name'] = cpp_topic_name
        type_serializer, cpp_type = io['type'].split(':', 1)
        
        io['cpp_message_type'] = cpp_type
        
        cpp_type = f'std::shared_ptr<const {cpp_type}>'
        io['serializer'] = type_serializer
        
        

        if io.get('accumulate'):
            cpp_type = f'std::vector<{cpp_type}>'
        io['cpp_type'] = cpp_type
        # cpp_headers


    template = jinja_env.get_template("handler.h.j2")
    print(template.render(unit_name = unit['name'], handler_name=handler_name, **handler))
    
        
    



