# Run this on MacOS with Xcode from the diplib directory
# set $PYPI_TOKEN to the PyPI token for the diplib project

# Setup
export BUILD_THREADS=3
export DELOCATE=`pwd`/tools/travis/delocate

brew install python@3.7
brew install python@3.8
brew install python@3.9
brew install python@3.10
# The install above might have changed the default version of `python3`, so we need to reinstall packages:
python3 -m pip install setuptools wheel twine delocate

mkdir build
cd build
wget https://downloads.openmicroscopy.org/bio-formats/6.5.0/artifacts/bioformats_package.jar

# Basic configuration
cmake .. -DDIP_PYDIP_WHEEL_INCLUDE_LIBS=On -DBIOFORMATS_JAR=`pwd`/bioformats_package.jar -DDIP_BUILD_DIPIMAGE=Off

# Python 3.7
export PYTHON=/usr/local/opt/python@3.7/bin/python3
export PYTHON_VERSION=3.7
cmake .. -DPYBIND11_PYTHON_VERSION=$PYTHON_VERSION -DPYTHON_EXECUTABLE=$PYTHON
make -j $BUILD_THREADS bdist_wheel
python3 $DELOCATE -w wheelhouse/ -v pydip/staging/dist/*.whl

# Python 3.8
export PYTHON=/usr/local/opt/python@3.8/bin/python3
export PYTHON_VERSION=3.8
cmake .. -DPYBIND11_PYTHON_VERSION=$PYTHON_VERSION -DPYTHON_EXECUTABLE=$PYTHON
make -j $BUILD_THREADS bdist_wheel
python3 $DELOCATE -w wheelhouse/ -v pydip/staging/dist/*.whl

# Python 3.9
export PYTHON=/usr/local/opt/python@3.9/bin/python3
export PYTHON_VERSION=3.9
cmake .. -DPYBIND11_PYTHON_VERSION=$PYTHON_VERSION -DPYTHON_EXECUTABLE=$PYTHON
make -j $BUILD_THREADS bdist_wheel
python3 $DELOCATE -w wheelhouse/ -v pydip/staging/dist/*.whl

# Python 3.10
export PYTHON=/usr/local/opt/python@3.10/bin/python3
export PYTHON_VERSION=3.10
cmake .. -DPYBIND11_PYTHON_VERSION=$PYTHON_VERSION -DPYTHON_EXECUTABLE=$PYTHON
make -j $BUILD_THREADS bdist_wheel
python3 $DELOCATE -w wheelhouse/ -v pydip/staging/dist/*.whl

# Upload to pypi.org
python3 -m twine upload -u __token__ -p $PYPI_TOKEN wheelhouse/*.whl
