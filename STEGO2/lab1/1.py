import os
from PIL import Image

# Создаем папки

def convert_to_8bit_bmp(input_path, output_path):
    """Конвертирует JPEG/PNG в 8-bit grayscale BMP 512x512"""
    img = Image.open(input_path).convert('L')  # 'L' = grayscale (8-bit)
    img = img.resize((512, 512), Image.Resampling.LANCZOS)  # LANCZOS = наилучшее качество
    img.save(output_path, 'BMP')

# Пример: конвертировать все jpg из папки 'raw_medical'
for filename in os.listdir('../lab2'):
    if filename.lower().endswith(('.jpg', '.jpeg', '.png')):
        input_file = os.path.join('../lab2', filename)
        output_file = os.path.join('../lab2', filename.rsplit('.', 1)[0] + '.bmp')
        convert_to_8bit_bmp(input_file, output_file)
        print(f'✓ {filename} -> {output_file}')

# import kagglehub

# # Download latest version
# path = kagglehub.dataset_download("rahmasleam/flowers-dataset")

# print("Path to dataset files:", path)