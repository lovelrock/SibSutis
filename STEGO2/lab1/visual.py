from PIL import Image
import glob
import os
import re

def create_composite_from_planes(pattern="plane_*.bmp", output_filename="composite.png"):
    """
    Создает составное изображение 3x3 (или меньше) из 8 битовых плоскостей.
    Файлы должны иметь имена вида: plane_*_k*.bmp
    """
    # Находим все файлы, соответствующие паттерну
    files = glob.glob(pattern)
    
    if not files:
        print(f"Файлы по паттерну '{pattern}' не найдены")
        return
    
    # Группируем по базовому имени (без _k*.bmp)
    groups = {}
    for f in files:
        # Извлекаем базовое имя (все до _k)
        base = re.sub(r'_k\d+\.bmp$', '', f)
        if base not in groups:
            groups[base] = []
        groups[base].append(f)
    
    # Для каждой группы создаем композит
    for base_name, group_files in groups.items():
        # Сортируем по номеру бита
        group_files.sort(key=lambda x: int(re.search(r'_k(\d+)\.bmp', x).group(1)))
        
        # Загружаем все изображения
        images = [Image.open(f) for f in group_files[:8]]  # Берем первые 8 (k=1..8)
        
        if not images:
            continue
            
        # Размер одного изображения
        w, h = images[0].size
        
        # Определяем сетку (3x3, центр пустой или заполняем черным)
        cols, rows = 4, 2
        composite = Image.new('L', (w * cols, h * rows), color=0)
        
        # Размещаем 8 плоскостей в сетке 3x3 (позиции 0-7, центр пустой)
        positions = [
            (0, 0),  # k1
            (1, 0),  # k2
            (2, 0),  # k3
            (3, 0),  # k4
            (0, 1),  # k5 (центр)
            (1, 1),  # k6
            (2, 1),  # k7
            (3, 1),  # k8
            # (2, 2) - пусто
        ]
        
        # Добавляем подписи (опционально)
        from PIL import ImageDraw, ImageFont
        draw = ImageDraw.Draw(composite)
        
        try:
            font = ImageFont.truetype("arial.ttf", 20)
        except:
            font = ImageFont.load_default()
        
        for idx, (img, (col, row)) in enumerate(zip(images[:8], positions[:8])):
            x = col * w
            y = row * h
            composite.paste(img, (x, y))
            
            # Добавляем подпись с номером бита
            draw.text((x + 5, y + 5), f"k={idx+1}", fill=255, font=font)
        
        # Добавляем центральную подпись
        draw.text((w + 10, h + 10), f"{base_name}", fill=255, font=font)
        
        # Сохраняем
        output_file = f"{os.path.basename(base_name)}_composite.png"
        composite.save(output_file)
        print(f"✓ Создан: {output_file} ({len(images)} плоскостей)")
        
        # Закрываем изображения
        for img in images:
            img.close()

def create_all_composites():
    create_composite_from_planes("visual\\plane_set1_img1_k*.bmp", "visual\\set1_img1_composite.png")
    create_composite_from_planes("visual\\plane_set1_img2_k*.bmp", "visual\\set1_img2_composite.png")
    create_composite_from_planes("visual\\plane_set1_img3_k*.bmp", "visual\\set1_img3_composite.png")
    create_composite_from_planes("visual\\plane_set1_img4_k*.bmp", "visual\\set1_img4_composite.png")
    create_composite_from_planes("visual\\plane_set1_img5_k*.bmp", "visual\\set1_img5_composite.png")
    create_composite_from_planes("visual\\plane_set2_img1_k*.bmp", "visual\\set2_img1_composite.png")
    create_composite_from_planes("visual\\plane_set2_img2_k*.bmp", "visual\\set2_img2_composite.png")
    create_composite_from_planes("visual\\plane_set2_img3_k*.bmp", "visual\\set2_img3_composite.png")
    create_composite_from_planes("visual\\plane_set2_img4_k*.bmp", "visual\\set2_img4_composite.png")
    create_composite_from_planes("visual\\plane_set2_img5_k*.bmp", "visual\\set2_img5_composite.png")
    create_composite_from_planes("visual\\plane_set3_img1_k*.bmp", "visual\\set3_img1_composite.png")
    create_composite_from_planes("visual\\plane_set3_img2_k*.bmp", "visual\\set3_img2_composite.png")
    create_composite_from_planes("visual\\plane_set3_img3_k*.bmp", "visual\\set3_img3_composite.png")
    create_composite_from_planes("visual\\plane_set3_img4_k*.bmp", "visual\\set3_img4_composite.png")
    create_composite_from_planes("visual\\plane_set3_img5_k*.bmp", "visual\\set3_img5_composite.png")

if __name__ == "__main__":
    print("\nСоздание композитов для всех групп:")
    create_all_composites()
    