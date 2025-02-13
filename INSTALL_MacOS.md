# Building the *DIPlib* project on macOS

Compiling *DIPlib* requires a few programs that do not come preinstalled on macOS.
Here we offer a simple way to install these programs.

See [`INSTALL.md`](INSTALL.md) for general concepts and additional information
on the compilation options.

We mostly use the command line here, which you will find in the terminal window. To
open a terminal window, press \<Command>-\<Space> to bring up the *Spotlight* search tool,
type `terminal`, and press \<Enter>. Alternatively, in *Finder*, go to the 'Applications'
folder, find the 'Utilities' folder in it, and the 'Terminal' app inside it.

## Computers with an Apple Silicon chip (M1, M1 Pro)

If you are building *DIPlib* for use in C++ or *Python* on a Apple Silicon computer,
you don't need to do anything  special. The instructions below will result in native
(arm64) binaries that will work well with the arm64 version of *Python* and with your
own arm64 programs. If you already have *Homebrew* installed, make sure it is the
Apple Silicon native version (installed in `/opt/homebrew/`, not in `/usr/local/`),
so that all libraries and so forth that you install use the same architecture.
Note that it is possible to run two versions of *Homebrew* side by side.

However, if you are building *DIPlib* for use in *MATLAB* (the *DIPimage* toolbox),
then you need to make sure that you build for the x86_64 architecture (Intel),
otherwise they will not work with *MATLAB*. This means that all the instructions
below need to be run in the x86_64 compatibility mode (through Rosetta 2, which
is an emulation layer).

Rosetta 2 can be installed by launching any program that is built for the x86_64
architecture, if you have *MATLAB* running, you already have Rosetta 2 installed.
You can install Rosetta 2 manually by typing the following in a terminal window:
```bash
softwareupdate --install-rosetta
```

One simple way to build *DIPlib* for the x86_64 architecture is to open a terminal
in x86_64 emulation mode, and running all instructions below in that terminal.
In particular, installing *Homebrew* in such a terminal results in a x86_64 version
of *Homebrew*, which installs in `/usr/local/`. All tools and libraries installed
by this version of *Homebrew* will be for the x86_64 architecture. You can open
a terminal in x86_64 emulation mode in various ways, the simplest is to just type
```bash
arch -x86_64 zsh
```
This starts a new shell in the current terminal window. `exit` will exit this shell,
returning  you to the previous, native shell in that same terminal window.

Alternatively, you can add `-DCMAKE_OSX_ARCHITECTURES=x86_64` to your `cmake` command
(see "Building" below). This will have all tools run in native mode, but cross-compile
to produce x86_64 binaries. This works, but I had trouble getting CMake to identify
the right version of all the libraries, and attempting to link to arm64 libraries,
which of course doesn't work. I was able to build *DIPimage* this way, by disabling
all optional components that depend on external libraries.

## *Xcode*

You can install *Xcode* from the App Store if you don't already have it installed.
However, you will not need all of *Xcode*, it is the command line tools that we're after.
You can install them by typing in a terminal window:
```bash
xcode-select --install
```
This will bring up a dialog box asking if you want to install the developer command
line tools.

The developer command line tools include `git`, `make`, compilers (`clang`) and linker.

However, `cmake` is not included in this package.

## *Homebrew*

