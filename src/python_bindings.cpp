#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "Jet.hpp"
#include "PixelOps.hpp"

namespace py = pybind11;
using namespace Renderer;

namespace {

constexpr size_t kMaxMeshVertices =
    static_cast<size_t>(std::numeric_limits<uint16_t>::max()) + 1;

int32_t checked_round(double value, const char* what)
{
    if (!std::isfinite(value) ||
        value < static_cast<double>(std::numeric_limits<int32_t>::min()) ||
        value > static_cast<double>(std::numeric_limits<int32_t>::max())) {
        throw std::invalid_argument(std::string(what) + " is not a finite int32 value");
    }
    return static_cast<int32_t>(std::llround(value));
}

uint8_t checked_component(int value, const char* what)
{
    if (value < 0 || value > 255) {
        throw std::invalid_argument(std::string(what) + " must be in [0, 255]");
    }
    return static_cast<uint8_t>(value);
}

size_t checked_product(size_t a, size_t b, const char* what)
{
    if (a != 0 && b > std::numeric_limits<size_t>::max() / a) {
        throw py::value_error(std::string(what) + " is too large");
    }
    return a * b;
}

// Return the unit face normal in the engine's fixed-point convention.
// This mirrors Object::computeFlatNormals() but does not touch vertices.
Vector3 faceNormal(const Object& object, const Object::Triangle& tri)
{
    const Object::Vertex& a = object.vertices[tri.v1];
    const Object::Vertex& b = object.vertices[tri.v2];
    const Object::Vertex& c = object.vertices[tri.v3];

    // Use double for the cross product so very large coordinates cannot
    // overflow the int64 intermediate used by the native face-normal helper.
    const double ux = (double)b.position.x - a.position.x;
    const double uy = (double)b.position.y - a.position.y;
    const double uz = (double)b.position.z - a.position.z;
    const double vx = (double)c.position.x - a.position.x;
    const double vy = (double)c.position.y - a.position.y;
    const double vz = (double)c.position.z - a.position.z;

    const double nx = uy * vz - uz * vy;
    const double ny = uz * vx - ux * vz;
    const double nz = ux * vy - uy * vx;
    const double mag = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (!(mag > 0.0)) {
        return Vector3(0, 0, 0);
    }

    const double scale = (double)FIXED_POINT_SCALE / mag;
    return Vector3(static_cast<int32_t>(std::llround(nx * scale)),
                   static_cast<int32_t>(std::llround(ny * scale)),
                   static_cast<int32_t>(std::llround(nz * scale)));
}

// A Mesh may use shared vertices (the natural way to author a cube).  Simply
// writing a per-triangle normal into every referenced vertex makes the last
// triangle win, which silently corrupts FLAT/GOURAUD/PHONG lighting.  Split a
// vertex only when its incident triangles have different normals, keeping
// coplanar triangles and flat surfaces indexed and compact.
void rebuildFlatNormals(Object& object, std::vector<Vector3>& basePositions)
{
    if (basePositions.size() != object.vertices.size()) {
        throw std::runtime_error("Mesh vertex cache is inconsistent");
    }

    struct NormalSlot {
        Vector3 normal;
        uint16_t index;
    };

    std::vector<std::vector<NormalSlot>> slots(object.vertices.size());
    std::vector<Object::Vertex> newVertices;
    std::vector<Vector3> newBasePositions;
    newVertices.reserve(object.vertices.size());
    newBasePositions.reserve(object.vertices.size());

    for (Object::Triangle& tri : object.triangles) {
        const Vector3 normal = faceNormal(object, tri);
        const uint16_t source[3] = {tri.v1, tri.v2, tri.v3};
        uint16_t result[3] = {0, 0, 0};

        for (int corner = 0; corner < 3; ++corner) {
            const uint16_t original = source[corner];
            // size_t rather than uint16_t: 65535 is a valid vertex index and
            // must not double as a "not found" sentinel.
            size_t found = slots[original].size();
            for (size_t slotIndex = 0; slotIndex < slots[original].size();
                 ++slotIndex) {
                const NormalSlot& slot = slots[original][slotIndex];
                if (slot.normal.x == normal.x &&
                    slot.normal.y == normal.y &&
                    slot.normal.z == normal.z) {
                    found = slotIndex;
                    break;
                }
            }

            if (found == slots[original].size()) {
                if (newVertices.size() >= kMaxMeshVertices) {
                    throw std::invalid_argument(
                        "Mesh expands to more than 65536 vertices after "
                        "per-face normal splitting");
                }
                const uint16_t newIndex =
                    static_cast<uint16_t>(newVertices.size());
                Object::Vertex vertex = object.vertices[original];
                vertex.normal = normal;
                newVertices.push_back(vertex);
                newBasePositions.push_back(basePositions[original]);
                slots[original].push_back({normal, newIndex});
                result[corner] = newIndex;
            } else {
                result[corner] = slots[original][found].index;
            }
        }

        tri.v1 = result[0];
        tri.v2 = result[1];
        tri.v3 = result[2];
    }

    object.vertices = std::move(newVertices);
    basePositions = std::move(newBasePositions);
    object.invalidatePositions();
}

std::vector<Vector3> readNormals(
    const std::vector<std::array<float, 3>>& normals,
    size_t expected)
{
    if (normals.empty()) {
        return {};
    }
    if (normals.size() != expected) {
        throw std::invalid_argument(
            "normals must contain one normal per vertex");
    }

    std::vector<Vector3> result;
    result.reserve(normals.size());
    for (const auto& normal : normals) {
        const double x = normal[0];
        const double y = normal[1];
        const double z = normal[2];
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
            throw std::invalid_argument("vertex normals must be finite");
        }

        const double length = std::sqrt(x * x + y * y + z * z);
        if (!std::isfinite(length)) {
            throw std::invalid_argument("vertex normals are too large");
        }
        if (!(length > 1e-12)) {
            throw std::invalid_argument("vertex normals must be non-zero");
        }

        const double scale = (double)FIXED_POINT_SCALE / length;
        result.emplace_back(
            static_cast<int32_t>(std::llround(x * scale)),
            static_cast<int32_t>(std::llround(y * scale)),
            static_cast<int32_t>(std::llround(z * scale)));
    }
    return result;
}

