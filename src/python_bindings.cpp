#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>
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
    std::vector<std::unique_ptr<Object>> ownedObjects;

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

struct PyMeshWrapper {
    Object object;

    PyMeshWrapper(
        const std::vector<std::array<float, 3>>& vertices,
        const std::vector<std::array<uint16_t, 3>>& triangles
    ) {
        if (vertices.size() < 3) {
            throw std::invalid_argument("Mesh requires at least 3 vertices");
        }

        object.vertices.reserve(vertices.size());
        for (const auto& v : vertices) {
            Object::Vertex vertex;
            vertex.position.assign(
                static_cast<int32_t>(std::lround(v[0])),
                static_cast<int32_t>(std::lround(v[1])),
                static_cast<int32_t>(std::lround(v[2]))
            );
            object.addVertex(vertex);
        }

        if (triangles.empty()) {
            if (vertices.size() == 3) {
                object.addTriangle(0, 1, 2, nullptr);
            } else {
                for (size_t i = 1; i + 1 < vertices.size(); ++i) {
                    object.addTriangle(0, static_cast<uint16_t>(i), static_cast<uint16_t>(i + 1), nullptr);
                }
            }
        } else {
            for (const auto& tri : triangles) {
                if (tri[0] >= vertices.size() || tri[1] >= vertices.size() || tri[2] >= vertices.size()) {
                    throw std::out_of_range("Triangle index out of vertex range");
                }
                object.addTriangle(tri[0], tri[1], tri[2], nullptr);
            }
        }

        object.computeFlatNormals();
        object.calculateBoundingBox();
    }

    void setPosition(float x, float y, float z) {
        object.setPosition(
            static_cast<int32_t>(std::lround(x)),
            static_cast<int32_t>(std::lround(y)),
            static_cast<int32_t>(std::lround(z))
        );
    }

    void setRotation(float x, float y, float z) {
        object.setRotation(
            static_cast<int32_t>(std::lround(x)),
            static_cast<int32_t>(std::lround(y)),
            static_cast<int32_t>(std::lround(z))
        );
    }

    void setScale(float x, float y, float z) {
        object.scale.assign(
            static_cast<int32_t>(std::lround(x * FIXED_POINT_SCALE)),
            static_cast<int32_t>(std::lround(y * FIXED_POINT_SCALE)),
            static_cast<int32_t>(std::lround(z * FIXED_POINT_SCALE))
        );
        object.transformScale = true;
    }
};

static Object* add_owned_mesh(PySceneWrapper& self, const Object& source, Material& material) {
    auto object = std::make_unique<Object>(source);
    for (auto& tri : object->triangles) {
        tri.material = &material;
    }
    object->calculateBoundingBox();
    Object* raw = object.get();
    self.scene->addObject(raw);
    self.ownedObjects.push_back(std::move(object));
    return raw;
}

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
            self.setFOV(fov, 320);
        });

    py::class_<PyMeshWrapper>(m, "Mesh")
        .def(
            py::init<const std::vector<std::array<float, 3>>&,
                     const std::vector<std::array<uint16_t, 3>>&>(),
            py::arg("vertices"),
            py::arg("indices") = std::vector<std::array<uint16_t, 3>>{},
            "Create mesh geometry from vertex positions and optional triangle indices."
        )
        .def("setPosition", &PyMeshWrapper::setPosition, py::arg("x"), py::arg("y"), py::arg("z"))
        .def("setRotation", &PyMeshWrapper::setRotation, py::arg("x"), py::arg("y"), py::arg("z"))
        .def("setScale", &PyMeshWrapper::setScale, py::arg("x"), py::arg("y"), py::arg("z"));

    py::class_<PySceneWrapper>(m, "Scene")
        .def(py::init<int, int>(), py::arg("width") = 320, py::arg("height") = 240)
        .def("setCamera", [](PySceneWrapper& self, Camera& cam) {
            self.scene->setCamera(&cam);
        })
        .def(
            "addMesh",
            [](PySceneWrapper& self, const PyMeshWrapper& mesh, Material& material) {
                add_owned_mesh(self, mesh.object, material);
            },
            py::arg("mesh"),
            py::arg("material"),
            "Add a Mesh instance to the scene with a material."
        )
        .def(
            "addMesh",
            [](PySceneWrapper& self,
               const std::vector<std::array<float, 3>>& vertices,
               Material& material,
               const std::vector<std::array<uint16_t, 3>>& indices) {
                PyMeshWrapper mesh(vertices, indices);
                add_owned_mesh(self, mesh.object, material);
            },
            py::arg("vertices"),
            py::arg("material"),
            py::arg("indices") = std::vector<std::array<uint16_t, 3>>{},
            R"pbdoc(
Add a mesh directly from Python lists.

Example:
    >>> import pyjet
    >>> scene = pyjet.Scene(320, 240)
    >>> cam = pyjet.Camera()
    >>> cam.setPosition(0, 0, -500)
    >>> cam.setFOV(70)
    >>> scene.setCamera(cam)
    >>> mat = pyjet.Material(0xF800)
    >>> mat.shadingMode = pyjet.ShadingMode.FLAT
    >>> scene.addMesh([(0, 120, 0), (-120, -120, 0), (120, -120, 0)], mat)
    >>> scene.render()
)pbdoc"
        )
        .def("render", [](PySceneWrapper& self) {
            self.scene->render();
        })
        .def("get_framebuffer", [](PySceneWrapper& self) {
            size_t size = static_cast<size_t>(self.width) * self.height * sizeof(uint16_t);
            return py::bytes(reinterpret_cast<const char*>(self.colorBuf), size);
        });
}
