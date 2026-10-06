from setuptools import setup, find_packages
from pybind11.setup_helpers import Pybind11Extension, build_ext

ext_modules = [
    Pybind11Extension(
        "pyjet._pyjet",
        sources=["src/python_bindings.cpp"],
        include_dirs=["src", "src/desktop"],
        cxx_std=17,
    ),
]

setup(
    name="pyjet",
    version="0.1.0",
    author="TestyLemonad",
    description="Python bindings for Jet 3D software rasteriser",
    package_dir={"": "src"},
    packages=find_packages(where="src"),
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    zip_safe=False,
    python_requires=">=3.8",
)
