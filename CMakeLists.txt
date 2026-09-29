cmake_minimum_required(VERSION 3.15)
project(pyjet CXX C)

# --- Вариант 1: Сборка для ESP-IDF (микроконтроллеры / zest OS) ---
if(COMMAND idf_component_register)

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

    # Авто-поиск директории с JetConfig.hpp
    file(GLOB_RECURSE ALL_CONFIG_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "JetConfig.hpp")
    set(EXTRA_INCLUDES "")
    foreach(CFG_FILE ${ALL_CONFIG_FILES})
        get_filename_component(CFG_DIR "${CFG_FILE}" DIRECTORY)
        list(APPEND EXTRA_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/${CFG_DIR}")
    endforeach()

    # Включаем src, найденные конфиги и стандартные папки на всякий случай
    target_include_directories(_pyjet PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/main
        ${CMAKE_CURRENT_SOURCE_DIR}/main/firmware
        ${CMAKE_CURRENT_SOURCE_DIR}/firmware
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${EXTRA_INCLUDES}
    )
    
    target_compile_features(_pyjet PRIVATE cxx_std_17)

endif()