// Inverse-transpose is the correct normal transform for a non-uniform scale.
// Custom normals are authored in the unscaled local frame, so re-apply it
// whenever setScale() changes the vertices.
void applyScaledCustomNormals(
    Object& object,
    const std::vector<Vector3>& baseNormals,
    float sx, float sy, float sz)
{
    if (baseNormals.size() != object.vertices.size()) {
        throw std::runtime_error("Mesh normal cache is inconsistent");
    }
    if (sx == 0.0f || sy == 0.0f || sz == 0.0f) {
        throw std::invalid_argument(
            "Mesh scale components must be non-zero when custom normals are used");
    }

    for (size_t i = 0; i < baseNormals.size(); ++i) {
        const Vector3& base = baseNormals[i];
        const double x = (double)base.x / sx;
        const double y = (double)base.y / sy;
        const double z = (double)base.z / sz;
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
            throw std::invalid_argument(
                "Mesh scale is too small for the authored normals");
        }
        const double length = std::sqrt(x * x + y * y + z * z);
        if (!std::isfinite(length)) {
            throw std::invalid_argument(
                "Mesh scale is too small for the authored normals");
        }
        if (!(length > 0.0)) {
            object.vertices[i].normal = base;
            continue;
        }
        const double scale = (double)FIXED_POINT_SCALE / length;
        object.vertices[i].normal.assign(
            static_cast<int32_t>(std::llround(x * scale)),
            static_cast<int32_t>(std::llround(y * scale)),
            static_cast<int32_t>(std::llround(z * scale)));
    }
}

// Owns the framebuffer, depth buffer and all C++ objects referenced by the
// Python Scene.  Buffers are value-initialised: get_framebuffer() must never
// expose uninitialised heap memory.
struct PySceneWrapper {
    int width;
    int height;
    std::vector<uint16_t> colorBuf;
    std::vector<uint16_t> depthBuf;
    std::vector<std::unique_ptr<Object>> ownedObjects;
    std::unique_ptr<Scene> scene;

    PySceneWrapper(int w, int h) : width(w), height(h)
    {
        if (w <= 0 || h <= 0) {
            throw py::value_error("Scene width and height must be positive");
        }
#if HALF_WIDTH_BUFFERS
        if ((w & 1) != 0) {
            throw py::value_error("Half-width framebuffer requires an even width");
        }
#endif
#if FIELD_BUFFERS
        if ((h & 1) != 0) {
            throw py::value_error("Field-buffered framebuffer requires an even height");
        }
#endif

        const size_t pixels = checked_product(
            static_cast<size_t>(w), static_cast<size_t>(h), "Scene dimensions");
        if (pixels > colorBuf.max_size() ||
            pixels > std::numeric_limits<size_t>::max() / sizeof(uint16_t)) {
            throw py::value_error("Scene colour buffer is too large");
        }

        const size_t zStride = static_cast<size_t>(ZBUFFER_STRIDE(w));
        const size_t zCells = checked_product(
            zStride, static_cast<size_t>(h), "Scene depth buffer");
        if (zCells > depthBuf.max_size()) {
            throw py::value_error("Scene depth buffer is too large");
        }

        colorBuf.assign(pixels, 0);
        depthBuf.assign(zCells, 0);
        scene = std::make_unique<Scene>(
            colorBuf.data(), depthBuf.data(), width, height);
        scene->setClearBuffer(true);
    }
};

