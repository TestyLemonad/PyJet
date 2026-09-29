#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "Jet.hpp"

namespace py = pybind11;
using namespace Renderer;

PYBIND11_MODULE(_pyjet, m) {
    m.doc() = "Python bindings for Jet 3D software rasteriser";

    // Пробрасываем перечисление ShadingMode
    py::enum_<ShadingMode>(m, "ShadingMode")
        .value("FLAT", ShadingMode::FLAT)
        .value("GOURAUD", ShadingMode::GOURAUD)
        .value("PHONG", ShadingMode::PHONG)
        .value("WIREFRAME", ShadingMode::WIREFRAME)
        .export_values();

    // Обёртка для Материала
    py::class_<Material>(m, "Material")
        .def(py::init<uint16_t>(), py::arg("color") = 0xFFFF)
        .def_readwrite("shadingMode", &Material::shadingMode);

    // Обёртка для Камеры
    py::class_<Camera>(m, "Camera")
        .def(py::init<>())
        .def("setPosition", &Camera::setPosition)
        .def("setFOV", &Camera::setFOV);

    // Обёртка для Сцены
    py::class_<Scene>(m, "Scene")
        .def(py::init([](int w, int h) {
            // Автоматически выделяем память под кадр
            uint16_t* colorBuf = new uint16_t[w * h];
            uint16_t* depthBuf = new uint16_t[ZBUFFER_STRIDE(w) * h];
            auto scene = new Scene(colorBuf, depthBuf, w, h);
            scene->setClearBuffer(true);
            return scene;
        }))
        .def("setCamera", &Scene::setCamera)
        .def("render", &Scene::render)
        // Метод для получения пикселей кадра в Python (байты RGB565)
        .def("get_framebuffer", [](Scene& self) {
            int size = self.getWidth() * self.getHeight() * sizeof(uint16_t);
            return py::bytes(reinterpret_cast<const char*>(self.getColorBuffer()), size);
        });
}
