import matplotlib.pyplot as plt

# Данные
threads = [1, 2, 4, 8, 12]
lld = [0.8, 0.57, 0.33, 0.30, 0.30]
mold = [0.76, 0.29, 0.26, 0.20, 0.14]

plt.figure(figsize=(9, 5.5))

plt.plot(threads, lld, marker='o', linewidth=2, label='ld.lld')
plt.plot(threads, mold, marker='o', linewidth=2, label='mold')

plt.title('Зависимость времени линковки от количества потоков, LLVM-clang22')
plt.xlabel('Количество потоков')
plt.ylabel('Время, с')
plt.xticks(threads)
plt.grid(True, axis='y', alpha=0.4)
plt.legend()

plt.tight_layout()
plt.savefig('llvm_threads_plot.png', dpi=300)
plt.show()