struct PyMeshWrapper {
    Object object;
    std::vector<Vector3> basePositions;
    std::vector<Vector3> baseNormals;
    bool customNormals = false;

    PyMeshWrapper(
        const std::vector<std::array<float, 3>>& vertices,
        const std::vector<std::array<uint16_t, 3>>& triangles,
        const std::vector<std::array<float, 3>>& normals = {})
    {
        if (vertices.empty()) {
            throw std::invalid_argument("Mesh requires at least 3 vertices");
        }
        if (vertices.size() < 3) {
            throw std::invalid_argument("Mesh requires at least 3 vertices");
        }
        if (vertices.size() > kMaxMeshVertices) {
            throw std::invalid_argument(
                "Mesh supports at most 65536 vertices (uint16 indices)");
        }

        object.vertices.reserve(vertices.size());
        basePositions.reserve(vertices.size());
        for (const auto& v : vertices) {
            const int32_t x = checked_round(v[0], "vertex x");
            const int32_t y = checked_round(v[1], "vertex y");
            const int32_t z = checked_round(v[2], "vertex z");

            Object::Vertex vertex;
            vertex.position.assign(x, y, z);
            object.addVertex(vertex);
            basePositions.push_back(vertex.position);
        }

        if (triangles.empty()) {
            if (vertices.size() == 3) {
                object.addTriangle(0, 1, 2, nullptr);
            } else {
                for (size_t i = 1; i + 1 < vertices.size(); ++i) {
                    object.addTriangle(
                        0,
                        static_cast<uint16_t>(i),
                        static_cast<uint16_t>(i + 1),
                        nullptr);
                }
            }
        } else {
            for (const auto& tri : triangles) {
                if (tri[0] >= vertices.size() ||
                    tri[1] >= vertices.size() ||
                    tri[2] >= vertices.size()) {
                    throw std::out_of_range("Triangle index out of vertex range");
                }
                object.addTriangle(tri[0], tri[1], tri[2], nullptr);
            }
        }

        baseNormals = readNormals(normals, object.vertices.size());
        if (!baseNormals.empty()) {
            customNormals = true;
            for (size_t i = 0; i < object.vertices.size(); ++i) {
                object.vertices[i].normal = baseNormals[i];
            }
            object.invalidatePositions();
        } else {
            rebuildFlatNormals(object, basePositions);
        }
        object.calculateBoundingBox();
    }

    void setPosition(float x, float y, float z)
    {
        object.setPosition(
            checked_round(x, "position x"),
            checked_round(y, "position y"),
            checked_round(z, "position z"));
    }

    void setRotation(float x, float y, float z)
    {
        object.setRotation(
            checked_round(x, "rotation x"),
            checked_round(y, "rotation y"),
            checked_round(z, "rotation z"));
    }

    // The engine has no live per-object scale transform; scale is baked into
    // the local vertex positions. basePositions preserves the authored mesh so
    // repeated setScale() calls are absolute rather than cumulative.
    void setScale(float x, float y, float z)
    {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
            throw std::invalid_argument("Mesh scale must be finite");
        }
        if (basePositions.size() != object.vertices.size()) {
            throw std::runtime_error("Mesh vertex cache is inconsistent");
        }
        if (customNormals && (x == 0.0f || y == 0.0f || z == 0.0f)) {
            throw std::invalid_argument(
                "Mesh scale components must be non-zero when custom normals "
                "are used");
        }

        for (size_t i = 0; i < basePositions.size(); ++i) {
            const Vector3& base = basePositions[i];
            object.vertices[i].position.assign(
                checked_round(static_cast<double>(base.x) * x, "scaled vertex x"),
                checked_round(static_cast<double>(base.y) * y, "scaled vertex y"),
                checked_round(static_cast<double>(base.z) * z, "scaled vertex z"));
        }

        object.invalidatePositions();
        if (customNormals) {
            applyScaledCustomNormals(object, baseNormals, x, y, z);
        } else {
            // The mesh was already split per unique face normal on creation,
            // so re-evaluating the face normals preserves the same topology.
            object.computeFlatNormals();
        }
        object.calculateBoundingBox();
    }

    CullingMode cullingMode() const { return object.cullingMode; }
    void setCullingMode(CullingMode mode) { object.cullingMode = mode; }

    BlendMode blendMode() const { return object.blendMode; }
    void setBlendMode(BlendMode mode) { object.blendMode = mode; }
};

