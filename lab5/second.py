import os
import numpy as np
import cv2
import random
import matplotlib.pyplot as plt
from collections import Counter
import itertools

class FingerprintGenerator:
    def __init__(self, n_users, c, eps=0.1):
        self.n_users = n_users
        self.c = c
        self.eps = eps
        self.l = int(2 * c * np.log(n_users / eps))
        self.U = None
        self.user_fingerprints = {}
    
    def generate_matrix(self):
        self.U = np.random.choice([0, 1], size=(self.n_users, self.l))
        for i in range(self.n_users):
            self.user_fingerprints[i] = self.U[i]
        return self.U
    
    def get_fingerprint(self, user_id):
        return self.user_fingerprints.get(user_id, None)

class WatermarkEmbedder:
    def __init__(self, alpha=0.1):
        self.alpha = alpha
    
    def embed_fingerprint_dct(self, image, fingerprint):
        h, w = image.shape
        image_float = np.float32(image)
        dct = cv2.dct(image_float)
        
        fingerprint_len = len(fingerprint)
        coeffs = dct.flatten()
        
        mid_freq_start = 100
        for i in range(min(fingerprint_len, len(coeffs) - mid_freq_start)):
            idx = mid_freq_start + i
            if fingerprint[i] == 1:
                coeffs[idx] += self.alpha
            else:
                coeffs[idx] -= self.alpha
        
        dct_modified = coeffs.reshape(dct.shape)
        idct = cv2.idct(dct_modified)
        stego = np.clip(idct, 0, 255).astype(np.uint8)
        return stego
    
    def extract_fingerprint(self, original, stego, fingerprint_len):
        h, w = original.shape
        original_float = np.float32(original)
        stego_float = np.float32(stego)
        
        original_dct = cv2.dct(original_float)
        stego_dct = cv2.dct(stego_float)
        
        diff = stego_dct.flatten() - original_dct.flatten()
        
        fingerprint = []
        mid_freq_start = 100
        for i in range(min(fingerprint_len, len(diff) - mid_freq_start)):
            idx = mid_freq_start + i
            if diff[idx] > 0:
                fingerprint.append(1)
            else:
                fingerprint.append(0)
        
        return np.array(fingerprint[:fingerprint_len])

class CoalitionAttack:
    def __init__(self, method='average'):
        self.method = method
    
    def attack(self, images, fingerprints, method='average'):
        if not images:
            return None
        
        h, w = images[0].shape
        result = np.zeros((h, w), dtype=np.float32)
        
        if method == 'average':
            for img in images:
                result += img.astype(np.float32)
            result = result / len(images)
        
        elif method == 'median':
            stacked = np.stack(images, axis=2)
            result = np.median(stacked, axis=2)
        
        elif method == 'minmax':
            stacked = np.stack(images, axis=2)
            result = (np.min(stacked, axis=2) + np.max(stacked, axis=2)) / 2
        
        return np.clip(result, 0, 255).astype(np.uint8)

class TardosDetector:
    def __init__(self, n_users, c, eps=0.1):
        self.n_users = n_users
        self.c = c
        self.eps = eps
        self.l = int(2 * c * np.log(n_users / eps))
        self.p = None
        self.threshold = None
    
    def set_probabilities(self, p):
        self.p = p
    
    def compute_scores(self, recovered_fingerprint, U):
        scores = {}
        
        for user_id in range(self.n_users):
            score = 0
            user_fp = U[user_id]
            for j in range(len(recovered_fingerprint)):
                if user_fp[j] == 1 and recovered_fingerprint[j] == 1:
                    score += np.sqrt((1 - self.p[j]) / self.p[j])
                elif user_fp[j] == 0 and recovered_fingerprint[j] == 1:
                    score -= np.sqrt(self.p[j] / (1 - self.p[j]))
            
            scores[user_id] = score
        
        return scores
    
    def detect_colluders(self, scores, c_real):
        sorted_scores = sorted(scores.items(), key=lambda x: x[1], reverse=True)
        detected = [user_id for user_id, _ in sorted_scores[:c_real]]
        return detected
    
    def compute_threshold(self, scores):
        values = list(scores.values())
        mean = np.mean(values)
        std = np.std(values)
        self.threshold = mean + 2 * std
        return self.threshold

