cmake_minimum_required(VERSION 3.15)
project(pyjet CXX C)

# --- Автоматический поиск папки с JetConfig.hpp ---
file(GLOB_RECURSE FOUND_JET_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/*JetConfig.hpp*")

set(JET_CONFIG_DIRS "")
foreach(CONFIG_PATH ${FOUND_JET_CONFIG})
    get_filename_component(CONFIG_DIR ${CONFIG_PATH} DIRECTORY)
    list(APPEND JET_CONFIG_DIRS ${CONFIG_DIR})
endforeach()

# --- Вариант 1: Сборка для ESP-IDF (микроконтроллеры / zest OS) ---
if(COMMAND idf_component_register)

    idf_component_register(
        SRC_DIRS "src"
        INCLUDE_DIRS "src"
        PRIV_INCLUDE_DIRS ${JET_CONFIG_DIRS}
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

    # Передаем src/ и абсолютно все найденные папки с конфигами
    target_include_directories(_pyjet PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${JET_CONFIG_DIRS}
    )
    
    target_compile_features(_pyjet PRIVATE cxx_std_17)

endif()