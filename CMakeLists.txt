# Applications may keep JetConfig.hpp in a frontend-specific directory.
# Allow an explicit override while retaining the original main/ layout.
if(NOT DEFINED JET_CONFIG_INCLUDE_DIR)
    if(EXISTS "${PROJECT_DIR}/main/firmware/JetConfig.hpp")
        set(JET_CONFIG_INCLUDE_DIR "${PROJECT_DIR}/main/firmware")
    else()
        set(JET_CONFIG_INCLUDE_DIR "${PROJECT_DIR}/main")
    endif()
endif()

idf_component_register(SRC_DIRS "src"
                       INCLUDE_DIRS "src"
                       PRIV_INCLUDE_DIRS "${JET_CONFIG_INCLUDE_DIR}"
                       )

# Jet requires C++17
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)

# --- Python Bindings ---
option(BUILD_PYTHON_BINDINGS "Build Python bindings using pybind11" OFF)

if(BUILD_PYTHON_BINDINGS)
    find_package(pybind11 CONFIG REQUIRED)
    
    # Собираем C++ файл обертки вместе со всеми исходниками Jet
    pybind11_add_module(_pyjet
        src/python_bindings.cpp
        src/Scene.cpp
        src/Camera.cpp
        src/Object.cpp
        src/Primitives.cpp
        src/Rasteriser.cpp
        src/Math.cpp
    )
    
    target_include_directories(_pyjet PRIVATE src/)
    target_compile_features(_pyjet PRIVATE cxx_std_17)
endif()

