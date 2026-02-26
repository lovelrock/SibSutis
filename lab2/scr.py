import os
from PIL import Image

DIR = "."

SUPPORTED_EXTENSIONS = (".png", ".jpg", ".jpeg", ".tiff", ".bmp")

for filename in os.listdir(DIR):
    if not filename.lower().endswith(SUPPORTED_EXTENSIONS):
        continue

    path = os.path.join(DIR, filename)
    name, _ = os.path.splitext(filename)
    new_path = os.path.join(DIR, f"{name}.bmp")

    try:
        with Image.open(path) as img:
            img_gray = img.convert("L")
            img_gray.save(new_path, format="BMP")

        if path != new_path:
            os.remove(path)

        print(f"OK: {filename} → {name}.bmp")
    except Exception as e:
        print(f"ERROR: {filename} — {e}")
