import numpy as np
import matplotlib.pyplot as plt

projects = ["clang-22", "mysqld", "postgres", "tarantool"]
gnu_ld = [4.73, 3.22, 0.36, 0.38]
ld_lld = [0.38, 0.22, 0.10, 0.05]
mold = [0.15, 0.14, 0.10, 0.03]

x = np.arange(len(projects))
width = 0.22

fig, ax = plt.subplots(figsize=(10, 6))

ax.bar(x - width, gnu_ld, width, label="GNU ld")
ax.bar(x, ld_lld, width, label="ld.lld")
ax.bar(x + width, mold, width, label="mold")

ax.set_title("Время компоновки исполняемого файла при использовании компоновщиков GNU ld, ld.lld, mold")
ax.set_ylabel("Время, с")
ax.set_xticks(x)
ax.set_xticklabels(projects)
ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.08), ncol=3)

ax.grid(axis="y", alpha=0.4)

ax.set_ylim(0, 2)

plt.tight_layout()
plt.savefig("linker_comparison_chart_simple.png", dpi=300)
plt.show()