class FingerprintSystem:
    def __init__(self, n_users, c, eps=0.1):
        self.n_users = n_users
        self.c = c
        self.eps = eps
        self.fingerprint_gen = FingerprintGenerator(n_users, c, eps)
        self.embedder = WatermarkEmbedder()
        self.detector = TardosDetector(n_users, c, eps)
        self.U = None
        self.user_images = {}
        self.user_fingerprints = {}
        self.pirate_image = None
    
    def setup(self):
        self.U = self.fingerprint_gen.generate_matrix()
        p = np.random.beta(0.5, 0.5, self.fingerprint_gen.l)
        self.detector.set_probabilities(p)
    
    def embed_for_users(self, original_image, user_ids):
        self.user_images = {}
        self.user_fingerprints = {}
        
        for user_id in user_ids:
            fingerprint = self.fingerprint_gen.get_fingerprint(user_id)
            stego = self.embedder.embed_fingerprint_dct(original_image, fingerprint)
            self.user_images[user_id] = stego
            self.user_fingerprints[user_id] = fingerprint
        
        return self.user_images
    
    def coalition_attack(self, colluder_ids, attack_method='average'):
        colluder_images = [self.user_images[uid] for uid in colluder_ids]
        attack = CoalitionAttack()
        self.pirate_image = attack.attack(colluder_images, None, attack_method)
        return self.pirate_image
    
    def extract_fingerprint_from_pirate(self, original_image):
        fingerprint_len = self.fingerprint_gen.l
        recovered = self.embedder.extract_fingerprint(original_image, self.pirate_image, fingerprint_len)
        return recovered
    
    def detect_colluders(self, recovered_fingerprint):
        scores = self.detector.compute_scores(recovered_fingerprint, self.U)
        detected = self.detector.detect_colluders(scores, self.c)
        return detected, scores
    
    def evaluate_detection(self, true_colluders, detected_colluders):
        true_set = set(true_colluders)
        detected_set = set(detected_colluders)
        
        true_positives = len(true_set & detected_set)
        false_positives = len(detected_set - true_set)
        false_negatives = len(true_set - detected_set)
        
        precision = true_positives / len(detected_set) if detected_set else 0
        recall = true_positives / len(true_set) if true_set else 0
        f1 = 2 * precision * recall / (precision + recall) if (precision + recall) > 0 else 0
        
        return {
            'true_positives': true_positives,
            'false_positives': false_positives,
            'false_negatives': false_negatives,
            'precision': precision,
            'recall': recall,
            'f1_score': f1,
            'detected': detected_colluders,
            'true': true_colluders
        }

def load_images_from_folder(folder_path, max_images=20):
    images = []
    for filename in sorted(os.listdir(folder_path)):
        if filename.lower().endswith(('.bmp', '.png', '.jpg', '.jpeg')):
            img_path = os.path.join(folder_path, filename)
            img = cv2.imread(img_path, cv2.IMREAD_GRAYSCALE)
            if img is not None:
                images.append((filename, img))
                if len(images) >= max_images:
                    break
    return images

