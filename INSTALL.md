# Building *DIPlib*, *DIPimage* and *PyDIP*


## Linux, MacOS, Cygwin and other Unix-like systems

To build the library you will need a C++14 compliant compiler and *CMake*.
See below under "Dependencies" for optional dependencies that you can install to
improve your *DIPlib* experience.

Use the following commands to build:
```bash
mkdir target
cd target
cmake /path/to/dip/root/directory
make -j install
```

The `-j` option to `make` enables a multi-threaded build. Limit the number of
concurrent jobs to, for example, 4 with `-j4`. On system with limited memory,
reduce the number of concurrent jobs if compilation stalls and the system thrashes.

See below under "*CMake* configuration" for other `make` targets and *CMake* configuration
options.

For step-by-step instructions for Ubuntu Linux, see [`INSTALL_Linux.md`](INSTALL_Linux.md).

For step-by-step instructions for MacOS, see [`INSTALL_MacOS.md`](INSTALL_MacOS.md).


## Windows

Unless you want to use *Cygwin* or *MinGW* (see above), we recommend *Microsoft Visual Studio 2019*
(version 16) or newer. You'll also need *CMake*.

Using *CMake-gui*, choose where the source directory is and where to build the binaries. Then
press "Configure" and select *Visual Studio*. Finally, press "Generate". You should now have
a *Visual Studio* solution file that you can open in *Visual Studio* and build as usual.

See below under "*CMake* configuration" for generated targets, and *CMake* configuration
options.

For step-by-step instructions, see [`INSTALL_Windows.md`](INSTALL_Windows.md).

See below for optional dependencies that you can install to improve your *DIPlib* experience.


## *CMake* configuration

Available `make` targets:

    all (default) # builds all configured targets (see below)
    install       # builds 'all' and installs everything except PyDIP
    check         # builds the unit_tests program and runs it
    check_memory  # ...and runs it under valgrind
    apidoc        # builds all the HTML documentation
    examples      # builds the examples
    package       # creates a distributable package for 'install'
    bdist_wheel   # builds a Python wheel for PyDIP
    pip_install   # builds and installs a Python wheel for PyDIP
    pip_uninstall # uninstalls PyDIP

The following `make` targets are part of the `all` target:

    DIP           # builds the DIPlib library
    DIPviewer     # builds the DIPviewer module (plus the DIPlib library)
    DIPjavaio     # builds the DIPjavaio module (plus the DIPlib library)
    PyDIP         # builds the PyDIP Python module (includes DIP, DIPviewer and
                  #    DIPjavaio targets)
    dipview       # builds the standalone DIP image viewer
    dipviewjava   # builds the java-enabled standalone DIP image viewer

The `apidoc` target requires that *dox++* is available, the target will not be available
if it is not; this target will fail to build if additional tools are not installed.
See [`INSTALL_documentation.md`](INSTALL_documentation.md) for details.

Important `cmake` command-line arguments controlling the build of *DIPlib*:

    -DCMAKE_INSTALL_PREFIX=$HOME/dip   # choose an instal location for DIPlib, DIPimage and the docs
    -DCMAKE_BUILD_TYPE=Debug           # by default it is release
    -DDIP_SHARED_LIBRARY=Off           # build a static DIPlib library (not compatible with DIPimage)
    -DCMAKE_C_COMPILER=gcc-6           # specify a C compiler (for libics, LibTIFF, libjpeg and zlib)
    -DCMAKE_CXX_COMPILER=g++-6         # specify a C++ compiler (for everything else)
    -DCMAKE_CXX_FLAGS="-march=native"  # specify additional C++ compiler flags

    -DDIP_ENABLE_STACK_TRACE=Off       # disable stack trace generation on exception
    -DDIP_ENABLE_ASSERT=On             # enable asserts
    -DDIP_ENABLE_DOCTEST=Off           # disable doctest within DIPlib
    -DDIP_ENABLE_MULTITHREADING=Off    # disable OpenMP multithreading
    -DDIP_ENABLE_ICS=Off               # disable ICS file format support
    -DDIP_ENABLE_TIFF=Off              # disable TIFF file format support
    -DDIP_ENABLE_JPEG=Off              # disable JPEG file format support, also affects TIFF
    -DDIP_ENABLE_ZLIB=Off              # disable zlib compression support in ICS and TIFF
    -DDIP_ENABLE_FFTW=On               # enable the use of the FFTW3 library
    -DDIP_ENABLE_FREETYPE=On           # enable the use of the FreeType2 library
    -DDIP_ENABLE_UNICODE=Off           # disable UTF-8 strings within DIPlib
    -DDIP_ALWAYS_128_PRNG=On           # use the 128-bit PRNG code where 128-bit
                                       #    integers are not natively supported

Controlling the build of *DIPviewer*:

    -DDIP_BUILD_DIPVIEWER=Off          # don't build/install the DIPviewer module

Controlling the build of *DIPjavaio*:

    -DDIP_BUILD_JAVAIO=Off             # don't build/install the DIPjavaio module
    -DJAVA_HOME=<path>                 # use the JDK in <path>
    -DBIOFORMATS_JAR=<path>/bioformats_package.jar
                                       # specify location of the Bio-Formats JAR file

