cmake_minimum_required(VERSION 3.15)
project(pyjet CXX C)

# --- Поиск директории с JetConfig.hpp ---
file(GLOB_RECURSE JET_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/*/JetConfig.hpp" "${CMAKE_CURRENT_SOURCE_DIR}/JetConfig.hpp")

if(JET_CONFIG_FILE)
    list(GET JET_CONFIG_FILE 0 FIRST_JET_CONFIG)
    get_filename_component(JET_CONFIG_INCLUDE_DIR "${FIRST_JET_CONFIG}" DIRECTORY)
else()
    set(JET_CONFIG_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()

# --- Вариант 1: Сборка для ESP-IDF (микроконтроллеры / zest OS) ---
if(COMMAND idf_component_register)

    idf_component_register(
        SRC_DIRS "src"
        INCLUDE_DIRS "src"
        PRIV_INCLUDE_DIRS "${JET_CONFIG_INCLUDE_DIR}"
    )

    target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)

# --- Вариант 2: Сборка PyJet для ПК через pybind11 ---
else()

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    find_package(pybind11 CONFIG REQUIRED)

    # Автоматически находим ВСЕ C++ исходники
    file(GLOB_RECURSE PYJET_SOURCES 
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    )

    # Собираем C++ модуль для Python
    pybind11_add_module(_pyjet ${PYJET_SOURCES})

    # Передаем и src/, и автоматическую папку с JetConfig.hpp
    target_include_directories(_pyjet PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${JET_CONFIG_INCLUDE_DIR}
    )
    
    target_compile_features(_pyjet PRIVATE cxx_std_17)

endif()