To install *CMake*, we recommend you use *Homebrew*. If you don't have *Homebrew*
installed yet, type the following in a terminal window (or rather, copy-paste it):
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```
See [the *Homebrew* web site](https://brew.sh) for up-to-date instructions.

Once *Homebrew* is installed, the following will install *CMake* (`cmake`):
```bash
brew install cmake
```

If you want to compile *DIPviewer*, you need to install `glfw` as well:
```bash
brew install glfw
```

To compile the documentation yourself (note that the compiled documentation can be found
online), you need *dox++*. See [`INSTALL_documentation.md`](INSTALL_documentation.md) for details.

Finally, macOS comes with *Python 2*. We recommend *Python 3*:
```bash
brew install python3
```

Other useful tools available through *Homebrew* are *Valgrind*, *QCacheGrind*, and
tools included in the `binutils` package, though we won't use any of them in this
guide.

## *OpenMP*

*Clang*, as provided with *Xcode*, does support *OpenMP*, but does not provide the OpenMP library.
If you want to enable parallel processing within *DIPlib*, you have two options:

1. Install the OpenMP library for use with *Xcode*'s *Clang*:
   ```bash
   brew install libomp
   ```

   You will need at least *CMake* version 3.12 for this to work (*Homebrew*'s version is suitable).

2. Install *GCC*:
   ```bash
   brew install gcc
   ```

In our experience, *Clang* is faster at compiling, but *GCC* usually produces slightly faster code.

## Cloning the repository

Next, get the source repository from *GitHub*:
```bash
mkdir ~/src
cd ~/src
git clone https://github.com/DIPlib/diplib.git
```
This creates a directory `src/diplib` in your home directory.

## Building

As explained in the [`INSTALL.md`](INSTALL.md) file, type
```bash
mkdir ~/src/diplib/target
cd ~/src/diplib/target
cmake ..
make -j check
make -j install
```

This will install to `/usr/local`. This is also where *Homebrew* puts its stuff.
If you prefer to install elsewhere, change the `cmake` line with the following:
```bash
cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/dip
```
This will install *DIPlib*, *DIPviewer*, *DIPjavaio*, *DIPimage* and the documentation
under the `dip` directory in your home directory.

Before running `make`, examine the output of `cmake` to verify all the features you need are enabled,
and that your chosen dependencies were found. The [`INSTALL.md`](INSTALL.md) file summarizes all the
CMake options to manually specify paths and configure your build.

*PyDIP* is installed separately through `pip`. Once the `install` target has finished building
and installing, run
```bash
make pip_install
```

We recommend you additionally specify the `-DCMAKE_CXX_FLAGS="-march=native"`
option to `cmake`. This will enable additional optimizations that are specific
to your computer. Note that the resulting binaries will likely be slower on another
computer, and possibly not work at all.

If you build a static version of the *DIPlib* library, *DIPimage* and *PyDIP* will not work
correctly.

Finally, if you installed the `gcc` package because you want to use *OpenMP*,
add `-DCMAKE_C_COMPILER=gcc-11 -DCMAKE_CXX_COMPILER=g++-11` to the `cmake` command
line. By default, `cmake` will find the compiler that came with *Xcode*. These
two options specify that you want to use the *GCC* compilers instead.
(**Note**: at the time of this writing, `gcc-11` and `g++-11` were the executables installed by
the `gcc` package. This will change over time, as new versions of *GCC* are adopted
by HomeBrew. Adjust as necessary.)

## Enabling *Bio-Formats*

First, make sure you have the *Java 8 SDK* (*JDK 8*) installed, you can obtain it from the
[Oracle website](http://www.oracle.com/technetwork/java/javase/downloads/index.html) for commercial
purposes, or from [jdk.java.net](https://jdk.java.net) for an open-source build. Next, download
`bioformats_package.jar` from the [*Bio-Formats* website](https://www.openmicroscopy.org/bio-formats/).
You need to add the location of this file to the `cmake` command line using the `-DBIOFORMATS_JAR=<path>`
flag.

When running *CMake* with the proper *JDK* installed, the *DIPjavaio* module becomes available.

Check the *CMake* output to see which *JNI* was found. It should match the version of Java found.
These two should be listed together, but the *JNI* output is only produced on first run. Delete the
`CMakeCache.txt` file to run `cmake` fresh and see all its output.

Sometimes the version of *JNI* found is not the one in the *JDK*. For example, on my Mac it might find the *JNI*
that belongs to *Java 6*. In this case, add `-DJAVA_HOME=<path>` to the `cmake` command line:
```bash
cmake .. -DBIOFORMATS_JAR=$HOME/java/bioformats_package.jar -DJAVA_HOME=/Library/Java/JavaVirtualMachines/jdk1.8.0_121.jdk/Contents/Home/
```
Note that these arguments to `cmake` must be combined with the arguments mentioned earlier, into a single,
long command line argument.

If an application that links to *DIPjavaio* pops up a message box saying that you need to install the
legacy *Java 6*, then your *Java 8* is not configured properly.
[See here](https://oliverdowling.com.au/2014/03/28/java-se-8-on-mac-os-x/) for instructions
on how to set it up.

## Using *DIPimage*

Once the `install` target has finished building and installing the toolbox, start
*MATLAB*. Type the following command:
```matlab
addpath('/Users/<name>/dip/share/DIPimage')
```
This will make the toolbox available (replace `/Users/<name>/dip` with the
actual path you installed to).

To get started using *DIPimage*, read the
[*DIPimage User Manual*](https://diplib.org/diplib-docs/dipimage_user_manual.html),
and look through the help, starting at
```matlab
help DIPimage
```
Or start the GUI:
```matlab
dipimage
```

## Using *PyDIP*

Once the `pip_install` target has finished installing, start *Python*.
The following command will import the *PyDIP* package as `dip`, which is shorter to
type and mimics the namespace used in the C++ library:
```python
import diplib as dip
```

To get started using *PyDIP*, look through the help, starting at
```python
help(dip)
```
