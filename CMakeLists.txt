cmake_minimum_required(VERSION 3.15)
project(pyjet CXX C)

# --- Вариант 1: Сборка для ESP-IDF (микроконтроллеры / zest OS) ---
if(COMMAND idf_component_register)

    idf_component_register(
        SRC_DIRS "src"
        INCLUDE_DIRS "src"
    )

    target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)

# --- Вариант 2: Сборка PyJet для ПК через pybind11 ---
else()

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    find_package(pybind11 CONFIG REQUIRED)

    # Ищем все C++ файлы
    file(GLOB_RECURSE PYJET_SOURCES 
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    )

    # Собираем C++ модуль для Python
    pybind11_add_module(_pyjet ${PYJET_SOURCES})

    # Пути к заголовочным файлам
    target_include_directories(_pyjet PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/desktop
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    
    target_compile_features(_pyjet PRIVATE cxx_std_17)

endif()