def research_part1():
    n_users = 10
    c_values = [2, 3, 5]
    eps = 0.1
    
    fingerprint_lengths = []
    thresholds = []
    
    for c in c_values:
        system = FingerprintSystem(n_users, c, eps)
        system.setup()
        fingerprint_lengths.append(system.fingerprint_gen.l)
        
        scores = {}
        for user_id in range(n_users):
            scores[user_id] = random.uniform(-10, 10)
        th = system.detector.compute_threshold(scores)
        thresholds.append(th)
        
        print(f"\nc = {c}")
        print(f"  Длина ЦОП (m): {system.fingerprint_gen.l} бит")
        print(f"  Пороговое значение (Z): {th:.4f}")
        print(f"  Формула: m = 2 * c * ln(n/ε) = 2 * {c} * ln({n_users}/{eps}) = {system.fingerprint_gen.l}")
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 4))
    
    ax1.plot(c_values, fingerprint_lengths, 'bo-', linewidth=2, markersize=8)
    ax1.set_xlabel('Предполагаемый размер коалиции c', fontsize=12)
    ax1.set_ylabel('Длина ЦОП m (бит)', fontsize=12)
    ax1.set_title('Зависимость длины ЦОП от параметра c', fontsize=14)
    ax1.grid(True, alpha=0.3)
    for i, c in enumerate(c_values):
        ax1.annotate(f'{fingerprint_lengths[i]}', (c, fingerprint_lengths[i]), 
                    textcoords="offset points", xytext=(0, 10), ha='center')
    
    ax2.plot(c_values, thresholds, 'rs-', linewidth=2, markersize=8)
    ax2.set_xlabel('Предполагаемый размер коалиции c', fontsize=12)
    ax2.set_ylabel('Пороговое значение Z', fontsize=12)
    ax2.set_title('Зависимость порога детектирования от параметра c', fontsize=14)
    ax2.grid(True, alpha=0.3)
    for i, c in enumerate(c_values):
        ax2.annotate(f'{thresholds[i]:.2f}', (c, thresholds[i]), 
                    textcoords="offset points", xytext=(0, 10), ha='center')
    
    plt.tight_layout()
    plt.savefig('research_part1_dependencies.png', dpi=300, bbox_inches='tight')
    
    return fingerprint_lengths, thresholds, c_values

def research_part2():
    print("=" * 80)
    
    n_users = 10
    c_values = [2, 3, 5]
    c_real_values = [1, 2, 3, 4, 5, 6]
    n_trials = 100
    eps = 0.1
    
    dataset_path = "../lab1/set1"
    images = load_images_from_folder(dataset_path, max_images=10)
    
    if not images:
        print("Изображения не найдены. Используем синтетическое изображение.")
        synthetic_image = np.random.randint(0, 255, (512, 512), dtype=np.uint8)
        test_images = [("synthetic", synthetic_image)]
    else:
        test_images = images
    
    results = {}
    
    for c in c_values:
        print(f"\n--- Предполагаемый размер коалиции c = {c} ---")
        results[c] = {}
        
        for c_real in c_real_values:
            success_count = 0
            trial_results = []
            
            print(f"  Тестирование c_real = {c_real}")
            
            for trial in range(n_trials):
                filename, original_image = test_images[trial % len(test_images)]
                
                system = FingerprintSystem(n_users, c, eps)
                system.setup()
                
                if c_real > n_users:
                    colluder_ids = list(range(n_users))
                else:
                    colluder_ids = sorted(random.sample(range(n_users), c_real))
                
                system.embed_for_users(original_image, range(n_users))
                
                system.coalition_attack(colluder_ids, attack_method='average')
                
                recovered_fingerprint = system.extract_fingerprint_from_pirate(original_image)
                
                detected_colluders, _ = system.detect_colluders(recovered_fingerprint)
                
                detected_set = set(detected_colluders)
                true_set = set(colluder_ids)
                
                correct_detections = len(detected_set & true_set)
                
                if correct_detections == c_real:
                    success_count += 1
                
                trial_results.append({
                    'trial': trial,
                    'true': colluder_ids,
                    'detected': detected_colluders,
                    'correct': correct_detections
                })
            
            detection_rate = success_count / n_trials
            results[c][c_real] = {
                'detection_rate': detection_rate,
                'success_count': success_count,
                'trials': trial_results
            }
            
            print(f"    Вероятность верного обнаружения всех участников: {detection_rate:.2%}")
            print(f"    Успешных испытаний: {success_count}/{n_trials}")
    
    print("\n" + "=" * 80)
    print("СВОДНАЯ ТАБЛИЦА РЕЗУЛЬТАТОВ")
    print("=" * 80)
    
    print("\nВероятность верного обнаружения всех участников коалиции (%):")
    print("c_real \\ c |", end="")
    for c in c_values:
        print(f"  c={c}   |", end="")
    print()
    print("-" * (10 + 12 * len(c_values)))
    
    for c_real in c_real_values:
        print(f"   {c_real:2d}      |", end="")
        for c in c_values:
            if c_real in results[c]:
                rate = results[c][c_real]['detection_rate'] * 100
                print(f"   {rate:5.1f}%  |", end="")
            else:
                print(f"     -    |", end="")
        print()
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    colors = ['blue', 'red', 'green']
    markers = ['o', 's', '^']
    
    for idx, c in enumerate(c_values):
        x_vals = []
        y_vals = []
        for c_real in c_real_values:
            if c_real in results[c]:
                x_vals.append(c_real)
                y_vals.append(results[c][c_real]['detection_rate'] * 100)
        
        ax.plot(x_vals, y_vals, f'{markers[idx]}-', color=colors[idx], 
                linewidth=2, markersize=8, label=f'c = {c}')
    
    ax.set_xlabel('Реальный размер коалиции c_real', fontsize=12)
    ax.set_ylabel('Вероятность верного обнаружения всех участников (%)', fontsize=12)
    ax.set_title('Зависимость эффективности обнаружения от соответствия параметров', fontsize=14)
    ax.grid(True, alpha=0.3)
    ax.legend()
    
    for c in c_values:
        for c_real in c_real_values:
            if c_real in results[c]:
                rate = results[c][c_real]['detection_rate'] * 100
                ax.annotate(f'{rate:.0f}%', 
                           (c_real, rate), 
                           textcoords="offset points", 
                           xytext=(5, 5), 
                           ha='center', fontsize=8)
    
    plt.tight_layout()
    plt.savefig('research_part2_detection_rate.png', dpi=300, bbox_inches='tight')
    
    return results

