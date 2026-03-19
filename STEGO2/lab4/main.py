import os
import csv
from pathlib import Path

import cv2
import numpy as np
from scipy import stats


SUPPORTED_EXTENSIONS = {".bmp", ".png", ".jpg", ".jpeg", ".tif", ".tiff"}


def is_image_file(path):
    return Path(path).suffix.lower() in SUPPORTED_EXTENSIONS


def load_gray(image_path):
    img = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
    return img


class ChiSquareAnalyzer:
    def __init__(self, n_parts=4):
        self.n_parts = n_parts

    def analyze(self, image_path):
        gray = load_gray(image_path)
        if gray is None:
            return None

        chi2_total, p_value = self._calculate_chi2(gray)

        h, w = gray.shape
        part_h = h // self.n_parts
        part_w = w // self.n_parts

        suspicious_parts = 0
        total_parts = 0

        for i in range(self.n_parts):
            for j in range(self.n_parts):
                block = gray[i * part_h:(i + 1) * part_h, j * part_w:(j + 1) * part_w]
                if block.size == 0:
                    continue
                _, pv = self._calculate_chi2(block)
                total_parts += 1
                if pv < 0.05:
                    suspicious_parts += 1

        suspicious_ratio = suspicious_parts / total_parts if total_parts else 0.0

        if p_value < 0.01:
            verdict = "high probability of stego"
        elif p_value < 0.05:
            verdict = "possible stego"
        else:
            verdict = "looks clean"

        return {
            "chi2": float(chi2_total),
            "p_value": float(p_value),
            "suspicious_parts": suspicious_parts,
            "total_parts": total_parts,
            "suspicious_ratio": suspicious_ratio,
            "verdict": verdict
        }

    def _calculate_chi2(self, block):
        hist = np.bincount(block.flatten(), minlength=256)
        chi2 = 0.0

        for k in range(128):
            even_count = hist[2 * k]
            odd_count = hist[2 * k + 1]
            expected = (even_count + odd_count) / 2.0
            if expected > 0:
                chi2 += (even_count - expected) ** 2 / expected
                chi2 += (odd_count - expected) ** 2 / expected

        p_value = 1.0 - stats.chi2.cdf(chi2, df=128)
        return chi2, p_value


class RSAnalyzer:
    def __init__(self, group_size=4):
        self.group_size = group_size
        self.masks = [
            np.array([0, 1, 1, 0], dtype=np.int16),
            np.array([1, 0, 0, 1], dtype=np.int16)
        ]

    def analyze(self, image_path):
        gray = load_gray(image_path)
        if gray is None:
            return None

        gray = gray.astype(np.int16)

        results = {}
        for idx, mask in enumerate(self.masks):
            r, s, u = self._count_groups(gray, mask)
            total = r + s + u
            results[f"mask_{idx}"] = {
                "R": r,
                "S": s,
                "U": u,
                "R_pct": (r / total * 100.0) if total else 0.0,
                "S_pct": (s / total * 100.0) if total else 0.0,
                "U_pct": (u / total * 100.0) if total else 0.0
            }

        embedding = self._estimate_embedding(results)

        if embedding < 1:
            verdict = "embedding not detected"
        elif embedding <= 5:
            verdict = "probably no hidden message"
        else:
            verdict = "hidden data likely present"

        return {
            "embedding_percent": float(embedding),
            "mask0_R": results["mask_0"]["R_pct"],
            "mask0_S": results["mask_0"]["S_pct"],
            "mask1_R": results["mask_1"]["R_pct"],
            "mask1_S": results["mask_1"]["S_pct"],
            "verdict": verdict
        }

    def _count_groups(self, img, mask):
        h, w = img.shape
        r = 0
        s = 0
        u = 0

        for y in range(h):
            for x in range(0, w - self.group_size, self.group_size):
                block = img[y, x:x + self.group_size]
                f_orig = self._smoothness(block)

                modified = block.copy()
                for i in range(self.group_size):
                    if mask[i] == 1:
                        modified[i] = modified[i] ^ 1

                f_mod = self._smoothness(modified)

                if f_mod > f_orig:
                    r += 1
                elif f_mod < f_orig:
                    s += 1
                else:
                    u += 1

        return r, s, u

    def _smoothness(self, block):
        return np.sum(np.abs(np.diff(block)))

    def _estimate_embedding(self, results):
        r0 = results["mask_0"]["R_pct"]
        s0 = results["mask_0"]["S_pct"]
        r1 = results["mask_1"]["R_pct"]
        s1 = results["mask_1"]["S_pct"]

        denom = 2.0 * (r0 + s0 - r1 - s1)
        if abs(denom) < 1e-10:
            return 0.0

        p = (r1 - s1) / denom
        return max(0.0, min(100.0, abs(p) * 100.0))


