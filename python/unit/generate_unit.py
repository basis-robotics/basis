#!/usr/bin/env python3

import argparse
import yaml
import jsonschema
import jinja2
import itertools
import os

dir_path = os.path.dirname(os.path.realpath(__file__))
basis_root = dir_path + "/../../"
jinja_dir = dir_path + "/jinja/"

SCHEMA_PATH = basis_root + '/unit/schema.yaml'

OUTPUT_DIR = basis_root + '/build/tmp/'

with open(SCHEMA_PATH) as f:
    schema = yaml.safe_load(f)

def generate_unit(unit_path, output_dir, source_dir):
    # Load the files
    with open(unit_path) as f:
        unit = yaml.safe_load(f)

    # TODO: this needs to be converted to CamelCase for the cpp class name
    unit_name = os.path.basename(unit_path).split(".")[0]
    
    output_dir = f"{output_dir}/unit/{unit_name}"

    handler_output_dir = output_dir + "/handlers"
    os.makedirs(handler_output_dir, exist_ok=True)
    os.makedirs(f'{source_dir}/include/', exist_ok=True)
    os.makedirs(f'{source_dir}/src/', exist_ok=True)
    os.makedirs(f'{source_dir}/template/', exist_ok=True)

    # Validate with the schema
    jsonschema.validate(instance=unit, schema=schema)

    jinja_env = jinja2.Environment(loader=jinja2.PackageLoader("generate_unit"),
                                   extensions=['jinja2.ext.do'],
                                   undefined=jinja2.StrictUndefined)

    serializers = set()
    
    # todo: set default sync to 'all'
    
    qos_defaults = {'depth': 10}
    def merge_qos_defaults(topic: dict, defaults: dict) -> None:
        if 'qos' in topic:
            topic['qos'] = {**defaults, **topic['qos']}
        else:
            topic['qos'] = defaults
        
    # Write handler headers
    for handler_name, handler in unit['handlers'].items():
        handler.setdefault('inputs', {})
        handler.setdefault('outputs', {})
        for output in handler['outputs'].values():
            output.setdefault('optional', False)

        def handle_io(topic_name, io, input_or_output):
            cpp_topic_name = topic_name.lstrip('/').replace('/', '_')
            io['cpp_topic_name'] = cpp_topic_name
            type_serializer, cpp_type = io['type'].split(':', 1)

            merge_qos_defaults(io, qos_defaults)

            # Kludge, fixup protobuf type names
            cpp_type = cpp_type.replace(".", "::")

            io['cpp_message_type'] = cpp_type

            cpp_type = f'std::shared_ptr<const {cpp_type}>'

            io['serializer'] = type_serializer
            if type_serializer != "raw":
                serializers.add(type_serializer)

            io['cpp_type'] = cpp_type
            if 'inproc_type' in io:
                io['inproc_cpp_type'] = f'std::shared_ptr<const {io["inproc_type"]}>'
                io['cpp_type'] = f"std::variant<std::monostate, {io['cpp_type']}, {io['inproc_cpp_type']}>"

            if io.get('accumulate'):
                # TODO: actually set max size
                io['cpp_type'] = f"std::vector<{io['cpp_type']}>"

        for topic_name, input in handler['inputs'].items():
            handle_io(topic_name, input, 'input')
        for topic_name, output in handler['outputs'].items():
            handle_io(topic_name, output, 'output')
            
        template = jinja_env.get_template("handler.h.j2")
        template_output = template.render(unit_name=unit_name, handler_name=handler_name, serializers=serializers, **handler)
        
        with open(f'{handler_output_dir}/{handler_name}.h', 'w') as f:
            f.write(template_output)
    
    # Write main unit files
    unit_files = ["unit_base.h", "unit_base.cpp", "create_unit.cpp"]
    
    for unit_file in unit_files:
        template = jinja_env.get_template(f"{unit_file}.j2")
        template_output = template.render(unit_name=unit_name, **unit)
        with open(f'{output_dir}/{unit_file}', 'w') as f:
            f.write(template_output)
        
    template = jinja_env.get_template("unit.h.j2")
    template_output = template.render(in_template_dir = True, unit_name=unit_name, **unit)
    with open(f'{source_dir}/template/{unit_name}.example.h', 'w') as f:
        f.write(template_output)

    unit_main_header = f'{source_dir}/include/{unit_name}.h'
    if not os.path.exists(unit_main_header):
        template = jinja_env.get_template("unit.h.j2")
        template_output = template.render(in_template_dir = False, unit_name=unit_name, **unit)
        with open(unit_main_header, 'w') as f:
            f.write(template_output)
            
    template = jinja_env.get_template("unit.cpp.j2")
    template_output = template.render(in_template_dir = True, unit_name=unit_name, **unit)
    with open(f'{source_dir}/template/{unit_name}.example.cpp', 'w') as f:
        f.write(template_output)
        
    unit_main_source = f'{source_dir}/src/{unit_name}.cpp'
    if not os.path.exists(unit_main_source):
        template = jinja_env.get_template("unit.cpp.j2")
        template_output = template.render(in_template_dir = False, unit_name=unit_name, **unit)
        with open(unit_main_source, 'w') as f:
            f.write(template_output)

parser = argparse.ArgumentParser(description='Generates basis units')

parser.add_argument('unit_definition_file')
parser.add_argument('output_dir')
parser.add_argument('source_dir')

args = parser.parse_args()

generate_unit(args.unit_definition_file, args.output_dir, args.source_dir)