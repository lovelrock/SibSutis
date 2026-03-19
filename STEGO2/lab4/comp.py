import os
import csv
import math
from pathlib import Path

import cv2
import numpy as np


SUPPORTED_EXTENSIONS = {".bmp", ".png", ".jpg", ".jpeg", ".tif", ".tiff"}


class CompressionAnalyzer:
    def __init__(self, quality=90, mse_threshold=35.0):
        self.quality = quality
        self.mse_threshold = mse_threshold

    def analyze(self, image_path):
        gray = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
        if gray is None:
            return None

        encode_params = [int(cv2.IMWRITE_JPEG_QUALITY), self.quality]
        ok, buffer = cv2.imencode(".jpg", gray, encode_params)
        if not ok:
            return None

        recompressed = cv2.imdecode(buffer, cv2.IMREAD_GRAYSCALE)
        if recompressed is None or recompressed.shape != gray.shape:
            return None

        diff = gray.astype(np.float64) - recompressed.astype(np.float64)
        mse = float(np.mean(diff ** 2))

        if mse <= 0:
            psnr = 100.0
        else:
            psnr = float(10.0 * math.log10((255.0 * 255.0) / mse))

        if mse < self.mse_threshold:
            verdict = "clean"
        else:
            verdict = "possible stego"

        return {
            "mse": mse,
            "psnr": psnr,
            "quality": self.quality,
            "threshold": self.mse_threshold,
            "verdict": verdict
        }


def is_image_file(path):
    return Path(path).suffix.lower() in SUPPORTED_EXTENSIONS


def collect_images_recursive(folder):
    files = []
    for root, _, names in os.walk(folder):
        for name in names:
            full_path = os.path.join(root, name)
            if is_image_file(full_path):
                files.append(full_path)
    files.sort()
    return files


def detect_dataset_name(path):
    parts = Path(path).parts
    lower_parts = [p.lower() for p in parts]

    # if "set1" in lower_parts:
    #     return "set1"
    # if "set2" in lower_parts:
    #     return "set2"
    # if "set3" in lower_parts:
    #     return "set3"
    if "images" in lower_parts:
        return "images"
    return "unknown"


def analyze_directories(base_dir, quality=90, mse_threshold=35.0):
    analyzer = CompressionAnalyzer(quality=quality, mse_threshold=mse_threshold)

    # target_sets = ["set1", "set2", "set3"]
    target_sets = ["images"]
    all_results = []

    for set_name in target_sets:
        folder = os.path.join(base_dir, set_name)
        if not os.path.isdir(folder):
            print(f"Folder not found: {folder}")
            continue

        files = collect_images_recursive(folder)
        print(f"{set_name}: found {len(files)} image(s)")

        for index, file_path in enumerate(files, start=1):
            print(f"[{set_name}] {index}/{len(files)} - {file_path}")
            result = analyzer.analyze(file_path)

            if result is None:
                all_results.append({
                    "dataset": set_name,
                    "file": file_path,
                    "mse": "",
                    "psnr": "",
                    "quality": quality,
                    "threshold": mse_threshold,
                    "verdict": "error"
                })
                continue

            all_results.append({
                "dataset": set_name,
                "file": file_path,
                "mse": result["mse"],
                "psnr": result["psnr"],
                "quality": result["quality"],
                "threshold": result["threshold"],
                "verdict": result["verdict"]
            })

    return all_results


def save_results_csv(results, output_path):
    with open(output_path, "w", newline="", encoding="utf-8-sig") as f:
        writer = csv.writer(f, delimiter=",")
        writer.writerow([
            "dataset",
            "file",
            "mse",
            "psnr",
            "quality",
            "threshold",
            "verdict"
        ])

        for row in results:
            writer.writerow([
                row["dataset"],
                row["file"],
                row["mse"],
                row["psnr"],
                row["quality"],
                row["threshold"],
                row["verdict"]
            ])


def print_summary(results):
    grouped = {}

    for row in results:
        dataset = row["dataset"]
        grouped.setdefault(dataset, {
            "total": 0,
            "clean": 0,
            "possible stego": 0,
            "error": 0,
            "mse_values": [],
            "psnr_values": []
        })

        grouped[dataset]["total"] += 1
        grouped[dataset][row["verdict"]] += 1

        if isinstance(row["mse"], (int, float)):
            grouped[dataset]["mse_values"].append(row["mse"])
        if isinstance(row["psnr"], (int, float)):
            grouped[dataset]["psnr_values"].append(row["psnr"])

    print("\nSummary")
    print("-" * 80)
    print(f"{'Dataset':10} {'Total':>8} {'Clean':>8} {'Stego':>8} {'Error':>8} {'Mean MSE':>12} {'Mean PSNR':>12}")
    print("-" * 80)

    for dataset, info in grouped.items():
        mean_mse = np.mean(info["mse_values"]) if info["mse_values"] else 0.0
        mean_psnr = np.mean(info["psnr_values"]) if info["psnr_values"] else 0.0

        print(
            f"{dataset:10} "
            f"{info['total']:8d} "
            f"{info['clean']:8d} "
            f"{info['possible stego']:8d} "
            f"{info['error']:8d} "
            f"{mean_mse:12.4f} "
            f"{mean_psnr:12.4f}"
        )


def main():
    print("Compression-based steganalysis")
    base_dir = input("Enter base directory containing set1, set2, set3: ").strip().strip('"')
    if not os.path.isdir(base_dir):
        print("Base directory not found.")
        return

    quality_input = input("JPEG quality [90]: ").strip()
    threshold_input = input("MSE threshold [35]: ").strip()

    quality = 90 if quality_input == "" else int(quality_input)
    mse_threshold = 35.0 if threshold_input == "" else float(threshold_input.replace(",", "."))

    results = analyze_directories(base_dir, quality=quality, mse_threshold=mse_threshold)

    if not results:
        print("No results.")
        return

    output_path = os.path.join(base_dir, "compression_analysis_results.csv")
    save_results_csv(results, output_path)

    print_summary(results)
    print(f"\nSaved: {output_path}")


if __name__ == "__main__":
    main()