Controlling the build of *DIPimage*:

    -DDIP_BUILD_DIPIMAGE=Off           # don't build/install the DIPimage MATLAB toolbox
    -DMatlab_ROOT_DIR=<path>           # compile DIPimage against MATLAB in <path>

Controlling the build of *PyDIP*:

    -DDIP_BUILD_PYDIP=Off              # don't build the PyDIP Python module
    -DPYBIND11_PYTHON_VERSION=3.6      # compile PyDIP against Python 3.6
    -DPYTHON_EXECUTABLE=<path>         # in case the Python binary is not on the path
                                       # and Pybind11 can't find it.
    -DDIP_PYDIP_WHEEL_INCLUDE_LIBS=On  # include libraries in PyDIP wheel (turn on for
                                       # binary distribution, keep off for personal builds)

Some of these options might not be available on your system. For example, if you don't have
*MATLAB* installed, the `DIP_BUILD_DIPIMAGE` option will not be defined. In this case, setting
it to `Off` will yield a warning message when running *CMake*.

Note that on some platforms, the Python module requires the *DIPlib* library to build as
a dynamic load library (`-DDIP_SHARED_LIBRARY=On`, which is the default).


## Dependencies

Here we list all external dependencies needed to compile the various parts of the project. *DIPlib*
also depends on a few other external projects, whose sources are included in this repository (see
[`README.md`](README.md) under "License" for more information). Note that, with the exception of
dynamic linking to a few external libraries, none of these dependencies are required when using the
*DIPlib* library (that is, *DIPlib*'s public header files do not import headers from other projects).

If you have [*FFTW3*](http://www.fftw.org) installed, you can set the `DIP_ENABLE_FFTW`
*CMake* variable to have *DIPlib* use *FFTW3* instead of the default *PocketFFT* library.
*FFTW3* is usually a bit faster, but it has a copyleft license.

If you have [*FreeType 2*](https://www.freetype.org) installed, you can set the `DIP_ENABLE_FREETYPE`
*CMake* variable to have *DIPlib* use *FreeType* for rendering text, the
[`dip::FreeTypeTool`](https://diplib.org/diplib-docs/dip-FreeTypeTool.html)
class (introduced in *DIPlib* 3.1.1) cannot be instantiated without it.

*DIPviewer* requires that *OpenGL* be available on your system (should come with the OS),
as well as one of [*FreeGLUT*](http://freeglut.sourceforge.net) or [*GLFW*](http://www.glfw.org).

*DIPjavaio* requires that the *Java 8 SDK* (*JDK 8*) or later be installed. This module is intended as a
bridge to [*OME Bio-Formats*](https://www.openmicroscopy.org/bio-formats/), which you will need
to download separately. *Bio-Formats* has a copyleft license.

*DIPimage* requires that [*MATLAB*](https://www.mathworks.com/products/matlab.html) be installed
for compilation and (of course) execution.
Optionally, you can install [*OME Bio-Formats*](https://www.openmicroscopy.org/bio-formats/) to
enable *DIPimage* to read many microscopy image file formats (type `help readim` in *MATLAB*,
after installing *DIPimage*, to learn more).

*PyDIP* requires that [Python 3](https://www.python.org) be installed.


## Linking against the library

When using *CMake*, simply import the `DIPlib` package with `find_package()`, then link against the
imported `DIPlib::DIP`, `DIPlib::DIPviewer` and/or `DIPlib::DIPjavaio` targets and everything will
be configured correctly, see the [example *CMake* script](examples/independent_project/CMakeLists.txt).
Alternatively, use `add_subdirectory()` to include the *DIPlib* repository as a sub-project, and
then link against the `DIP`, `DIPviewer` and/or `DIPjavaio` targets.

If you do not use *CMake*, there are several macros that you should define when building any program
that links against *DIPlib*:

If *DIPlib* was build with the `DIP_SHARED_LIBRARY` flag not set, then you need to define the `DIP_CONFIG_DIP_IS_STATIC`
macro when compiling the code that links against it. Likewise, if the `DIP_ALWAYS_128_PRNG` flag was set,
then you must define a `DIP_CONFIG_ALWAYS_128_PRNG` macro when compiling your program. Mismatching this flag
could cause your program to not link, or worse, crash at runtime.

The following flags do not need to be matched, but they should be if you want the inline functions to behave
the same as the pre-compiled ones:
- If the flag `DIP_ENABLE_STACK_TRACE` is set, define the macro `DIP_CONFIG_ENABLE_STACK_TRACE`.
- If the flag `DIP_ENABLE_ASSERT` is set, define the macro `DIP_CONFIG_ENABLE_ASSERT`.

Also, if your compiler supports `__PRETTY_FUNCTION__`, set the macro `DIP_CONFIG_HAS_PRETTY_FUNCTION` to
get better stack traces.

For *DIPviewer*, if `DIP_SHARED_LIBRARY` was not set, define the `DIP_CONFIG_DIPVIEWER_IS_STATIC` macro.
Also define `DIP_CONFIG_HAS_FREEGLUT` or `DIP_CONFIG_HAS_GLFW` depending on which back-end is used.

For *DIPjavaio*, if `DIP_SHARED_LIBRARY` was not set, define the `DIP_CONFIG_DIPJAVAIO_IS_STATIC` macro.
Also define `DIP_CONFIG_HAS_DIPJAVAIO` for the function `dip::ImageRead` to be able to make use of it.
