import numpy as np
from PIL import Image

def upscale_image(input_path, output_path):
    img = Image.open(input_path)
    img_resized = img.resize((512, 512), Image.Resampling.LANCZOS)
    img_resized.save(output_path)

if __name__ == "__main__":
    upscale_image("watermark3.bmp", "watermark6.bmp")
