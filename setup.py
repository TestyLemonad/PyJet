from pathlib import Path

from setuptools import find_packages, setup
from pybind11.setup_helpers import Pybind11Extension, build_ext

ROOT = Path(__file__).resolve().parent
SRC = ROOT / "src"

# Keep the extension source list in sync with the engine automatically.  The
# two sample/demo translation units are intentionally excluded: they are not
# part of the Python module.
cpp_sources = sorted(
    str(path.relative_to(ROOT))
    for path in SRC.glob("*.cpp")
    if path.name not in {"Example.cpp", "Sample.cpp"}
)

headers = sorted(
    str(path.relative_to(ROOT))
    for path in list(SRC.glob("*.hpp")) + list(SRC.glob("*.h"))
)
headers += [str((SRC / "pyjet_config" / "JetConfig.hpp").relative_to(ROOT))]

ext_modules = [
    Pybind11Extension(
        "pyjet3d._pyjet",
        sources=cpp_sources,
        include_dirs=[str(SRC / "pyjet_config"), str(SRC)],
        depends=headers,
        cxx_std=17,
    ),
]

setup(
    name="pyjet3d",
    version="0.3.0",
    author="TestyLemonad",
    description="Python bindings for the Jet 3D software rasteriser",
    package_dir={"": "src"},
    packages=find_packages(where="src"),
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    zip_safe=False,
    python_requires=">=3.9",
)
