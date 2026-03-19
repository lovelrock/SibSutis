import pandas as pd
import matplotlib.pyplot as plt
import glob
import os

plt.style.use('ggplot')
plt.rcParams['figure.figsize'] = (12, 5)

for orig_file in glob.glob('hist_*_original.csv'):
    base = orig_file.replace('_original.csv', '')
    stego_file = f"{base}_stego.csv"
    
    if not os.path.exists(stego_file):
        continue
    
    orig = pd.read_csv(orig_file)
    stego = pd.read_csv(stego_file)
    
    fig, (ax1, ax2) = plt.subplots(1, 2, sharey=True)
    
    ax1.bar(orig['Brightness'], orig['Count'], width=1.0, color='steelblue', alpha=0.8)
    ax1.set_title(f'Исходное: {base}', fontsize=11)
    ax1.set_xlabel('Яркость')
    ax1.set_ylabel('Частота')
    ax1.grid(True, alpha=0.3)
    
    ax2.bar(stego['Brightness'], stego['Count'], width=1.0, color='crimson', alpha=0.8)
    ax2.set_title(f'Стего (k=1): {base}', fontsize=11)
    ax2.set_xlabel('Яркость')
    ax2.grid(True, alpha=0.3)
    
    diff = orig['Count'] - stego['Count']
    max_diff = abs(diff).max()
    
    plt.suptitle(f'Сравнение гистограмм: {base} (макс. различие = {max_diff})', 
                 fontsize=12, fontweight='bold')
    plt.tight_layout()
    plt.savefig(f'hist_compare_{base}.png', dpi=150, bbox_inches='tight')
    plt.close()
    
    print(f'Сохранено: hist_compare_{base}.png')

plt.figure(figsize=(14, 6))
boss_files = glob.glob('hist_set1_*_original.csv')

for i, file in enumerate(boss_files[:3]):  
    base = file.replace('_original.csv', '')
    orig = pd.read_csv(file)
    stego = pd.read_csv(f"{base}_stego.csv")
    
    plt.subplot(2, 3, i+1)
    plt.bar(orig['Brightness'], orig['Count'], width=1.0, alpha=0.7, label='Исходное')
    plt.title(f'Set1 {i+1}')
    plt.xlabel('Яркость')
    plt.grid(True, alpha=0.2)
    
    plt.subplot(2, 3, i+4)
    plt.bar(stego['Brightness'], stego['Count'], width=1.0, color='crimson', alpha=0.7, label='Стего')
    plt.title(f'Set1 {i+1} (k=1)')
    plt.xlabel('Яркость')
    plt.grid(True, alpha=0.2)

plt.suptitle('Сравнение гистограмм: Set1', fontsize=14, fontweight='bold')
plt.tight_layout()
plt.savefig('hist_summary_Set1.png', dpi=150)
print('Сохранено: hist_summary_Set1.png')