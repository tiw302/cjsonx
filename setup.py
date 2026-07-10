import sys
from setuptools import setup, Extension
import pybind11

import platform
from pathlib import Path

# read the readme file for the long description
this_directory = Path(__file__).parent
long_desc = (this_directory / "README.md").read_text(encoding="utf-8")

extra_compile_args = []
if sys.platform == 'win32':
    extra_compile_args = ['/O2']
else:
    extra_compile_args = ['-O3', '-D_GNU_SOURCE']
    if platform.machine() in ('x86_64', 'AMD64'):
        extra_compile_args += ['-mavx2', '-mpclmul']

ext_modules = [
    Extension(
        'cjsonx',
        ['python/cjsonx_python.cpp', 'src/cjsonx.c'],
        include_dirs=[pybind11.get_include(), 'include'],
        language='c++',
        extra_compile_args=extra_compile_args
    ),
]

setup(
    name='cjsonx',
    version='1.4.3',
    description="High-performance JSON parser C11 bindings for Python",
    long_description=long_desc,
    long_description_content_type="text/markdown",
    ext_modules=ext_modules,
    packages=[],
    zip_safe=False,
)