Object* add_owned_mesh(PySceneWrapper& self, const Object& source, Material& material)
{
    auto object = std::make_unique<Object>(source);
    for (auto& tri : object->triangles) {
        tri.material = &material;
        tri.colorBaked = false;
    }
    object->calculateBoundingBox();

    Object* raw = object.get();
    self.ownedObjects.push_back(std::move(object));
    self.scene->addObject(raw);
    return raw;
}

} // namespace

PYBIND11_MODULE(_pyjet, m)
{
    m.doc() = "Python bindings for the Jet 3D software rasteriser";
    m.attr("PIXEL_FORMAT") = "RGB565";

    m.def("framebuffer_to_rgb",
          [](py::bytes framebuffer, int width, int height) {
              if (width <= 0 || height <= 0) {
                  throw py::value_error("width and height must be positive");
              }
              const size_t pixels = checked_product(
                  static_cast<size_t>(width), static_cast<size_t>(height),
                  "framebuffer dimensions");
              const size_t expected = pixels * sizeof(uint16_t);
              if (static_cast<size_t>(PyBytes_GET_SIZE(framebuffer.ptr())) != expected) {
                  throw py::value_error(
                      "framebuffer size does not match width * height * 2");
              }
              std::vector<uint8_t> output(pixels * 3);
              const char* source = PyBytes_AS_STRING(framebuffer.ptr());
              {
                  py::gil_scoped_release release;
                  convertRGB565ToRGB888(
                      reinterpret_cast<const uint16_t*>(source),
                      output.data(), pixels);
              }
              return py::bytes(reinterpret_cast<const char*>(output.data()),
                               output.size());
          },
          py::arg("framebuffer"), py::arg("width"), py::arg("height"),
          R"pbdoc(
Convert a packed RGB565 framebuffer to RGB888 in native code.

The returned ``bytes`` object contains three bytes per pixel in R, G, B order.
This replaces the old Python-level per-frame conversion loop; the SIMD clear
and span helpers keep the hot render path independent of the Python GIL.
)pbdoc");

    m.def("framebuffer_to_rgba",
          [](py::bytes framebuffer, int width, int height) {
              if (width <= 0 || height <= 0) {
                  throw py::value_error("width and height must be positive");
              }
              const size_t pixels = checked_product(
                  static_cast<size_t>(width), static_cast<size_t>(height),
                  "framebuffer dimensions");
              const size_t expected = pixels * sizeof(uint16_t);
              if (static_cast<size_t>(PyBytes_GET_SIZE(framebuffer.ptr())) != expected) {
                  throw py::value_error(
                      "framebuffer size does not match width * height * 2");
              }
              std::vector<uint32_t> output(pixels);
              const char* source = PyBytes_AS_STRING(framebuffer.ptr());
              {
                  py::gil_scoped_release release;
                  convertRGB565ToRGBA8888(
                      reinterpret_cast<const uint16_t*>(source),
                      output.data(), pixels);
              }
              return py::bytes(reinterpret_cast<const char*>(output.data()),
                               output.size() * sizeof(uint32_t));
          },
          py::arg("framebuffer"), py::arg("width"), py::arg("height"),
          R"pbdoc(
Convert a packed RGB565 framebuffer to opaque RGBA8888 bytes.

Each pixel is returned as four bytes in R, G, B, 0xFF order, which is directly
usable by Pillow ``RGBA`` images and true RGBA surfaces.
)pbdoc");

    py::class_<Vector3>(m, "Vector3", "Engine-space integer vector (world units).")
        .def(py::init<int32_t, int32_t, int32_t>(),
             py::arg("x") = 0, py::arg("y") = 0, py::arg("z") = 0)
        .def_readwrite("x", &Vector3::x)
        .def_readwrite("y", &Vector3::y)
        .def_readwrite("z", &Vector3::z)
        .def("__repr__", [](const Vector3& v) {
            return "Vector3(" + std::to_string(v.x) + ", " +
                   std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
        });

    py::class_<Color>(m, "Color", "8-bit RGB colour used by lights.")
        .def(py::init([](int r, int g, int b) {
                 return Color{checked_component(r, "red"),
                              checked_component(g, "green"),
                              checked_component(b, "blue")};
             }),
             py::arg("r") = 255, py::arg("g") = 255, py::arg("b") = 255)
        .def_readwrite("r", &Color::r)
        .def_readwrite("g", &Color::g)
        .def_readwrite("b", &Color::b)
        .def("__repr__", [](const Color& c) {
            return "Color(" + std::to_string(c.r) + ", " +
                   std::to_string(c.g) + ", " + std::to_string(c.b) + ")";
        });

    py::enum_<ShadingMode>(m, "ShadingMode")
        .value("FLAT", ShadingMode::FLAT)
        .value("GOURAUD", ShadingMode::GOURAUD)
        .value("PHONG", ShadingMode::PHONG)
        .value("WIREFRAME", ShadingMode::WIREFRAME)
        .value("UNLIT", ShadingMode::UNLIT)
        .value("WATER_REFLECT", ShadingMode::WATER_REFLECT)
        .value("ADDITIVE", ShadingMode::ADDITIVE)
        .export_values();

    py::enum_<CullingMode>(m, "CullingMode")
        .value("CULL_BACKFACES", CullingMode::CULL_BACKFACES)
        .value("CULL_FRONTFACES", CullingMode::CULL_FRONTFACES)
        .value("NO_CULLING", CullingMode::NO_CULLING)
        .export_values();

    py::enum_<BlendMode>(m, "BlendMode")
        .value("BLEND_REPLACE", BlendMode::BLEND_REPLACE)
        .value("BLEND_ADD", BlendMode::BLEND_ADD)
        .value("BLEND_SUBTRACT", BlendMode::BLEND_SUBTRACT)
        .value("BLEND_SCALE", BlendMode::BLEND_SCALE)
        .value("BLEND_AVERAGE", BlendMode::BLEND_AVERAGE)
        .value("BLEND_XOR", BlendMode::BLEND_XOR)
        .export_values();

    py::class_<Material>(m, "Material")
        .def(py::init<uint16_t>(), py::arg("color") = 0xFFFF)
        .def(py::init([](uint16_t color, int alpha) {
                 return Material(color, checked_component(alpha, "alpha"));
             }),
             py::arg("color"), py::arg("alpha"))
        .def_readwrite("color", &Material::color)
        .def_readwrite("alpha", &Material::alpha)
        .def_readwrite("diffuse", &Material::diffuse)
        .def_readwrite("specular", &Material::specular)
        .def_readwrite("emissive", &Material::emissive)
        .def_readwrite("specularExponent", &Material::specularExponent)
        .def_readwrite("perspectiveCorrect", &Material::perspectiveCorrect)
        .def_readwrite("waterYBias", &Material::waterYBias)
        .def_readwrite("waterReflectionMaxY", &Material::waterReflectionMaxY)
        .def_readwrite("shadingMode", &Material::shadingMode);

    py::class_<Camera>(m, "Camera")
        .def(py::init<>())
        .def_property(
            "position",
            [](const Camera& self) {
                return std::make_tuple(self.position.x, self.position.y, self.position.z);
            },
            [](Camera& self, const std::tuple<int32_t, int32_t, int32_t>& p) {
                self.setPosition(std::get<0>(p), std::get<1>(p), std::get<2>(p));
            })
        .def_property(
            "rotation",
            [](const Camera& self) {
#if FLOAT_CAMERA_ANGLES
                constexpr float radToDeg = 57.29577951308232f;
                return std::make_tuple(self.rotation.x * radToDeg,
                                       self.rotation.y * radToDeg,
                                       self.rotation.z * radToDeg);
#else
                return std::make_tuple(static_cast<float>(self.rotation.x),
                                       static_cast<float>(self.rotation.y),
                                       static_cast<float>(self.rotation.z));
#endif
            },
            [](Camera& self, const std::tuple<float, float, float>& r) {
                self.setRotation(
                    checked_round(std::get<0>(r), "rotation x"),
                    checked_round(std::get<1>(r), "rotation y"),
                    checked_round(std::get<2>(r), "rotation z"));
            })
        .def_property_readonly("fov", [](const Camera& self) { return self.fov; })
        .def_readwrite("nearPlane", &Camera::nearPlane)
        .def_readwrite("farPlane", &Camera::farPlane)
        .def("setPosition", [](Camera& self, float x, float y, float z) {
            self.setPosition(
                checked_round(x, "camera x"),
                checked_round(y, "camera y"),
                checked_round(z, "camera z"));
        })
        .def("setRotation", [](Camera& self, float x, float y, float z) {
            self.setRotation(
                checked_round(x, "camera rotation x"),
                checked_round(y, "camera rotation y"),
                checked_round(z, "camera rotation z"));
        })
        .def("lookAt", [](Camera& self, float x, float y, float z) {
            self.lookAt(Vector3(
                checked_round(x, "look-at x"),
                checked_round(y, "look-at y"),
                checked_round(z, "look-at z")));
        })
        .def("setFOV", [](Camera& self, float fov, int screenWidth) {
            if (screenWidth <= 0) {
                throw py::value_error("screen_width must be positive");
            }
            if (!std::isfinite(fov) || fov <= 0.0f || fov >= 180.0f) {
                throw py::value_error("fov must be in (0, 180) degrees");
            }
            self.setFOV(fov, screenWidth);
        }, py::arg("fov"), py::arg("screen_width") = 320);

    py::class_<PyMeshWrapper>(m, "Mesh")
        .def(
            py::init<const std::vector<std::array<float, 3>>&,
                     const std::vector<std::array<uint16_t, 3>>&,
                     const std::vector<std::array<float, 3>>&>(),
            py::arg("vertices"),
            py::arg("indices") = std::vector<std::array<uint16_t, 3>>{},
            py::arg("normals") = std::vector<std::array<float, 3>>{},
            R"pbdoc(
Create mesh geometry from vertex positions, optional triangle indices and
optional per-vertex normals.

Vertices use a counter-clockwise winding order for front faces.  Use
``mesh.cullingMode = pyjet3d.CullingMode.NO_CULLING`` for double-sided meshes.

If ``normals`` is omitted, normals are computed per face.  Shared vertices
with different face normals are split automatically, so FLAT/GOURAUD/PHONG
lighting stays predictable for indexed meshes such as a cube.  Supplying one
normal per vertex enables smooth GOURAUD/PHONG shading.
)pbdoc")
        .def("setPosition", &PyMeshWrapper::setPosition,
             py::arg("x"), py::arg("y"), py::arg("z"))
        .def("setRotation", &PyMeshWrapper::setRotation,
             py::arg("x"), py::arg("y"), py::arg("z"))
        .def("setScale", &PyMeshWrapper::setScale,
             py::arg("x"), py::arg("y"), py::arg("z"),
             "Bake an absolute local-space scale into the mesh vertices.")
        .def_property("cullingMode",
                      &PyMeshWrapper::cullingMode,
                      &PyMeshWrapper::setCullingMode)
        .def_property("blendMode",
                      &PyMeshWrapper::blendMode,
                      &PyMeshWrapper::setBlendMode)
        .def_property(
            "ignoreZBuffer",
            [](const PyMeshWrapper& self) { return self.object.ignoreZBuffer; },
            [](PyMeshWrapper& self, bool value) { self.object.ignoreZBuffer = value; })
        .def_property(
            "noWriteZBuffer",
            [](const PyMeshWrapper& self) { return self.object.noWriteZBuffer; },
            [](PyMeshWrapper& self, bool value) { self.object.noWriteZBuffer = value; })
        .def_property(
            "enabled",
            [](const PyMeshWrapper& self) { return self.object.enabled; },
            [](PyMeshWrapper& self, bool value) { self.object.enabled = value; });

    py::class_<DirectionalLight>(m, "DirectionalLight")
        .def(py::init([](int azimuth, int elevation, int r, int g, int b, int intensity) {
                 if (intensity < 0 || intensity > 65535) {
                     throw py::value_error("intensity must be in [0, 65535]");
                 }
                 return DirectionalLight(
                     Vector3(azimuth, elevation, 0),
                     Color{checked_component(r, "red"),
                           checked_component(g, "green"),
                           checked_component(b, "blue")},
                     static_cast<uint16_t>(intensity));
             }),
             py::arg("azimuth") = 225,
             py::arg("elevation") = 45,
             py::arg("r") = 255,
             py::arg("g") = 255,
             py::arg("b") = 255,
             py::arg("intensity") = 255)
        .def_readwrite("direction", &DirectionalLight::direction)
        .def_readwrite("color", &DirectionalLight::color)
        .def_readwrite("intensity", &DirectionalLight::intensity)
        .def("updateDirection", [](DirectionalLight& self, int azimuth, int elevation) {
            self.updateDirection(Vector3(azimuth, elevation, 0));
        }, py::arg("azimuth"), py::arg("elevation"));

    py::class_<AmbientLight>(m, "AmbientLight")
        .def(py::init([](int r, int g, int b) {
                 return AmbientLight(Color{
                     checked_component(r, "red"),
                     checked_component(g, "green"),
                     checked_component(b, "blue")});
             }),
             py::arg("r") = 32, py::arg("g") = 32, py::arg("b") = 32)
        .def_readwrite("color", &AmbientLight::color);

    py::class_<PointLight>(m, "PointLight", R"pbdoc(
A world-space diffuse point light.

The built-in FLAT, GOURAUD and PHONG paths evaluate enabled point lights.
Intensity fades linearly from full brightness at ``rangeMin`` to zero at
``rangeMax``.  When ``rangeMax <= rangeMin`` (for example both set to zero)
the light has no distance attenuation.  Point lights add a range calculation
per shaded sample, so use a small number of them in software-rendered scenes.
)pbdoc")
        .def(py::init([](int x, int y, int z,
                         int r, int g, int b,
                         int rangeMin, int rangeMax) {
                 if (rangeMin < 0 || rangeMin > 65535 ||
                     rangeMax < 0 || rangeMax > 65535 ||
                     rangeMax < rangeMin) {
                     throw py::value_error("invalid point-light range");
                 }
                 return PointLight(
                     Vector3(x, y, z),
                     Color{checked_component(r, "red"),
                           checked_component(g, "green"),
                           checked_component(b, "blue")},
                     static_cast<uint16_t>(rangeMin),
                     static_cast<uint16_t>(rangeMax));
             }),
             py::arg("x"), py::arg("y"), py::arg("z"),
             py::arg("r") = 255, py::arg("g") = 255, py::arg("b") = 255,
             py::arg("rangeMin") = 0, py::arg("rangeMax") = 255)
        .def_readwrite("position", &PointLight::position)
        .def_readwrite("color", &PointLight::color)
        .def_readwrite("rangeMin", &PointLight::rangeMin)
        .def_readwrite("rangeMax", &PointLight::rangeMax)
        .def_readwrite("enabled", &PointLight::enabled);

    py::class_<PySceneWrapper>(m, "Scene")
        .def(py::init<int, int>(), py::arg("width") = 320, py::arg("height") = 240)
        .def_property_readonly("width", [](const PySceneWrapper& self) {
            return self.width;
        })
        .def_property_readonly("height", [](const PySceneWrapper& self) {
            return self.height;
        })
        .def_property_readonly("pixel_format", [](const PySceneWrapper&) {
            return std::string("RGB565");
        })
        .def_property_readonly("frame_counter", [](const PySceneWrapper& self) {
            return self.scene->frameCounter;
        })
        .def_property_readonly("last_frame_drawn_objects", [](const PySceneWrapper& self) {
            return self.scene->lastFrameDrawnObjects;
        })
        .def_property_readonly("last_frame_drawn_triangles", [](const PySceneWrapper& self) {
            return self.scene->lastFrameDrawnTriangles;
        })
        .def_property_readonly("last_frame_rasterized_triangles", [](const PySceneWrapper& self) {
            return self.scene->lastFrameRasterizedTriangles;
        })
        .def_property("thread_count",
            [](const PySceneWrapper& self) {
                return self.scene->getThreadCount();
            },
            [](PySceneWrapper& self, int count) {
                if (count < 0) {
                    throw py::value_error("thread_count must be >= 0");
                }
                self.scene->setThreadCount(count);
            },
            R"pbdoc(
Number of CPU participants used by the tile rasterizer (including the
calling thread). 0 selects the hardware default, 1 forces the classic serial
path, and values above 1 request a specific participant count. The count is
clamped by the engine to a safe maximum.
)pbdoc")
        .def_property("threaded_rendering",
            [](const PySceneWrapper& self) {
                return self.scene->isThreadedRendering();
            },
            [](PySceneWrapper& self, bool enabled) {
                self.scene->setThreadedRendering(enabled);
            },
            "Enable or disable the internal framebuffer tile pool.")
        .def("setThreadCount",
            [](PySceneWrapper& self, int count) {
                if (count < 0) {
                    throw py::value_error("count must be >= 0");
                }
                self.scene->setThreadCount(count);
            },
            py::arg("count"),
            "Set raster participants: 0 = auto, 1 = serial, >1 = explicit.")
        .def("setCamera",
             [](PySceneWrapper& self, Camera& cam) {
                 cam.setFOV(cam.fov, self.width);
                 self.scene->setCamera(&cam);
             },
             py::arg("camera"), py::keep_alive<1, 2>(),
             "Set the active camera. The scene keeps a reference to it.")
        .def("setDirectionalLight",
             [](PySceneWrapper& self, DirectionalLight& light) {
                 self.scene->setDirectionalLight(&light);
             },
             py::arg("light"), py::keep_alive<1, 2>(),
             "Set the active directional light. The scene keeps a reference.")
        .def("setAmbientLight",
             [](PySceneWrapper& self, AmbientLight& light) {
                 self.scene->setAmbientLight(&light);
             },
             py::arg("light"), py::keep_alive<1, 2>(),
             "Set the active ambient light. The scene keeps a reference.")
        .def("addPointLight",
             [](PySceneWrapper& self, PointLight& light) {
                 self.scene->addPointLight(&light);
             },
             py::arg("light"), py::keep_alive<1, 2>(),
             "Add a point light. The scene keeps a reference.")
        .def("setBackcolor",
             [](PySceneWrapper& self, uint16_t color) {
                 self.scene->setBackcolor(color);
             },
             py::arg("color"),
             "Set the RGB565 clear colour.")
        .def("setClearBuffer",
             [](PySceneWrapper& self, bool clear) {
                 self.scene->setClearBuffer(clear);
             },
             py::arg("clear"))
        .def("addMesh",
             [](PySceneWrapper& self, const PyMeshWrapper& mesh, Material& material) {
                 add_owned_mesh(self, mesh.object, material);
             },
             py::arg("mesh"), py::arg("material"),
             py::keep_alive<1, 3>(),
             "Add a Mesh instance to the scene with a material.")
        .def("addMesh",
             [](PySceneWrapper& self,
                const std::vector<std::array<float, 3>>& vertices,
                Material& material,
                const std::vector<std::array<uint16_t, 3>>& indices,
                const std::vector<std::array<float, 3>>& normals) {
                 PyMeshWrapper mesh(vertices, indices, normals);
                 add_owned_mesh(self, mesh.object, material);
             },
             py::arg("vertices"), py::arg("material"),
             py::arg("indices") = std::vector<std::array<uint16_t, 3>>{},
             py::arg("normals") = std::vector<std::array<float, 3>>{},
             py::keep_alive<1, 3>(),
             R"pbdoc(
Add a mesh directly from Python lists.

The triangle winding is counter-clockwise for front faces.  For example:

    scene.addMesh(
        [(0, 120, 0), (120, -120, 0), (-120, -120, 0)],
        material,
    )

Pass ``indices`` to share vertices and ``normals`` (one per vertex) for
smooth shading.  The scene keeps the material alive.
)pbdoc")
        .def("render",
             [](PySceneWrapper& self) {
                 Camera* camera = self.scene->getCamera();
                 if (camera == nullptr) {
                     throw std::runtime_error(
                         "Scene has no camera; call setCamera() before render()");
                 }
                 // The camera API has no back-reference to the Scene, so keep
                 // the projection tied to the actual output width even if the
                 // caller changes FOV after setCamera().
                 camera->setFOV(camera->fov, self.width);
                 {
                     // Rendering does not touch Python objects. Releasing the
                     // GIL lets a GUI thread stay responsive and lets the tile
                     // pool use every requested CPU participant.
                     py::gil_scoped_release release;
                     self.scene->render();
                 }
             })
        .def("get_framebuffer",
             [](const PySceneWrapper& self) {
                 const size_t bytes = self.colorBuf.size() * sizeof(uint16_t);
                 return py::bytes(
                     reinterpret_cast<const char*>(self.colorBuf.data()), bytes);
             },
             "Return width*height RGB565 pixels as little-endian bytes.")
        .def("get_framebuffer_rgb888",
             [](const PySceneWrapper& self) {
                 const size_t pixels = self.colorBuf.size();
                 std::vector<uint8_t> output(pixels * 3);
                 {
                     py::gil_scoped_release release;
                     convertRGB565ToRGB888(self.colorBuf.data(), output.data(), pixels);
                 }
                 return py::bytes(reinterpret_cast<const char*>(output.data()),
                                  output.size());
             },
             "Return the current frame as packed RGB888 bytes (3 BPP).")
        .def("get_framebuffer_rgba8888",
             [](const PySceneWrapper& self) {
                 const size_t pixels = self.colorBuf.size();
                 std::vector<uint32_t> output(pixels);
                 {
                     py::gil_scoped_release release;
                     convertRGB565ToRGBA8888(self.colorBuf.data(), output.data(), pixels);
                 }
                 return py::bytes(reinterpret_cast<const char*>(output.data()),
                                  output.size() * sizeof(uint32_t));
             },
             "Return the current frame as opaque RGBA8888 bytes (4 BPP).")
        .def("get_pixel",
             [](const PySceneWrapper& self, int x, int y) {
                 if (x < 0 || y < 0 || x >= self.width || y >= self.height) {
                     throw py::index_error("pixel coordinate out of range");
                 }
                 return self.colorBuf[
                     static_cast<size_t>(y) * static_cast<size_t>(self.width) +
                     static_cast<size_t>(x)];
             },
             py::arg("x"), py::arg("y"))
        .def("get_statistics",
             [](PySceneWrapper& self) {
                 int objects = 0;
                 int triangles = 0;
                 int vertices = 0;
                 self.scene->getStatistics(objects, triangles, vertices);
                 return std::make_tuple(objects, triangles, vertices);
             },
             "Return (object_count, triangle_count, vertex_count).");
}