class AUMPDetector:
    def __init__(self, m=16, sig_th=1.0, beta_threshold=0.01):
        self.m = m
        self.sig_th = sig_th
        self.beta_threshold = beta_threshold

    def analyze(self, image_path):
        gray = load_gray(image_path)
        if gray is None:
            return None

        x = gray.astype(np.float64)
        beta = self._compute_beta(x)

        if beta < 0:
            verdict = "invalid beta"
        elif beta < self.beta_threshold:
            verdict = "looks clean"
        else:
            verdict = "possible stego"

        return {
            "beta": float(beta),
            "threshold": float(self.beta_threshold),
            "verdict": verdict
        }

    def _compute_beta(self, x):
        h, w = x.shape
        m = self.m
        beta_values = []

        for i in range(0, h - m, m):
            for j in range(0, w - m, m):
                block = x[i:i + m, j:j + m]
                predicted = self._predict_block(block)
                error = block - predicted
                variance = np.var(error)

                if variance > self.sig_th:
                    beta_val = np.mean(np.abs(error)) / np.sqrt(variance + 1e-10)
                    beta_values.append(beta_val)

        return np.median(beta_values) if beta_values else 0.0

    def _predict_block(self, block):
        h, w = block.shape
        predicted = np.zeros_like(block)

        for i in range(h):
            for j in range(w):
                neighbors = []
                if i > 0:
                    neighbors.append(block[i - 1, j])
                if i < h - 1:
                    neighbors.append(block[i + 1, j])
                if j > 0:
                    neighbors.append(block[i, j - 1])
                if j < w - 1:
                    neighbors.append(block[i, j + 1])

                predicted[i, j] = np.mean(neighbors) if neighbors else block[i, j]

        return predicted


def final_decision(chi2_result, rs_result, aump_result):
    score = 0

    if chi2_result is not None and chi2_result["p_value"] < 0.05:
        score += 1
    if rs_result is not None and rs_result["embedding_percent"] > 5:
        score += 1
    if aump_result is not None and aump_result["beta"] >= aump_result["threshold"]:
        score += 1

    if score == 0:
        return "clean"
    if score == 1:
        return "weak suspicion"
    if score == 2:
        return "suspicious"
    return "highly suspicious"


def analyze_file(image_path, chi2_analyzer, rs_analyzer, aump_detector):
    chi2_result = chi2_analyzer.analyze(image_path)
    rs_result = rs_analyzer.analyze(image_path)
    aump_result = aump_detector.analyze(image_path)

    final_result = final_decision(chi2_result, rs_result, aump_result)

    return {
        "file": image_path,
        "chi2": chi2_result,
        "rs": rs_result,
        "aump": aump_result,
        "final": final_result
    }


def print_result(result):
    print("\n" + "=" * 80)
    print(f"File: {result['file']}")
    print("-" * 80)

    chi2_result = result["chi2"]
    if chi2_result:
        print("Chi-square")
        print(f"  chi2             : {chi2_result['chi2']:.4f}")
        print(f"  p-value          : {chi2_result['p_value']:.6f}")
        print(f"  suspicious parts : {chi2_result['suspicious_parts']}/{chi2_result['total_parts']}")
        print(f"  part ratio       : {chi2_result['suspicious_ratio']:.4f}")
        print(f"  verdict          : {chi2_result['verdict']}")
        print()

    rs_result = result["rs"]
    if rs_result:
        print("RS")
        print(f"  embedding %      : {rs_result['embedding_percent']:.4f}")
        print(f"  mask0 R/S        : {rs_result['mask0_R']:.2f} / {rs_result['mask0_S']:.2f}")
        print(f"  mask1 R/S        : {rs_result['mask1_R']:.2f} / {rs_result['mask1_S']:.2f}")
        print(f"  verdict          : {rs_result['verdict']}")
        print()

    aump_result = result["aump"]
    if aump_result:
        print("AUMP")
        print(f"  beta             : {aump_result['beta']:.6f}")
        print(f"  threshold        : {aump_result['threshold']:.6f}")
        print(f"  verdict          : {aump_result['verdict']}")
        print()

    print(f"Final decision: {result['final']}")
    print("=" * 80)


