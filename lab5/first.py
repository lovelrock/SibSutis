import os
import numpy as np
import cv2
import random
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
    
    def embed_fingerprint_lsb(self, image, fingerprint):
        h, w = image.shape
        stego = image.copy()
        fingerprint_len = len(fingerprint)
        
        flat = stego.flatten()
        for i in range(min(fingerprint_len, len(flat))):
            flat[i] = (flat[i] & 0xFE) | fingerprint[i]
        
        stego = flat.reshape(h, w)
        return stego
    
    def embed_fingerprint_adaptive(self, image, fingerprint):
        h, w = image.shape
        stego = image.copy()
        fingerprint_len = len(fingerprint)
        
        gradient = np.zeros_like(image, dtype=np.float32)
        for y in range(1, h-1):
            for x in range(1, w-1):
                gx = float(image[y, x+1]) - float(image[y, x-1])
                gy = float(image[y+1, x]) - float(image[y-1, x])
                gradient[y, x] = np.sqrt(gx*gx + gy*gy)
        
        positions = np.argsort(gradient.flatten())[::-1]
        
        flat = stego.flatten()
        for i in range(min(fingerprint_len, len(flat))):
            pos = positions[i]
            flat[pos] = (flat[pos] & 0xFE) | fingerprint[i]
        
        stego = flat.reshape(h, w)
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
        
        elif method == 'random':
            result = images[0].astype(np.float32)
            noise = np.random.normal(0, 2, (h, w))
            result = np.clip(result + noise, 0, 255)
        
        return np.clip(result, 0, 255).astype(np.uint8)

class TardosDetector:
    def __init__(self, n_users, c, eps=0.1):
        self.n_users = n_users
        self.c = c
        self.eps = eps
        self.l = int(2 * c * np.log(n_users / eps))
        self.p = None
        self.threshold = None
    
    def generate_probabilities(self):
        self.p = np.random.beta(0.5, 0.5, self.l)
        return self.p
    
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

    def save_images(self, original_image, user_images, pirate_image, output_dir):
        os.makedirs(output_dir, exist_ok=True)
        
        original_path = os.path.join(output_dir, "original.bmp")
        cv2.imwrite(original_path, original_image)
        
        for user_id, img in user_images.items():
            user_path = os.path.join(output_dir, f"user_{user_id}_fingerprinted.bmp")
            cv2.imwrite(user_path, img)
        
        pirate_path = os.path.join(output_dir, "pirate_copy.bmp")
        cv2.imwrite(pirate_path, pirate_image)
        
        return True

    def save_fingerprints(self, user_fingerprints, recovered_fingerprint, evaluation, 
                        true_colluders, detected_colluders, output_dir):
        os.makedirs(output_dir, exist_ok=True)
        
        with open(os.path.join(output_dir, "user_fingerprints.txt"), "w") as f:
            for user_id, fp in user_fingerprints.items():
                fp_str = ''.join(str(b) for b in fp)
                f.write(f"User {user_id}: {fp_str}\n")
        
        with open(os.path.join(output_dir, "recovered_fingerprint.txt"), "w") as f:
            fp_str = ''.join(str(b) for b in recovered_fingerprint)
            f.write(f"Recovered fingerprint: {fp_str}\n")
        
        with open(os.path.join(output_dir, "detection_results.txt"), "w") as f:
            f.write(f"True colluders: {true_colluders}\n")
            f.write(f"Detected colluders: {detected_colluders}\n")
            f.write(f"Precision: {evaluation['precision']:.4f}\n")
            f.write(f"Recall: {evaluation['recall']:.4f}\n")
            f.write(f"F1 Score: {evaluation['f1_score']:.4f}\n")

def load_images_from_folder(folder_path, max_images=100):
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



def main():
    dataset_path = "../lab1/set1"
    
    n_users = 10
    c = int(input("\nВведите предполагаемый размер коалиции (c): "))
    c_real = int(input("Введите реальный размер коалиции (c_real): "))
    
    if c_real > n_users:
        print(f"Ошибка: c_real ({c_real}) не может превышать n_users ({n_users})")
        return
    
    epsilon = 0.1
    images = load_images_from_folder(dataset_path, max_images=5)
    
    if not images:
        print("Изображения не найдены в указанной папке")
        return
    
    print(f"\nЗагружено изображений: {len(images)}")
    print(f"Параметры: n={n_users}, c={c}, c_real={c_real}, ε={epsilon}")
    
    system = FingerprintSystem(n_users, c, epsilon)
    system.setup()
    
    colluder_ids = sorted(random.sample(range(n_users), c_real))
    print(f"\nРеальные участники коалиции: {colluder_ids}")
    
    filename, original_image = images[0]
    print(f"\nИспользуемое изображение: {filename}")
    print(f"Размер изображения: {original_image.shape}")
    
    user_images = system.embed_for_users(original_image, range(n_users))
    
    pirate_image = system.coalition_attack(colluder_ids, attack_method='average')
    
    recovered_fingerprint = system.extract_fingerprint_from_pirate(original_image)
    
    detected_colluders, scores = system.detect_colluders(recovered_fingerprint)
    
    print("=" * 70)
    
    print(f"\nРеальные участники коалиции: {colluder_ids}")
    print(f"Обнаруженные участники:      {detected_colluders}")
    
    evaluation = system.evaluate_detection(colluder_ids, detected_colluders)

    system.save_images(original_image, user_images, pirate_image, "./images/")
    
    system.save_fingerprints(
        system.user_fingerprints, 
        recovered_fingerprint, 
        evaluation,
        colluder_ids,
        detected_colluders,
        "./finprint/"
    )
    
    print(f"\nВерно обнаружено:   {evaluation['true_positives']}/{c_real}")
    print(f"Ложные срабатывания: {evaluation['false_positives']}")
    print(f"Пропущенные:         {evaluation['false_negatives']}")
    print(f"\nТочность (Precision):  {evaluation['precision']:.4f}")
    print(f"Полнота (Recall):      {evaluation['recall']:.4f}")
    print(f"F1-мера:               {evaluation['f1_score']:.4f}")
    
    sorted_scores = sorted(scores.items(), key=lambda x: x[1], reverse=True)
    for user_id, score in sorted_scores[:c_real + 3]:
        if user_id in detected_colluders:
            marker = " (обнаружен)"
        print(f"Пользователь {user_id:2d}: {score:8.4f}{marker}")
    
    # if system.fingerprint_gen.l < 100:
        # print("\n⚠ ВНИМАНИЕ: Длина отпечатка мала для надежного детектирования.")
        # print("  Рекомендуется увеличить параметр c для повышения длины.")
    

if __name__ == "__main__":
    main()