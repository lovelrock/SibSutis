import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import numpy as np

projects = ["clang-22", "mysqld", "postgres", "tarantool"]
gnu_ld = [4.73, 3.22, 0.36, 0.38]
ld_lld = [0.38, 0.22, 0.10, 0.05]
mold = [0.15, 0.14, 0.10, 0.03]

data = pd.DataFrame({"GNU ld": gnu_ld, "ld.lld": ld_lld, "mold": mold}, index=projects)

# ПРАВИЛЬНО: делим время GNU ld на время других (во сколько раз БЫСТРЕЕ)
speedup = data.div(data["GNU ld"], axis=0)  # Это уже правильно!
# Но нужно инвертировать, чтобы показать ускорение
speedup = 1 / speedup  # <--- ВОТ ЭТА СТРОКА ВАЖНА!

# Для наглядности: установим 1.0 для GNU ld
for col in speedup.columns:
    speedup[col] = speedup[col].round(1)

plt.figure(figsize=(8, 6))
sns.heatmap(speedup, annot=True, fmt='.1f', cmap='RdYlGn_r', 
            cbar_kws={'label': 'Ускорение относительно GNU ld (x раз)'},
            vmin=1, vmax=35, center=15)  # vmin=1, чтобы 1 был нейтральным
plt.title("Коэффициент ускорения компоновки относительно компоновщика GNU ld")
plt.ylabel("Проект")
plt.xlabel("Компоновщик")
plt.tight_layout()
plt.show()

# Вывод для проверки
print("\nРеальные значения ускорения:")
print(speedup)
