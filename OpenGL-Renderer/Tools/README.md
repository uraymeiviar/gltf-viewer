# UX3D Tools

## Project Setup

### Creating new Files

Because CMake is collecting the source files for us, don't use IDE features to create new files. You have two options:
* create your files through the OS and rerun CMake
* use the `create_class.py` script and rerun CMake

### Visual Studio (Windows)

* Run `GenerateProjectFiles.bat`
* Open `build/Tools.sln` with Visual Studio

### CLion (Linux/MacOS)

* Open the project folder with CLion

## Static analysis with clang-tidy

Currently, static analysis works only for builds on Linux.
To enable clang-tidy for a build, enable the `UX3D_USE_CLANG_TIDY` option:

```
cmake -DUX3D_USE_CLANG_TIDY:Bool=true ..
```

When building the project, clang-tidy will use the configuration file `.clang-tidy`, located at the project root.
The file can explicitly enable and disable checks.

When changing the checks-configuration, the projects needs to be rebuilt: `make clean ; make`