def research_part2_heatmap(results, c_values, c_real_values):
    data_matrix = []
    for c_real in c_real_values:
        row = []
        for c in c_values:
            if c_real in results[c]:
                row.append(results[c][c_real]['detection_rate'] * 100)
            else:
                row.append(0)
        data_matrix.append(row)
    
    fig, ax = plt.subplots(figsize=(8, 6))
    
    im = ax.imshow(data_matrix, cmap='YlOrRd', aspect='auto', vmin=0, vmax=100)
    
    ax.set_xticks(np.arange(len(c_values)))
    ax.set_yticks(np.arange(len(c_real_values)))
    ax.set_xticklabels([f'c={c}' for c in c_values])
    ax.set_yticklabels([f'c_real={cr}' for cr in c_real_values])
    
    plt.setp(ax.get_xticklabels(), rotation=45, ha="right", rotation_mode="anchor")
    
    for i in range(len(c_real_values)):
        for j in range(len(c_values)):
            value = data_matrix[i][j]
            if value > 0:
                text_color = 'white' if value > 50 else 'black'
                ax.text(j, i, f'{value:.0f}%', ha="center", va="center", color=text_color, fontsize=10)
    
    ax.set_xlabel('Предполагаемый размер коалиции c', fontsize=12)
    ax.set_ylabel('Реальный размер коалиции c_real', fontsize=12)
    ax.set_title('Тепловая карта вероятности верного обнаружения (%)', fontsize=14)
    
    cbar = ax.figure.colorbar(im, ax=ax)
    cbar.ax.set_ylabel('Вероятность верного обнаружения (%)', rotation=-90, va="bottom")
    
    plt.tight_layout()
    plt.savefig('research_part2_heatmap.png', dpi=300, bbox_inches='tight')

def main():
    fingerprint_lengths, thresholds, c_values = research_part1()
    
    results = research_part2()
    
    c_real_values = [1, 2, 3, 4, 5, 6]
    research_part2_heatmap(results, c_values, c_real_values)

if __name__ == "__main__":
    main()