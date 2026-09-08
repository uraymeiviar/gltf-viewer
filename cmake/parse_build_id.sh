set -e

prefix="$1"
output_path="$2"

commit_hash=$(git rev-parse --short HEAD)
dirty_files=$(git status --porcelain)

if [ "$prefix" != "" ]
then
	build_id="$prefix-"
fi

build_id="${build_id}${commit_hash}"

if [ "$dirty_files" != "" ]
then
	build_id="${build_id}*"
fi

if [ "$output_path" != "" ]
then
	echo "$build_id" > "$output_path"
else
	echo "$build_id"
fi
