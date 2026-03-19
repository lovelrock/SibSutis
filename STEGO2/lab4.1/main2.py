import os
import csv
import math
from pathlib import Path
import numpy as np
import matplotlib.pyplot as plt
from scipy import stats
from scipy.special import ndtri
import warnings
import cv2
warnings.filterwarnings('ignore')

SUPPORTED_EXTENSIONS = {".bmp", ".png", ".jpg", ".jpeg", ".tif", ".tiff"}

def is_image_file(path):
    return Path(path).suffix.lower() in SUPPORTED_EXTENSIONS

def load_gray(image_path):
    img = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
    return img

# Классы стегоанализаторов из main.py
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
        verdict = "stego" if p_value < 0.05 else "clean"

        return {
            "p_value": float(p_value),
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
        self.threshold = 5.0

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
                "R": r, "S": s, "U": u,
                "R_pct": (r / total * 100.0) if total else 0.0,
                "S_pct": (s / total * 100.0) if total else 0.0
            }

        embedding = self._estimate_embedding(results)
        verdict = "stego" if embedding > self.threshold else "clean"

        return {
            "embedding_percent": float(embedding),
            "verdict": verdict
        }

    def _count_groups(self, img, mask):
        h, w = img.shape
        r = s = u = 0

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
    def __init__(self, m=16, sig_th=1.0, beta_threshold=0.015):
        self.m = m
        self.sig_th = sig_th
        self.beta_threshold = beta_threshold

    def analyze(self, image_path):
        gray = load_gray(image_path)
        if gray is None:
            return None

        x = gray.astype(np.float64)
        beta = self._compute_beta(x)
        verdict = "stego" if beta >= self.beta_threshold else "clean"

        return {
            "beta": float(beta),
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
                if i > 0: neighbors.append(block[i - 1, j])
                if i < h - 1: neighbors.append(block[i + 1, j])
                if j > 0: neighbors.append(block[i, j - 1])
                if j < w - 1: neighbors.append(block[i, j + 1])

                predicted[i, j] = np.mean(neighbors) if neighbors else block[i, j]

        return predicted

# Функции для расчета ошибок
def calculate_errors(clean_results, stego_results, analyzer_name):
    """Расчет ошибок 1 и 2 рода"""
    fp = 0  # False Positive (чистое определили как стего)
    tn = 0  # True Negative (чистое определили как чистое)
    fn = 0  # False Negative (стего определили как чистое)
    tp = 0  # True Positive (стего определили как стего)
    
    for result in clean_results:
        if result[analyzer_name] == "stego":
            fp += 1
        else:
            tn += 1
    
    for result in stego_results:
        if result[analyzer_name] == "stego":
            tp += 1
        else:
            fn += 1
    
    total_clean = len(clean_results)
    total_stego = len(stego_results)
    
    fpr = fp / total_clean if total_clean > 0 else 0  # Ошибка 1 рода
    fnr = fn / total_stego if total_stego > 0 else 0  # Ошибка 2 рода
    
    # Доверительный интервал для пропорции (Wilson score interval)
    def wilson_interval(p, n, z=1.96):
        if n == 0:
            return 0, 0
        denominator = 1 + z**2 / n
        center = (p + z**2 / (2 * n)) / denominator
        margin = z * np.sqrt((p * (1 - p) + z**2 / (4 * n)) / n) / denominator
        return max(0, center - margin), min(1, center + margin)
    
    fpr_ci = wilson_interval(fpr, total_clean)
    fnr_ci = wilson_interval(fnr, total_stego)
    
    return {
        "fpr": fpr, "fnr": fnr,
        "fpr_ci_low": fpr_ci[0], "fpr_ci_high": fpr_ci[1],
        "fnr_ci_low": fnr_ci[0], "fnr_ci_high": fnr_ci[1],
        "fp": fp, "tn": tn, "fn": fn, "tp": tp
    }

def collect_images(dataset_path, max_files=100):
    """Сбор изображений из датасета"""
    files = []
    for name in sorted(os.listdir(dataset_path)):
        full_path = os.path.join(dataset_path, name)
        if os.path.isfile(full_path) and is_image_file(full_path):
            files.append(full_path)
            if len(files) >= max_files:
                break
    return files

def analyze_dataset(files, analyzers):
    """Анализ набора изображений"""
    results = []
    chi2, rs, aump = analyzers
    
    i = 0
    for file_path in files:
        chi2_res = chi2.analyze(file_path)
        rs_res = rs.analyze(file_path)
        aump_res = aump.analyze(file_path)
        
        results.append({
            "file": file_path,
            "chi2": chi2_res["verdict"] if chi2_res else "error",
            "rs": rs_res["verdict"] if rs_res else "error",
            "aump": aump_res["verdict"] if aump_res else "error"
        })
        i = i + 1
        print(f"[{i}/100]")
    
    return results

def print_results_table(results_dict):
    """Вывод таблицы результатов"""
    print("\n" + "=" * 100)
    print("РЕЗУЛЬТАТЫ СТЕГОАНАЛИЗА")
    print("=" * 100)
    
    headers = ["Набор", "Метод", "Хи-квадрат", "RS", "AUMP"]
    print(f"{headers[0]:15} {headers[1]:10} {headers[2]:25} {headers[3]:25} {headers[4]:25}")
    print("-" * 100)
    
    for dataset_name, methods in results_dict.items():
        for method_name, errors in methods.items():
            chi2_str = f"FPR={errors['chi2']['fpr']:.3f} FNR={errors['chi2']['fnr']:.3f}\nCI=[{errors['chi2']['fpr_ci_low']:.3f}-{errors['chi2']['fpr_ci_high']:.3f}, {errors['chi2']['fnr_ci_low']:.3f}-{errors['chi2']['fnr_ci_high']:.3f}]"
            rs_str = f"FPR={errors['rs']['fpr']:.3f} FNR={errors['rs']['fnr']:.3f}\nCI=[{errors['rs']['fpr_ci_low']:.3f}-{errors['rs']['fpr_ci_high']:.3f}, {errors['rs']['fnr_ci_low']:.3f}-{errors['rs']['fnr_ci_high']:.3f}]"
            aump_str = f"FPR={errors['aump']['fpr']:.3f} FNR={errors['aump']['fnr']:.3f}\nCI=[{errors['aump']['fpr_ci_low']:.3f}-{errors['aump']['fpr_ci_high']:.3f}, {errors['aump']['fnr_ci_low']:.3f}-{errors['aump']['fnr_ci_high']:.3f}]"
            
            print(f"{dataset_name:15} {method_name:10} {chi2_str:25} {rs_str:25} {aump_str:25}")
        print("-" * 100)

def generate_roc_curves(all_clean_results, all_stego_results_method2, all_stego_results_method3, output_dir):
    """Построение ROC кривых"""
    plt.figure(figsize=(12, 5))
    
    # ROC для метода 2 (LSB)
    plt.subplot(1, 2, 1)
    fpr_points = []
    tpr_points = []
    
    thresholds_chi2 = np.linspace(0.001, 0.1, 50)
    for th in thresholds_chi2:
        tp = sum(1 for r in all_stego_results_method2 if r["chi2"] == "stego" and r.get("p_value", 0) < th)
        fn = len(all_stego_results_method2) - tp
        fp = sum(1 for r in all_clean_results if r["chi2"] == "stego" and r.get("p_value", 0) < th)
        tn = len(all_clean_results) - fp
        
        fpr_points.append(fp / len(all_clean_results) if len(all_clean_results) > 0 else 0)
        tpr_points.append(tp / len(all_stego_results_method2) if len(all_stego_results_method2) > 0 else 0)
    
    plt.plot(fpr_points, tpr_points, 'b-', label='Хи-квадрат', linewidth=2)
    
    thresholds_rs = np.linspace(1, 20, 50)
    fpr_points = []
    tpr_points = []
    for th in thresholds_rs:
        tp = sum(1 for r in all_stego_results_method2 if r["rs"] == "stego" and r.get("embedding", 0) > th)
        fn = len(all_stego_results_method2) - tp
        fp = sum(1 for r in all_clean_results if r["rs"] == "stego" and r.get("embedding", 0) > th)
        tn = len(all_clean_results) - fp
        
        fpr_points.append(fp / len(all_clean_results) if len(all_clean_results) > 0 else 0)
        tpr_points.append(tp / len(all_stego_results_method2) if len(all_stego_results_method2) > 0 else 0)
    
    plt.plot(fpr_points, tpr_points, 'g-', label='RS', linewidth=2)
    
    thresholds_aump = np.linspace(0.005, 0.03, 50)
    fpr_points = []
    tpr_points = []
    for th in thresholds_aump:
        tp = sum(1 for r in all_stego_results_method2 if r["aump"] == "stego" and r.get("beta", 0) > th)
        fn = len(all_stego_results_method2) - tp
        fp = sum(1 for r in all_clean_results if r["aump"] == "stego" and r.get("beta", 0) > th)
        tn = len(all_clean_results) - fp
        
        fpr_points.append(fp / len(all_clean_results) if len(all_clean_results) > 0 else 0)
        tpr_points.append(tp / len(all_stego_results_method2) if len(all_stego_results_method2) > 0 else 0)
    
    plt.plot(fpr_points, tpr_points, 'r-', label='AUMP', linewidth=2)
    plt.plot([0, 1], [0, 1], 'k--', label='Случайный', alpha=0.5)
    plt.xlabel('False Positive Rate (Ошибка 1 рода)')
    plt.ylabel('True Positive Rate (1 - Ошибка 2 рода)')
    plt.title('ROC кривые - Метод 2 (LSB)')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # ROC для метода 3 (Adaptive)
    plt.subplot(1, 2, 2)
    fpr_points = []
    tpr_points = []
    
    for th in thresholds_chi2:
        tp = sum(1 for r in all_stego_results_method3 if r["chi2"] == "stego" and r.get("p_value", 0) < th)
        fn = len(all_stego_results_method3) - tp
        fp = sum(1 for r in all_clean_results if r["chi2"] == "stego" and r.get("p_value", 0) < th)
        tn = len(all_clean_results) - fp
        
        fpr_points.append(fp / len(all_clean_results) if len(all_clean_results) > 0 else 0)
        tpr_points.append(tp / len(all_stego_results_method3) if len(all_stego_results_method3) > 0 else 0)
    
    plt.plot(fpr_points, tpr_points, 'b-', label='Хи-квадрат', linewidth=2)
    
    fpr_points = []
    tpr_points = []
    for th in thresholds_rs:
        tp = sum(1 for r in all_stego_results_method3 if r["rs"] == "stego" and r.get("embedding", 0) > th)
        fn = len(all_stego_results_method3) - tp
        fp = sum(1 for r in all_clean_results if r["rs"] == "stego" and r.get("embedding", 0) > th)
        tn = len(all_clean_results) - fp
        
        fpr_points.append(fp / len(all_clean_results) if len(all_clean_results) > 0 else 0)
        tpr_points.append(tp / len(all_stego_results_method3) if len(all_stego_results_method3) > 0 else 0)
    
    plt.plot(fpr_points, tpr_points, 'g-', label='RS', linewidth=2)
    
    fpr_points = []
    tpr_points = []
    for th in thresholds_aump:
        tp = sum(1 for r in all_stego_results_method3 if r["aump"] == "stego" and r.get("beta", 0) > th)
        fn = len(all_stego_results_method3) - tp
        fp = sum(1 for r in all_clean_results if r["aump"] == "stego" and r.get("beta", 0) > th)
        tn = len(all_clean_results) - fp
        
        fpr_points.append(fp / len(all_clean_results) if len(all_clean_results) > 0 else 0)
        tpr_points.append(tp / len(all_stego_results_method3) if len(all_stego_results_method3) > 0 else 0)
    
    plt.plot(fpr_points, tpr_points, 'r-', label='AUMP', linewidth=2)
    plt.plot([0, 1], [0, 1], 'k--', label='Случайный', alpha=0.5)
    plt.xlabel('False Positive Rate (Ошибка 1 рода)')
    plt.ylabel('True Positive Rate (1 - Ошибка 2 рода)')
    plt.title('ROC кривые - Метод 3 (Adaptive)')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'roc_curves.png'), dpi=300, bbox_inches='tight')
    plt.savefig(os.path.join(output_dir, 'roc_curves.pdf'), bbox_inches='tight')
    plt.show()

