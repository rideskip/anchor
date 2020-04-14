#!/usr/bin/env python3

import os
import subprocess
import sys

if 'NANOPB_ROOT_DIR' not in os.environ:
    sys.stderr.write("Must define NANOPB_ROOT_DIR before calling sonar_proto_gen.py\n")
    sys.exit(1)
NANOPB_ROOT_DIR = os.path.abspath(os.environ['NANOPB_ROOT_DIR'])

# Handle msys path weirdness (FIXME: this is hacky)
file_path = __file__.replace("C:", "/c").replace("\\", "/")

file_dir = os.path.dirname(os.path.realpath(file_path))

# Need to import the plugin_pb2 object from nanopb to be able to use its message options
sys.path += [os.path.join(NANOPB_ROOT_DIR, "generator")]
import proto.nanopb_pb2 as nanopb_pb2
import proto.plugin_pb2 as plugin_pb2
if not os.path.exists(os.path.join(file_dir, "build", "sonar_extensions_pb2.py")):
    # Automatically build the sonar_extensions_pb2.py file if it doesn't exist
    include_paths = [
        os.path.abspath(os.getcwd()),
        os.path.join(NANOPB_ROOT_DIR, "generator"),
        os.path.join(NANOPB_ROOT_DIR, "generator", "proto"),
        file_dir,
    ]
    os.makedirs(os.path.join(file_dir, 'build'), exist_ok=True)
    subprocess.check_call(['protoc', '--python_out=build'] + ['-I' + p for p in include_paths] + ['sonar_extensions.proto'], cwd=file_dir)
sys.path += [os.path.join(file_dir, "build")]
import sonar_extensions_pb2

def process_request(request):
    response = plugin_pb2.CodeGeneratorResponse()
    for proto_file in request.proto_file:
        if proto_file.name not in request.file_to_generate:
            # this is a dependency file, so we don't need to process it
            continue

        if request.parameter == 'python':
            f = response.file.add()
            f.name = proto_file.name.replace('.proto', '_pb2.py')
            f.insertion_point = 'module_scope'
            f.content = "MESSAGE_ID_TO_CLASS = {}"

        # iterate through each message in this proto file
        for message in proto_file.message_type:
            if not message.options.HasExtension(nanopb_pb2.nanopb_msgopt):
                # ignore messages without the nanopb_msgopt option
                continue
            elif not message.options.HasExtension(sonar_extensions_pb2.sonar_msgopt):
                # ignore messages without the sonar_msgopt option
                continue
            # grab the msgid option from nanopb
            nanopb_options = nanopb_pb2.NanoPBOptions()
            nanopb_options.MergeFrom(message.options.Extensions[nanopb_pb2.nanopb_msgopt])
            msg_id = nanopb_options.msgid
            # grab our attr_ops option
            sonar_options = sonar_extensions_pb2.SonarOptions()
            sonar_options.MergeFrom(message.options.Extensions[sonar_extensions_pb2.sonar_msgopt])
            attr_ops = sonar_options.attr_ops
            attr_ops_name = sonar_extensions_pb2.AttrOps.Name(attr_ops).replace("ATTR_OPS_", "")
            f = response.file.add()
            if request.parameter == "java":
                if 'SONAR_PROTO_GEN_REQUEST_INFO_JAVA_CLASS' not in os.environ:
                    sys.stderr.write("Must define SONAR_PROTO_GEN_REQUEST_INFO_JAVA_CLASS before calling sonar_proto_gen.py\n")
                    sys.exit(1)
                request_info_class = os.environ['SONAR_PROTO_GEN_REQUEST_INFO_JAVA_CLASS']
                # generate the path to the target java file path which we'll be modifying
                f.name = os.path.join(os.path.join(*proto_file.options.java_package.split('.')), proto_file.package + ".java")
                f.insertion_point = "class_scope:" + proto_file.package + "." + message.name
                # generate a REQUEST_INFO field
                f.content = ""
                f.content += "public static %s<%s> REQUEST_INFO;\n"%(request_info_class, message.name)
                f.content += "static {\n"
                f.content += "    REQUEST_INFO = new %s<%s>() {\n"%(request_info_class, message.name)
                f.content += "        @Override\n"
                f.content += "        public short getAttributeId() {\n"
                f.content += "            return 0x%03x;\n"%(msg_id)
                f.content += "        }\n"
                f.content += "        @Override\n"
                f.content += "        public boolean supportsNotify() {\n"
                f.content += "            return %s;\n"%("true" if "N" in attr_ops_name else "false")
                f.content += "        }\n"
                f.content += "        @Override\n"
                f.content += "        public com.google.protobuf.Parser<%s> getParser() {\n"%(message.name)
                f.content += "            return parser();\n"
                f.content += "        }\n"
                f.content += "    };\n"
                f.content += "}\n"
            elif request.parameter == "python":
                # # generate the path to the target python file path which we'll be modifying
                f.name = proto_file.name.replace('.proto', '_pb2.py')
                f.insertion_point = "module_scope"
                f.content = "MESSAGE_ID_TO_CLASS[%d] = %s" % (msg_id, message.name)
            elif request.parameter == "c":
                # generate the path to the target C header which we'll be modifying
                f.name = proto_file.name.replace(".proto", ".pb.h")
                struct_name = proto_file.package + "_" + message.name
                f.insertion_point = "struct:" + struct_name
                # generate a define for the operations which will get passed to SONAR_ATTR_DEF()
                f.content += "#define %s_ops %s\n"%(struct_name, attr_ops_name)
            else:
                raise Exception("Unknown target language (parameter=%s)"%(request.parameter))
    return response

def run():
    # Read the request from stdin
    if hasattr(sys.stdin, 'buffer'):
        data = sys.stdin.buffer.read()
    else:
        data = sys.stdin.read()
    request = plugin_pb2.CodeGeneratorRequest()
    request.ParseFromString(data)

    # Get the response and write it to stdout
    response = process_request(request)
    if hasattr(sys.stdout, 'buffer'):
        sys.stdout.buffer.write(response.SerializeToString())
    else:
        sys.stdout.write(response.SerializeToString())

if __name__ == '__main__':
    run()
