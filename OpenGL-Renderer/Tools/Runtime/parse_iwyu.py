from pathlib import Path

iwyu_log_path = Path("iwyu.log")

fixes = []
messages = []

def execute():
	with iwyu_log_path.open() as iwyu_log_file:
		iwyu_log = iwyu_log_file.readlines()

	parse(iwyu_log)


def parse(log):
	path = ""
	in_iwyu_part = False
	in_lines_to_add = False
	in_lines_to_remove = False

	for line in log:
		line = line.strip()

		if "include-what-you-use reported diagnostics:" in line:
			in_iwyu_part = True
			continue

		if in_iwyu_part:
			if "should add these lines:" in line:
				path = parse_path(line)
				in_lines_to_add = True
				continue
			if "should remove these lines:" in line:
				path = parse_path(line)
				in_lines_to_remove = True
				continue
			if "---" == line:
				path = ""
				in_iwyu_part = False
				continue

		if in_lines_to_add:
			if "" == line:
				in_lines_to_add = False
				continue

			if "class" in line and ".cpp" not in path:
				fix = fix_object(path)
				fix["forward_declare"] = parse_forward_declaration(line)
				fixes.append(fix)

		if in_lines_to_remove:
			if "" == line:
				in_lines_to_remove = False
				continue

			fix = fix_object(path)
			fix["remove"] = line
			fixes.append(fix)

	fixes_by_file = {}
	for fix in fixes:
		if fix["path"] not in fixes_by_file:
			fixes_by_file[fix["path"]] = []
		fixes_by_file[fix["path"]].append(fix)

	for file, file_fixes in fixes_by_file.items():
		print("{}:".format(file))
		for fix in file_fixes:
			if "forward_declare" in fix:
				print("    forward decare '{}'".format(fix["forward_declare"]))
			if "remove" in fix:
				print("    remove '{}'".format(fix["remove"]))
		print()


def fix_object(path):
	fix = {}
	fix["path"] = path
	fix["project"] = parse_project(path)
	fix["module"] = parse_module(path)
	fix["file"] = parse_file(path)
	return fix

def parse_path(line):
	return line.split("should")[0].split("middleware/")[1].strip()

def parse_file(path):
	return path.split("/")[-1]

def parse_project(path):
	return path.split("/")[0]

def parse_module(path):
	return path.split("/")[1]

def parse_forward_declaration(line):
	return line


execute()