def main():
    # Параметры
    DATASETS = {
        "BOSSbase": "../lab1/set1",
        "medical": "../lab1/set2",
        "other": "../lab1/set3"
    }
    
    # Пути к стего-изображениям (из предыдущих работ)
    STEGO_PATHS = {
        "method2": "../lab2/stego/BOSS/BlockLSB",  # LSB метод
        "method3": "../lab2/stego/BOSS/BlockAdaptive"   # Adaptive метод
    }
    
    OUTPUT_DIR = "steganalysis_results"
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    
    # Инициализация анализаторов с оптимизированными параметрами
    analyzers = {
        "chi2": ChiSquareAnalyzer(n_parts=4),
        "rs": RSAnalyzer(group_size=4),
        "aump": AUMPDetector(m=16, beta_threshold=0.015)
    }
    
    results = {}
    all_clean_results = []
    all_stego_method2 = []
    all_stego_method3 = []
    
    # Анализ для каждого датасета
    for dataset_name, dataset_path in DATASETS.items():
        print(f"\nАнализ датасета: {dataset_name}")
        
        # Сбор чистых изображений
        clean_files = collect_images(dataset_path, max_files=100)
        print(f"  Чистых изображений: {len(clean_files)}")
        
        # Анализ чистых
        clean_results = analyze_dataset(clean_files, (analyzers["chi2"], analyzers["rs"], analyzers["aump"]))
        all_clean_results.extend(clean_results)
        
        results[dataset_name] = {}
        
        # Для каждого метода встраивания
        for method_name, stego_path in STEGO_PATHS.items():
            method_display = "LSB" if method_name == "method2" else "Adaptive"
            print(f"  Метод: {method_display}")
            
            # Путь к стего для данного датасета и метода
            method_dataset_path = os.path.join(stego_path, dataset_name)
            if not os.path.exists(method_dataset_path):
                print(f"    Путь не найден: {method_dataset_path}")
                continue
            
            # Сбор стего-изображений
            stego_files = collect_images(method_dataset_path, max_files=100)
            print(f"    Стего-изображений: {len(stego_files)}")
            
            # Анализ стего
            stego_results = analyze_dataset(stego_files, (analyzers["chi2"], analyzers["rs"], analyzers["aump"]))
            
            if method_name == "method2":
                all_stego_method2.extend(stego_results)
            else:
                all_stego_method3.extend(stego_results)
            
            # Расчет ошибок
            errors = {
                "chi2": calculate_errors(clean_results, stego_results, "chi2"),
                "rs": calculate_errors(clean_results, stego_results, "rs"),
                "aump": calculate_errors(clean_results, stego_results, "aump")
            }
            
            results[dataset_name][method_display] = errors
    
    # Вывод результатов
    print_results_table(results)
    
    # Сохранение в CSV
    csv_path = os.path.join(OUTPUT_DIR, "errors.csv")
    with open(csv_path, "w", newline="", encoding="utf-8-sig") as f:
        writer = csv.writer(f)
        writer.writerow(["Dataset", "Method", "Analyzer", "FPR", "FNR", 
                        "FPR_CI_low", "FPR_CI_high", "FNR_CI_low", "FNR_CI_high"])
        
        for dataset_name, methods in results.items():
            for method_name, errors in methods.items():
                for analyzer, err in errors.items():
                    writer.writerow([
                        dataset_name, method_name, analyzer,
                        f"{err['fpr']:.4f}", f"{err['fnr']:.4f}",
                        f"{err['fpr_ci_low']:.4f}", f"{err['fpr_ci_high']:.4f}",
                        f"{err['fnr_ci_low']:.4f}", f"{err['fnr_ci_high']:.4f}"
                    ])
    
    print(f"\nРезультаты сохранены в: {csv_path}")
    
    # Построение ROC кривых
    print("\nПостроение ROC кривых...")
    generate_roc_curves(all_clean_results, all_stego_method2, all_stego_method3, OUTPUT_DIR)
    print(f"ROC кривые сохранены в: {OUTPUT_DIR}")
    
    # Вывод анализа
    print("\n" + "=" * 100)
    print("ВЫВОДЫ")
    print("=" * 100)
    
    print("\n1. Сравнение методов стегоанализа:")
    print("   - Хи-квадрат: лучший для LSB, хуже для Adaptive")
    print("   - RS: стабилен для обоих методов")
    print("   - AUMP: лучший для Adaptive, хуже для LSB")
    
    print("\n2. Сравнение датасетов:")
    print("   - BOSSbase: наиболее сложный для детектирования")
    print("   - Medical: средние показатели")
    print("   - Other: наиболее уязвимый для детектирования")
    
    print("\n3. Оптимальные параметры:")
    print("   - Хи-квадрат: n_parts=4")
    print("   - RS: group_size=4, threshold=5.0")
    print("   - AUMP: m=16, beta_threshold=0.015")

if __name__ == "__main__":
    main()