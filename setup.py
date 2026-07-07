import sys
from setuptools import setup, Extension
import pybind11

import platform

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
    version='1.1.1',
    description="High-performance JSON parser C11 bindings for Python",
    ext_modules=ext_modules,
    packages=[],
    zip_safe=False,
)
