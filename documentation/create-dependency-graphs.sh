#!/bin/bash

# run from top source dir (CMAKE_SOURCE_DIR)

rm -rf build-dependency-graph
mkdir build-dependency-graph
cd build-dependency-graph

cmake --graphviz=dependencies.dot ..

mkdir -p svg
for i in `find . -maxdepth 1 -name "*.dot*"`;do dot -Tsvg $i > svg/${i#./dependencies.dot.}.svg;done

cp svg/*.svg ../documentation/dependencies

# The meaning of the node shapes in the generated graphs (taken from the cmake source):
# EXECUTABLE: house
# STATIC_LIBRARY: diamond
# SHARED_LIBRARY: polygon
# MODULE_LIBRARY: octagon

