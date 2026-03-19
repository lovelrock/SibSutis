import pandas as pd
import matplotlib.pyplot as plt


def plot_roc_from_csv(csv_file, title, output_file):
    df = pd.read_csv(csv_file, sep=None, engine="python")

    plt.figure(figsize=(8, 6))

    markers = {
        "BOSSbase": "o",
        "Medical": "s",
        "Other": "^"
    }

    for method in df["Method"].unique():
        part = df[df["Method"] == method].copy()
        part = part.sort_values("FPR")

        plt.plot(
            part["FPR"],
            part["TPR"],
            linewidth=2,
            marker="o",
            label=method
        )

        for _, row in part.iterrows():
            marker = markers.get(row["Dataset"], "o")
            plt.scatter(
                row["FPR"],
                row["TPR"],
                marker=marker,
                s=80
            )

    plt.plot([0, 1], [0, 1], "r--", linewidth=1, label="Random classifier")

    plt.xlim(0, 0.25)
    plt.ylim(0.6, 1.0)
    plt.xlabel("FPR (False Positive Rate, alpha)")
    plt.ylabel("TPR (True Positive Rate, 1 - beta)")
    plt.title(title)
    plt.grid(True, alpha=0.3)

    dataset_handles = [
        plt.Line2D([0], [0], marker='o', color='black', linestyle='', label='BOSSbase'),
        plt.Line2D([0], [0], marker='s', color='black', linestyle='', label='Medical'),
        plt.Line2D([0], [0], marker='^', color='black', linestyle='', label='Other')
    ]

    handles, labels = plt.gca().get_legend_handles_labels()
    handles.extend(dataset_handles)

    plt.legend(handles=handles, loc="lower right")
    plt.tight_layout()
    plt.savefig(output_file, dpi=200)
    plt.show()
    plt.close()


def main():
    plot_roc_from_csv("method2.csv", "Method 2: FPR-TPR comparison", "roc_method2.png")
    plot_roc_from_csv("method3.csv", "Method 3: FPR-TPR comparison", "roc_method3.png")
    print("Saved: roc_method2.png, roc_method3.png")


if __name__ == "__main__":
    main()