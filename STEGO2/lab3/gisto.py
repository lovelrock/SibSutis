import matplotlib.pyplot as plt
import numpy as np

# Данные
data = {
    'BOSS': {
        'Лаб1': {'PSNR': 51.59, 'Макс_ёмкость': 262144, 'Ср_ёмкость': 131043},
        'Лаб2': {'PSNR': 55.25, 'Макс_ёмкость': 139264, 'Ср_ёмкость': 69567},
        'Лаб3': {'PSNR': 51.99, 'Макс_ёмкость': 24234, 'Ср_ёмкость': 7089}
    },
    'Medical': {
        'Лаб1': {'PSNR': 51.68, 'Макс_ёмкость': 262144, 'Ср_ёмкость': 131072},
        'Лаб2': {'PSNR': 55.98, 'Макс_ёмкость': 139264, 'Ср_ёмкость': 69733},
        'Лаб3': {'PSNR': 50.54, 'Макс_ёмкость': 20543, 'Ср_ёмкость': 6054}
    },
    'Flowers': {
        'Лаб1': {'PSNR': 51.79, 'Макс_ёмкость': 262144, 'Ср_ёмкость': 131122},
        'Лаб2': {'PSNR': 55.41, 'Макс_ёмкость': 139264, 'Ср_ёмкость': 69632},
        'Лаб3': {'PSNR': 52.34, 'Макс_ёмкость': 24255, 'Ср_ёмкость': 7145}
    }
}

# Настройка стиля
plt.style.use('default')
fig, axes = plt.subplots(2, 2, figsize=(15, 12))
fig.suptitle('Сравнительный анализ по лабораторным работам', fontsize=16, fontweight='bold')

# Цвета для разных наборов данных
colors = ['#FF6B6B', '#4ECDC4', '#45B7D1']
datasets = list(data.keys())
labs = ['Лаб1', 'Лаб2', 'Лаб3']

# 1. График PSNR
ax1 = axes[0, 0]
x = np.arange(len(labs))
width = 0.25

for i, dataset in enumerate(datasets):
    psnr_values = [data[dataset][lab]['PSNR'] for lab in labs]
    bars = ax1.bar(x + i*width, psnr_values, width, label=dataset, color=colors[i], alpha=0.8)
    
    # Добавление значений на столбцы
    for bar in bars:
        height = bar.get_height()
        ax1.text(bar.get_x() + bar.get_width()/2., height,
                f'{height:.2f}', ha='center', va='bottom', fontsize=9)

ax1.set_xlabel('Лабораторные работы', fontsize=12)
ax1.set_ylabel('Средний PSNR (дБ)', fontsize=12)
ax1.set_title('Сравнение PSNR', fontsize=14, fontweight='bold')
ax1.set_xticks(x + width)
ax1.set_xticklabels(labs)
ax1.legend()
ax1.grid(True, alpha=0.3)

# 2. График максимальной ёмкости
ax2 = axes[0, 1]
for i, dataset in enumerate(datasets):
    max_capacity = [data[dataset][lab]['Макс_ёмкость'] for lab in labs]
    bars = ax2.bar(x + i*width, max_capacity, width, label=dataset, color=colors[i], alpha=0.8)
    
    for bar in bars:
        height = bar.get_height()
        ax2.text(bar.get_x() + bar.get_width()/2., height,
                f'{int(height)}', ha='center', va='bottom', fontsize=9, rotation=45)

ax2.set_xlabel('Лабораторные работы', fontsize=12)
ax2.set_ylabel('Максимальная ёмкость (бит)', fontsize=12)
ax2.set_title('Сравнение максимальной ёмкости', fontsize=14, fontweight='bold')
ax2.set_xticks(x + width)
ax2.set_xticklabels(labs)
ax2.legend()
ax2.grid(True, alpha=0.3)

# 3. График средней ёмкости
ax3 = axes[1, 0]
for i, dataset in enumerate(datasets):
    avg_capacity = [data[dataset][lab]['Ср_ёмкость'] for lab in labs]
    bars = ax3.bar(x + i*width, avg_capacity, width, label=dataset, color=colors[i], alpha=0.8)
    
    for bar in bars:
        height = bar.get_height()
        ax3.text(bar.get_x() + bar.get_width()/2., height,
                f'{int(height)}', ha='center', va='bottom', fontsize=9, rotation=45)

ax3.set_xlabel('Лабораторные работы', fontsize=12)
ax3.set_ylabel('Средняя ёмкость (бит)', fontsize=12)
ax3.set_title('Сравнение средней ёмкости', fontsize=14, fontweight='bold')
ax3.set_xticks(x + width)
ax3.set_xticklabels(labs)
ax3.legend()
ax3.grid(True, alpha=0.3)

# 4. Сводный график для одной лабораторной работы (Лаб3 как пример)
ax4 = axes[1, 1]
lab_of_interest = 'Лаб3'
metrics = ['PSNR', 'Макс. ёмкость\n(/1000)', 'Ср. ёмкость\n(/1000)']

# Нормализация данных для отображения на одном графике
x_swarm = np.arange(len(metrics))
width_swarm = 0.25

for i, dataset in enumerate(datasets):
    values = [
        data[dataset][lab_of_interest]['PSNR'],
        data[dataset][lab_of_interest]['Макс_ёмкость'] / 1000,
        data[dataset][lab_of_interest]['Ср_ёмкость'] / 1000
    ]
    bars = ax4.bar(x_swarm + i*width_swarm, values, width_swarm, 
                   label=dataset, color=colors[i], alpha=0.8)
    
    for bar in bars:
        height = bar.get_height()
        ax4.text(bar.get_x() + bar.get_width()/2., height,
                f'{height:.1f}', ha='center', va='bottom', fontsize=8)

ax4.set_xlabel('Параметры', fontsize=12)
ax4.set_ylabel('Значения (PSNR в дБ, ёмкость в тысячах бит)', fontsize=10)
ax4.set_title(f'Сравнение параметров для {lab_of_interest}', fontsize=14, fontweight='bold')
ax4.set_xticks(x_swarm + width_swarm)
ax4.set_xticklabels(metrics)
ax4.legend()
ax4.grid(True, alpha=0.3)

plt.tight_layout()
plt.show()

# Дополнительно: создание отдельных файлов с графиками
# Сохранение в высоком разрешении
fig.savefig('comparison_histograms.png', dpi=300, bbox_inches='tight')
print("Графики сохранены в файл 'comparison_histograms.png'")

# Создание отдельного графика только для PSNR
plt.figure(figsize=(10, 6))
x = np.arange(len(labs))
width = 0.25

for i, dataset in enumerate(datasets):
    psnr_values = [data[dataset][lab]['PSNR'] for lab in labs]
    plt.bar(x + i*width, psnr_values, width, label=dataset, color=colors[i], alpha=0.8)

plt.xlabel('Лабораторные работы', fontsize=12)
plt.ylabel('Средний PSNR (дБ)', fontsize=12)
plt.title('Сравнение PSNR по лабораторным работам', fontsize=14, fontweight='bold')
plt.xticks(x + width, labs)
plt.legend()
plt.grid(True, alpha=0.3)
plt.tight_layout()
plt.savefig('psnr_comparison.png', dpi=300, bbox_inches='tight')
plt.show()