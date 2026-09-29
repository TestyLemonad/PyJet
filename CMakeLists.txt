cmake_minimum_required(VERSION 3.15)
project(pyjet CXX C)

# --- Вариант 1: Сборка для ESP-IDF (микроконтроллеры / zest OS) ---
if(COMMAND idf_component_register)

    # Applications may keep JetConfig.hpp in a frontend-specific directory.
    # Allow an explicit override while retaining the original main/ layout.
    if(NOT DEFINED JET_CONFIG_INCLUDE_DIR)
        if(EXISTS "${PROJECT_DIR}/main/firmware/JetConfig.hpp")
            set(JET_CONFIG_INCLUDE_DIR "${PROJECT_DIR}/main/firmware")
        else()
            set(JET_CONFIG_INCLUDE_DIR "${PROJECT_DIR}/main")
        endif()
    endif()

    idf_component_register(
        SRC_DIRS "src"
        INCLUDE_DIRS "src"
        PRIV_INCLUDE_DIRS "${JET_CONFIG_INCLUDE_DIR}"
    )

    # Jet requires C++17
    target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)

# --- Вариант 2: Сборка PyJet для ПК через pybind11 ---
else()

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    # Находим pybind11, который передаёт scikit-build-core
    find_package(pybind11 CONFIG REQUIRED)

    # Собираем C++ модуль для Python
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
