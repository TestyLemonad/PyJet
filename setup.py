from setuptools import setup, find_packages
from pybind11.setup_helpers import Pybind11Extension, build_ext

# Все C++ исходники движка + файл биндингов
cpp_sources = [
    "src/python_bindings.cpp",
    "src/BlendSpans.cpp",
    "src/Camera.cpp",
    "src/Light.cpp",
    "src/Material.cpp",
    "src/Object.cpp",
    "src/PostFX.cpp",
    "src/Primitives.cpp",
    "src/Renderer.cpp",
    "src/Scene.cpp",
    "src/Sprite2D.cpp",
    "src/Texture.cpp",
    "src/TiledSpan.cpp",
    "src/TrigLUT.cpp",
]

ext_modules = [
    Pybind11Extension(
        "pyjet._pyjet",
        sources=cpp_sources,
        include_dirs=["src", "src/desktop"],
        cxx_std=17,
    ),
]

setup(
    name="pyjet3d",
    version="0.1.1",
    author="TestyLemonad",
    description="Python bindings for Jet 3D software rasteriser",
    package_dir={"": "src"},
    packages=find_packages(where="src"),
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    zip_safe=False,
    python_requires=">=3.9",
)
