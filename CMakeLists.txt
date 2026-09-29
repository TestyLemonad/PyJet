cmake_minimum_required(VERSION 3.15)
project(pyjet CXX C)

# Определяем путь к JetConfig.hpp для обеих платформ
if(NOT DEFINED JET_CONFIG_INCLUDE_DIR)
    if(EXISTS "${PROJECT_DIR}/main/firmware/JetConfig.hpp")
        set(JET_CONFIG_INCLUDE_DIR "${PROJECT_DIR}/main/firmware")
    elseif(EXISTS "${PROJECT_SOURCE_DIR}/main/firmware/JetConfig.hpp")
        set(JET_CONFIG_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/main/firmware")
    elseif(EXISTS "${PROJECT_SOURCE_DIR}/main/JetConfig.hpp")
        set(JET_CONFIG_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/main")
    else()
        set(JET_CONFIG_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/main")
    endif()
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

    # Добавляем и src/, и папку с JetConfig.hpp в инклюды!
    target_include_directories(_pyjet PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${JET_CONFIG_INCLUDE_DIR}
    )
    
    target_compile_features(_pyjet PRIVATE cxx_std_17)

endif()
