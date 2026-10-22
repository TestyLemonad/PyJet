cmake_minimum_required(VERSION 3.15)
project(pyjet CXX C)

# --- Option 1: ESP-IDF component (embedded Jet, no Python bindings) ---
if(COMMAND idf_component_register)

    idf_component_register(
        SRC_DIRS "src"
        EXCLUDE_SRCS
            "src/python_bindings.cpp"
            "src/Example.cpp"
            "src/Sample.cpp"
        INCLUDE_DIRS "src"
        PRIV_INCLUDE_DIRS "src/desktop"
    )

    target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)

# --- Option 2: PyJet Python extension for desktop ---
else()

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    find_package(pybind11 CONFIG REQUIRED)
    find_package(Threads REQUIRED)

    file(GLOB_RECURSE PYJET_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    )

    pybind11_add_module(_pyjet ${PYJET_SOURCES})

    # The Python frontend uses a full-resolution, lit, depth-buffered config.
    # It must come before src/ and src/desktop so quote-includes resolve to
    # the Python configuration rather than the embedded template.
    target_include_directories(_pyjet PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pyjet_config
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_compile_features(_pyjet PRIVATE cxx_std_17)
    target_link_libraries(_pyjet PRIVATE Threads::Threads)

    install(TARGETS _pyjet
        LIBRARY DESTINATION pyjet3d
        RUNTIME DESTINATION pyjet3d
        ARCHIVE DESTINATION pyjet3d
    )

endif()
