from pathlib import Path

class_path = input("Enter the class path (e.g. tools/gltf/json/TestClass): ")

parts = class_path.split("/")

if len(parts) < 3:
	print("Path must have at least 3 parts (project/module/Class)")
	exit(1)

project = parts[0]
module = parts[1]
class_name = parts[-1]
subfolder = "/".join(parts[2:-1])

header_path = Path("/".join([project, module, "include", module, subfolder, class_name + ".h"]).replace("//", "/"))
source_path = Path("/".join([project, module, "source", subfolder, class_name + ".cpp"]).replace("//", "/"))
tests_path = Path("/".join([project, module, "tests", subfolder, class_name + "Tests.cpp"]).replace("//", "/"))

if not header_path.parent.exists():
	print("'{}' does not exist".format(header_path.parent))
	exit(1)
if not source_path.parent.exists():
	print("'{}' does not exist".format(source_path.parent))
	exit(1)
if not tests_path.parent.exists():
	print("'{}' does not exist".format(tests_path.parent))
	exit(1)

print("Creating header file at '{}'".format(header_path))
print("Creating source file at '{}'".format(source_path))
print("Creating tests file at '{}'".format(tests_path))

with Path(header_path).open("w") as header_file:
	lines = []
	lines += "// Copyright (c) 2019 UX3D GmbH. All rights reserved.\n"
	lines += "\n"
	lines += "#pragma once\n"
	lines += "\n"
	lines += "namespace ux3d::{}\n".format(module)
	lines += "{\n"
	lines += "\tclass {}\n".format(class_name)
	lines += "\t{\n"
	lines += "\n"
	lines += "\t};\n"
	lines += "}\n"

	header_file.writelines(lines)

with Path(source_path).open("w") as source_file:
	lines = []
	lines += "// Copyright (c) 2019 UX3D GmbH. All rights reserved.\n"
	lines += "\n"
	lines += "#include \"{}.h\"\n".format("/".join([module, subfolder, class_name]).replace("//", "/"))

	source_file.writelines(lines)

with Path(tests_path).open("w") as tests_file:
	lines = []
	lines += "// Copyright (c) 2019 UX3D GmbH. All rights reserved.\n"
	lines += "\n"
	lines += "#include <catch2/catch.hpp>\n"
	lines += "#include \"{}.h\"\n".format("/".join([module, subfolder, class_name]).replace("//", "/"))
	lines += "\n"
	lines += "namespace ux3d::{}::tests\n".format(module)
	lines += "{\n"
	lines += "\tSCENARIO(\"{}\", \"[{}.{}.{}]\")\n".format(class_name, project, module, class_name)
	lines += "\t{\n"
	lines += "\t\tCHECK(false);\n"
	lines += "\t}\n"
	lines += "}\n"

	tests_file.writelines(lines)