def save_results_csv(results, output_path):
    with open(output_path, "w", newline="", encoding="utf-8-sig") as f:
        writer = csv.writer(f)
        writer.writerow([
            "file",
            "chi2",
            "p_value",
            "chi2_suspicious_parts",
            "chi2_total_parts",
            "chi2_ratio",
            "rs_embedding_percent",
            "rs_mask0_R",
            "rs_mask0_S",
            "rs_mask1_R",
            "rs_mask1_S",
            "aump_beta",
            "aump_threshold",
            "final"
        ])

        for result in results:
            chi2_result = result["chi2"] or {}
            rs_result = result["rs"] or {}
            aump_result = result["aump"] or {}

            writer.writerow([
                result["file"],
                chi2_result.get("chi2", ""),
                chi2_result.get("p_value", ""),
                chi2_result.get("suspicious_parts", ""),
                chi2_result.get("total_parts", ""),
                chi2_result.get("suspicious_ratio", ""),
                rs_result.get("embedding_percent", ""),
                rs_result.get("mask0_R", ""),
                rs_result.get("mask0_S", ""),
                rs_result.get("mask1_R", ""),
                rs_result.get("mask1_S", ""),
                aump_result.get("beta", ""),
                aump_result.get("threshold", ""),
                result["final"]
            ])


def collect_images_from_folder(folder_path):
    files = []
    for name in sorted(os.listdir(folder_path)):
        full_path = os.path.join(folder_path, name)
        if os.path.isfile(full_path) and is_image_file(full_path):
            files.append(full_path)
    return files


def single_file_mode():
    path = input("Enter image path: ").strip().strip('"')
    if not os.path.isfile(path):
        print("File not found.")
        return

    if not is_image_file(path):
        print("Unsupported file format.")
        return

    chi2_analyzer = ChiSquareAnalyzer()
    rs_analyzer = RSAnalyzer()
    aump_detector = AUMPDetector()

    result = analyze_file(path, chi2_analyzer, rs_analyzer, aump_detector)
    print_result(result)


def folder_mode():
    folder = input("Enter folder path: ").strip().strip('"')
    if not os.path.isdir(folder):
        print("Folder not found.")
        return

    files = collect_images_from_folder(folder)
    if not files:
        print("No images found.")
        return

    chi2_analyzer = ChiSquareAnalyzer()
    rs_analyzer = RSAnalyzer()
    aump_detector = AUMPDetector()

    results = []
    for idx, path in enumerate(files, start=1):
        print(f"[{idx}/{len(files)}] {os.path.basename(path)}")
        result = analyze_file(path, chi2_analyzer, rs_analyzer, aump_detector)
        results.append(result)

    print("\nSummary")
    print("-" * 80)
    print(f"{'File':35} {'Chi2 p-value':>14} {'RS %':>10} {'AUMP beta':>12} {'Final':>16}")
    print("-" * 80)

    for result in results:
        file_name = os.path.basename(result["file"])
        p_value = result["chi2"]["p_value"] if result["chi2"] else 0.0
        rs_percent = result["rs"]["embedding_percent"] if result["rs"] else 0.0
        beta = result["aump"]["beta"] if result["aump"] else 0.0
        final = result["final"]

        print(f"{file_name[:35]:35} {p_value:14.6f} {rs_percent:10.3f} {beta:12.6f} {final:>16}")

    save = input("\nSave CSV results? (y/n): ").strip().lower()
    if save == "y":
        output_path = input("Enter CSV path: ").strip().strip('"')
        if output_path:
            save_results_csv(results, output_path)
            print(f"Saved: {output_path}")


def main():
    while True:
        print("\nStegoanalysis")
        print("1 - Analyze one image")
        print("2 - Analyze folder")
        print("0 - Exit")

        choice = input("Select: ").strip()

        if choice == "1":
            single_file_mode()
        elif choice == "2":
            folder_mode()
        elif choice == "0":
            break
        else:
            print("Invalid choice.")


if __name__ == "__main__":
    main()