<p align="center">
  <img src="Logo.png" alt="Логотип PyJet" width="512px">
</p>

# PyJet

[English](README.md) | **Русский**

[![PyPI](https://img.shields.io/pypi/v/pyjet3d.svg)](https://pypi.org/project/pyjet3d/)
[![Python](https://img.shields.io/badge/python-3.9%2B-blue.svg)](https://pypi.org/project/pyjet3d/)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

`pyjet3d` — компактный 3D-растеризатор для Python без внешних зависимостей.
Он рисует low-poly геометрию на CPU в RGB565-фреймбуфер, поэтому для сцены не
нужны GPU, OpenGL, игровой движок или библиотека для создания окон.

Это привязки Python к [CubeCoders/Jet](https://github.com/CubeCoders/Jet) —
фиксированному C++17-растеризатору, рассчитанному на ограниченные устройства
и вполне пригодному для обычного десктопа.

## Возможности

- Полностью программный рендеринг; во время работы нужна только стандартная
  библиотека Python.
- Полноразмерный RGB565-вывод с буфером глубины.
- Камера с позицией, поворотом, `lookAt()`, FOV и ближней/дальней плоскостями
  отсечения.
- Плоское затенение, Гуро, Фонг, каркасный и неосвещённый режимы.
- Направленный и окружающий свет; объекты точечного света для
  пользовательского/нативного затенения.
- Треугольные меши, собираемые прямо из списков Python.
- Настройки отсечения граней, смешивания и буфера глубины для каждого меша.
- Чтение фреймбуфера как `bytes` и преобразование в RGB888.
- Колёса для Linux, Windows и macOS через `cibuildwheel`.

## Установка

```bash
python -m pip install pyjet3d
```

Требуется Python 3.9 или новее. Дополнительные пакеты во время работы не
нужны.

> **Переход на 0.3.0:** импорт переименован с `pyjet` на `pyjet3d`, чтобы
> избежать конфликта с посторонним проектом `pyjet` на PyPI. Замените
> `import pyjet` на `import pyjet3d`.

## Быстрый старт

```python
import pyjet3d

scene = pyjet3d.Scene(320, 240)

camera = pyjet3d.Camera()
camera.setPosition(0, 0, -500)
camera.setFOV(70, scene.width)   # FOV в градусах; ширина задаёт масштаб проекции
camera.nearPlane = 32
camera.farPlane = 4096
scene.setCamera(camera)

material = pyjet3d.Material(0xF800)  # ярко-красный в RGB565
material.shadingMode = pyjet3d.ShadingMode.FLAT

# Обход против часовой стрелки: лицевая грань при CULL_BACKFACES по умолчанию.
scene.addMesh(
    [(0, 120, 0), (120, -120, 0), (-120, -120, 0)],
    material,
)
scene.render()

# Во фреймбуфере width * height little-endian значений RGB565.
framebuffer = scene.get_framebuffer()
assert len(framebuffer) == scene.width * scene.height * 2

# Преобразуем в RGB888 и сохраняем PPM без сторонних библиотек.
rgb = pyjet3d.framebuffer_to_rgb(framebuffer, scene.width, scene.height)
with open("triangle.ppm", "wb") as out:
    out.write(b"P6\n%d %d\n255\n" % (scene.width, scene.height))
    out.write(rgb)
```

Это весь цикл: создать сцену, добавить геометрию, отрисовать и забрать
пиксели. Скрытого окна или цикла событий нет. Для вывода через Pillow:

```python
from PIL import Image

rgb = pyjet3d.framebuffer_to_rgb(
    scene.get_framebuffer(), scene.width, scene.height
)
Image.frombytes("RGB", (scene.width, scene.height), rgb).save("triangle.png")
```

## Демки на Python

В репозитории есть готовые демки без внешних зависимостей:

| Демка | Описание |
| --- | --- |
| [`examples/python/triangle.py`](examples/python/triangle.py) | Минимальный рендер; сохраняет `triangle.ppm`. |
| [`examples/python/lit_cube.py`](examples/python/lit_cube.py) | Куб с проверкой глубины, камерой, окружающим и направленным светом. |
| [`examples/python/orbit.py`](examples/python/orbit.py) | Последовательность кадров с вращающимся кубом для экспериментов с анимацией. |

```bash
python examples/python/triangle.py
python examples/python/lit_cube.py --output lit_cube.ppm
python examples/python/orbit.py --frames 24 --output-dir orbit_frames
```

Параметры описаны в [`examples/python/README_RU.md`](examples/python/README_RU.md),
английская версия — в [`examples/python/README.md`](examples/python/README.md).

## Документация

- [Руководство по Python API](docs/Python_RU.md) — полная русская версия.
- [Python API guide](docs/Python.md) — полная английская версия.
- [История изменений](CHANGELOG.md)
- [Справочник API движка](https://cubecoders.github.io/Jet/)
- Заметки о нативном движке: [производительность ESP32](docs/ESP32Performance.md),
  [инстансинг](docs/instancing.md), [сплошные прямоугольники](docs/SolidRectangles.md)
  и [тайловые текстуры](docs/TiledTextures.md).

## Что доступно из Python

`pyjet3d` — намеренно сфокусированная обёртка:

- **Scene** — размеры, камера, свет, добавление мешей, цвет фона, очистка,
  чтение фреймбуфера, доступ к пикселям и статистика кадров.
- **Camera** — позиция, поворот Эйлера, `lookAt()`, FOV и плоскости отсечения.
- **Mesh** — вершины, индексы треугольников, преобразования, отсечение,
  смешивание и флаги глубины. Геометрия копируется в сцену при `addMesh()`.
- **Material** — цвет RGB565, альфа, diffuse/specular/emissive и все режимы
  затенения.
- **Свет** — направленное и окружающее освещение; объекты точечного света
  для пользовательского/нативного затенения.
- **Вспомогательные типы** — `framebuffer_to_rgb()`, а также `Vector3` и
  `Color`.

Текстуры, пользовательские шейдеры, спрайты, частицы, постобработка, picking
и нативные помощники для примитивов/OBJ есть в C++-движке, но не входят в
текущий Python API.

## Как это работает

Jet — растеризатор с фиксированным конвейером. Он преобразует вершины
целочисленной арифметикой с фиксированной точкой, отсекает треугольники, растеризует их
с необязательным буфером глубины и затеняет по данным материала и света.
Сборка Python настроена на полноразмерный десктопный вывод с буфером глубины
и освещением; встраиваемая C++-сборка может быть урезана под ESP32 и похожие
платформы.

Движок умеет гораздо больше, чем показывает Python-обёртка: текстуры,
инстансинг, спрайты, частицы, экранные эффекты и пользовательские шейдеры.
См. [нативную документацию](https://cubecoders.github.io/Jet/) и репозиторий
[JetExamples](https://github.com/CubeCoders/JetExamples).

## Разработка

Запуск тестов Python из корня репозитория:

```bash
python -m pytest
```

Локальная сборка колеса:

```bash
python -m pip install build
python -m build --wheel
```

Версия релиза хранится в [`version.txt`](version.txt) одной строкой, например
`v0.3.0`. Пуш, меняющий этот файл в `main`, запускает релизный workflow:
собираются колёса для Linux, Windows и macOS, создаётся или обновляется
соответствующий GitHub-релиз с прикреплёнными колёсами, а затем они
публикуются на PyPI. Подробности — в
[`.github/workflows/wheels.yml`](.github/workflows/wheels.yml).

## Сообщество и лицензия

Jet развивает [CubeCoders](https://cubecoders.com). Присоединяйтесь к
[Discord-серверу](https://discord.gg/FSdJYDTEYt), чтобы делиться проектами и
получать помощь.

PyJet распространяется по лицензии MIT. Полный текст — в [`LICENSE`](LICENSE).
Проект можно использовать в открытых и закрытых проектах, включая
коммерческие, при сохранении уведомлений об авторских правах и разрешении.

Мы рады вкладу в проект; сначала прочитайте [`CONTRIBUTING.md`](CONTRIBUTING.md).
