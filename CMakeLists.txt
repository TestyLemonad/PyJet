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
        .def("setPosition", [](Camera& self, float x, float y, float z) {
            self.setPosition(x, y, z);
        })
        .def("setFOV", [](Camera& self, float fov) {
            self.setFOV(fov);
        });

    // Обёртка для Сцены
    py::class_<Scene>(m, "Scene")
        .def(py::init([](int w, int h) {
            uint16_t* colorBuf = new uint16_t[w * h];
            uint16_t* depthBuf = new uint16_t[ZBUFFER_STRIDE(w) * h];
            auto scene = new Scene(colorBuf, depthBuf, w, h);
            scene->setClearBuffer(true);
            return scene;
        }))
        .def("setCamera", &Scene::setCamera)
        .def("render", &Scene::render)
        .def("get_framebuffer", [](Scene& self) {
            // Размеры берутся напрямую из глобального конфига Jet
            int w = JET_WIDTH;
            int h = JET_HEIGHT;
            int size = w * h * sizeof(uint16_t);
            
            // Если в Scene буфер кадра лежит в публичном поле colorBufferPtr или getBuffer()
            // Ниже используем прямую отдачу памяти сцены
            return py::bytes(reinterpret_cast<const char*>(&self), size); 
        });
}