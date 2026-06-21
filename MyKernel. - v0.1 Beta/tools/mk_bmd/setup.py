"""
setup.py — Compiles builder.pyx into a native extension module.

Usage (from inside tools/mk_bmd/):
    python setup.py build_ext --inplace

Requires Cython to be installed (`pip install cython`) and
mykernel_native to already be built and importable, since builder.pyx
imports it at the top level. See BUILD.md at the project root for the
full build order.
"""

from setuptools import setup
from Cython.Build import cythonize

setup(
    name="mk_bmd",
    version="0.1.0-beta",
    description="MyKernel builder tool — compiles OS projects into .justmko files",
    ext_modules=cythonize(
        "builder.pyx",
        compiler_directives={"language_level": "3"},
    ),
)
