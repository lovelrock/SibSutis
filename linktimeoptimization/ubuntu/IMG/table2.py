import matplotlib.pyplot as plt

# Данные
threads = [1, 2, 4, 8, 12]
lld = [0.45, 0.29, 0.25, 0.23, 0.21]
mold = [0.48, 0.32, 0.20, 0.15, 0.13]

plt.figure(figsize=(9, 5.5))

plt.plot(threads, lld, marker='o', linewidth=2, label='ld.lld')
plt.plot(threads, mold, marker='o', linewidth=2, label='mold')

plt.title('Зависимость времени линковки от количества потоков, Mysqlserver')
plt.xlabel('Количество потоков')
plt.ylabel('Время, с')
plt.xticks(threads)
plt.grid(True, axis='y', alpha=0.4)
plt.legend()

plt.tight_layout()
plt.savefig('mysql_threads_plot.png', dpi=300)
plt.show()
