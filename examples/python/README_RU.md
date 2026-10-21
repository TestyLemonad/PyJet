# Демки на Python

[English](README.md) | **Русский**

Эти скрипты рендерят через `pyjet3d` и сохраняют изображения в бинарном
формате PPM, не требуя сторонних библиотек. Используются только пакет и
стандартная библиотека Python.

## Требования

```bash
python -m pip install pyjet3d
```

Демки можно запускать из корня репозитория или из этого каталога. Из корня
репозитория:

```bash
python examples/python/triangle.py
python examples/python/lit_cube.py --output lit_cube.ppm
python examples/python/lit_cube.py --smooth --output lit_cube_smooth.ppm
python examples/python/orbit.py --frames 24 --output-dir orbit_frames
```

## Демки

### `triangle.py`

Самый маленький полный рендер: один лицевой треугольник, один материал, один
кадр и файл PPM.

```bash
python examples/python/triangle.py --width 320 --height 240 --output triangle.ppm
```

### `lit_cube.py`

Создаёт куб с проверкой глубины, камерой, тёплым направленным светом,
окружающей подсветкой и точечным светом в мировом пространстве. По умолчанию
куб повёрнут на 35 градусов; угол задаётся через `--angle`. Нормали по
умолчанию строятся по граням. Флаг `--smooth` передаёт собственные нормали
вершин: с PHONG куб выглядит округлым и показывает необязательный аргумент
`normals` у меша.

```bash
python examples/python/lit_cube.py --width 480 --height 320 --angle 35 --output lit_cube.ppm
python examples/python/lit_cube.py --smooth --output lit_cube_smooth.ppm
```

### `orbit.py`

Рендерит короткую последовательность пронумерованных PPM-кадров с вращением
куба вокруг оси Y и тем же набором из трёх источников света. Сцена
пересобирается на каждом кадре, потому что `Scene.addMesh()` копирует
геометрию меша. Флаг `--smooth` включает собственные нормали вершин.

```bash
python examples/python/orbit.py --frames 24 --width 320 --height 240 --output-dir orbit_frames
python examples/python/orbit.py --smooth --frames 24 --output-dir orbit_smooth
```

Преобразование последовательности в GIF или видео через `ffmpeg`:

```bash
ffmpeg -framerate 24 -i orbit_frames/frame_%03d.ppm orbit.gif
```

## Просмотр PPM

Большинство просмотрщиков изображений не открывают PPM. Преобразуйте файл
через Pillow:

```python
from PIL import Image

Image.open("lit_cube.ppm").save("lit_cube.png")
```

или через ImageMagick:

```bash
magick lit_cube.ppm lit_cube.png
```

## Файлы

- `demo_utils.py` — запись PPM, помощник RGB565, настройка камеры, общий набор
  направленного/окружающего/точечного света и геометрия куба (плоская или
  гладкая).
- `triangle.py` — минимальный пример.
- `lit_cube.py` — освещение и преобразования, с необязательными нормалями.
- `orbit.py` — цикл последовательности кадров с тем же освещением.

Демки намеренно обходятся без окна и цикла событий. Настоящее приложение может
отправить фреймбуфер на дисплей, в текстуру, видеоэнкодер или веб-эндпоинт
после `scene.render()`.
