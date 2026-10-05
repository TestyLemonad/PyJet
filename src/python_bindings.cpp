#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "Jet.hpp"

namespace py = pybind11;
using namespace Renderer;

// Простая структура-обёртка для Python, чтобы безопасно владеть буфером кадра
struct PySceneWrapper {
    Scene* scene;
    uint16_t* colorBuf;
    uint16_t* depthBuf;
    int width;
    int height;

    PySceneWrapper(int w, int h) : width(w), height(h) {
        colorBuf = new uint16_t[w * h];
        depthBuf = new uint16_t[ZBUFFER_STRIDE(w) * h];
        scene = new Scene(colorBuf, depthBuf, w, h);
        scene->setClearBuffer(true);
    }

    ~PySceneWrapper() {
        delete scene;
        delete[] colorBuf;
        delete[] depthBuf;
    }
};

PYBIND11_MODULE(_pyjet, m) {
    m.doc() = "Python bindings for Jet 3D software rasteriser";

    py::enum_<ShadingMode>(m, "ShadingMode")
        .value("FLAT", ShadingMode::FLAT)
        .value("GOURAUD", ShadingMode::GOURAUD)
        .value("PHONG", ShadingMode::PHONG)
        .value("WIREFRAME", ShadingMode::WIREFRAME)
        .export_values();

    py::class_<Material>(m, "Material")
        .def(py::init<uint16_t>(), py::arg("color") = 0xFFFF)
        .def_readwrite("shadingMode", &Material::shadingMode);

    py::class_<Camera>(m, "Camera")
        .def(py::init<>())
        .def("setPosition", [](Camera& self, float x, float y, float z) {
            self.setPosition(x, y, z);
        })
        .def("setFOV", [](Camera& self, float fov) {
            self.setFOV(fov);
        });

    py::class_<PySceneWrapper>(m, "Scene")
        .def(py::init<int, int>(), py::arg("width") = JET_WIDTH, py::arg("height") = JET_HEIGHT)
        .def("setCamera", [](PySceneWrapper& self, Camera& cam) {
            self.scene->setCamera(&cam);
        })
        .def("render", [](PySceneWrapper& self) {
            self.scene->render();
        })
        .def("get_framebuffer", [](PySceneWrapper& self) {
            size_t size = static_cast<size_t>(self.width) * self.height * sizeof(uint16_t);
            return py::bytes(reinterpret_cast<const char*>(self.colorBuf), size);
        });
}
