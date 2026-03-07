from PIL import Image
import struct
import os
from math import log10


def MSE(image1, image2):
    size = image1.size
    pixels1 = image1.load()
    pixels2 = image2.load()
    mse = 0
    for i in range(size[0]):
        for j in range(size[1]):
            mse += (pixels1[i, j] - pixels2[i, j]) ** 2
    return mse / (size[0] * size[1])


def PSNR(image1, image2):
    mse = MSE(image1, image2)
    return 10 * log10(255 ** 2 / mse) if mse != 0 else float('inf')


def calculate_histogram(image):
    hist = [0] * 256
    width, height = image.size
    pixels = image.load()
    for y in range(height):
        for x in range(width):
            hist[pixels[x, y]] += 1
    return hist


def find_max_in_range(hist, left, right):
    max_val, max_idx = -1, left
    for i in range(left, min(right, 256)):
        if hist[i] > max_val:
            max_val, max_idx = hist[i], i
    return max_idx


def find_min_max_pairs(hist, count):
    temp_hist = hist.copy()
    min_points = []
    for _ in range(count):
        min_val, min_idx = float('inf'), 0
        for i in range(1, 255):
            if 0 <= temp_hist[i] < min_val:
                min_val, min_idx = temp_hist[i], i
        min_points.append(min_idx)
        temp_hist[min_idx] = float('inf')
    min_points.sort()

    peak_candidates = [find_max_in_range(hist, 0, min_points[0])]
    for i in range(count - 1):
        mid = (min_points[i] + min_points[i + 1]) // 2
        peak_candidates.append(find_max_in_range(hist, min_points[i], mid))
        peak_candidates.append(find_max_in_range(hist, mid, min_points[i + 1]))
    peak_candidates.append(find_max_in_range(hist, min_points[-1], 256))

    selected_peaks = []
    for i in range(count):
        pair_start = i * 2
        if pair_start + 1 < len(peak_candidates):
            idx = pair_start if hist[peak_candidates[pair_start]] > hist[peak_candidates[pair_start + 1]] else pair_start + 1
            selected_peaks.append(peak_candidates[idx])
        else:
            selected_peaks.append(peak_candidates[pair_start])
    return min_points, selected_peaks


def bytes_to_bits(data):
    bits = []
    for byte in data:
        for i in range(7, -1, -1):
            bits.append((byte >> i) & 1)
    return bits


def bits_to_bytes(bits):
    while len(bits) % 8 != 0:
        bits.append(0)
    result = []
    for i in range(0, len(bits), 8):
        byte = 0
        for j in range(8):
            byte = (byte << 1) | bits[i + j]
        result.append(byte)
    return result


def hide_mppz_text(image_path, text_path, output_path, peak_count=3):
    image = Image.open(image_path).convert('L')
    width, height = image.size
    pixels = image.load()
    hist = calculate_histogram(image)
    zero_points, peak_points = find_min_max_pairs(hist, peak_count)

    print(f"Найдены zero points: {zero_points}")
    print(f"Найдены peak points: {peak_points}")

    with open(text_path, 'r', encoding='utf-8') as f:
        text_data = f.read()
    text_bytes = text_data.encode('utf-8')
    wm_length = len(text_bytes)
    payload = struct.pack('<I', wm_length) + text_bytes
    data_bits = bytes_to_bits(list(payload))

    capacity = sum(hist[p] for p in peak_points)
    print(f"Ёмкость: {capacity} бит ({capacity // 8} байт)")

    if len(data_bits) > capacity:
        raise ValueError(f"Не хватает места: нужно {len(data_bits)} бит, доступно {capacity}")

    zeros_info = []
    for p in range(peak_count):
        zero = zero_points[p]
        zero_indices = [y * width + x for y in range(height) for x in range(width) if pixels[x, y] == zero]
        zeros_info.append(zero_indices)

    zeros_bytes = []
    for indices in zeros_info:
        zeros_bytes.append(len(indices))
        zeros_bytes.extend(indices)
    zeros_bytes.append(0xFFFFFFFF)
    for val in zeros_bytes:
        for i in range(31, -1, -1):
            data_bits.append((val >> i) & 1)

    if len(data_bits) > capacity:
        raise ValueError(f"Данные + метаданные не помещаются: {len(data_bits)} бит")

    marked = [[pixels[x, y] for x in range(width)] for y in range(height)]
    data_idx = 0

    for p in range(peak_count):
        peak, zero = peak_points[p], zero_points[p]
        if zero < peak:
            for y in range(height):
                for x in range(width):
                    if zero < marked[y][x] < peak:
                        marked[y][x] -= 1
        else:
            for y in range(height):
                for x in range(width):
                    if peak < marked[y][x] < zero:
                        marked[y][x] += 1

        for y in range(height):
            for x in range(width):
                if data_idx >= len(data_bits):
                    break
                if marked[y][x] == peak:
                    if (zero < peak and data_bits[data_idx] == 0) or (zero > peak and data_bits[data_idx] == 1):
                        marked[y][x] += 1 if zero > peak else -1
                    data_idx += 1
            if data_idx >= len(data_bits):
                break

    stego = Image.new('L', (width, height))
    stego_pixels = stego.load()
    for y in range(height):
        for x in range(width):
            stego_pixels[x, y] = marked[y][x]

    os.makedirs(os.path.dirname(output_path) or '.', exist_ok=True)
    stego.save(output_path)
    print(f"Внедрено {data_idx} бит. Файл: {output_path}")

    return {
        'zero_points': zero_points,
        'peak_points': peak_points,
        'embedded_bits': data_idx,
        'capacity': capacity,
        'text_length': wm_length
    }


def extract_mppz_text(stego_path, zero_points, peak_points):
    image = Image.open(stego_path).convert('L')
    width, height = image.size
    pixels = image.load()
    peak_count = len(peak_points)
    extracted_bits = []

    for p in range(peak_count):
        peak, zero = peak_points[p], zero_points[p]
        for y in range(height):
            for x in range(width):
                val = pixels[x, y]
                if zero < peak:
                    if val == peak:
                        extracted_bits.append(1)
                    elif val == peak - 1:
                        extracted_bits.append(0)
                        pixels[x, y] = peak
                else:
                    if val == peak:
                        extracted_bits.append(0)
                    elif val == peak + 1:
                        extracted_bits.append(1)
                        pixels[x, y] = peak

    if len(extracted_bits) < 32:
        raise ValueError("Недостаточно бит для извлечения длины")

    length_bytes = bits_to_bytes(extracted_bits[:32])
    wm_length = struct.unpack('<I', bytes(length_bytes))[0]

    text_bits = extracted_bits[32:32 + wm_length * 8]
    text_bytes = bytes(bits_to_bytes(text_bits))
    extracted_text = text_bytes.decode('utf-8', errors='replace')

    meta_bits = extracted_bits[32 + wm_length * 8:]
    meta_bytes = bits_to_bytes(meta_bits)

    zeros_info = []
    idx = 0
    while idx + 4 <= len(meta_bytes):
        count = struct.unpack('<I', bytes(meta_bytes[idx:idx+4]))[0]
        idx += 4
        if count == 0xFFFFFFFF:
            break
        indices = []
        if idx + count * 4 > len(meta_bytes):
            remaining = (len(meta_bytes) - idx) // 4
            for _ in range(remaining):
                indices.append(struct.unpack('<I', bytes(meta_bytes[idx:idx+4]))[0])
                idx += 4
            zeros_info.append(indices)
            break
        for _ in range(count):
            indices.append(struct.unpack('<I', bytes(meta_bytes[idx:idx+4]))[0])
            idx += 4
        zeros_info.append(indices)

    for p in range(peak_count):
        peak, zero = peak_points[p], zero_points[p]
        zero_set = set(zeros_info[p]) if p < len(zeros_info) else set()
        for y in range(height):
            for x in range(width):
                linear_idx = y * width + x
                if linear_idx in zero_set:
                    continue
                val = pixels[x, y]
                if zero < peak:
                    if zero <= val < peak:
                        pixels[x, y] = val + 1
                else:
                    if peak < val <= zero:
                        pixels[x, y] = val - 1

    restored = Image.new('L', (width, height))
    restored_pixels = restored.load()
    for y in range(height):
        for x in range(width):
            restored_pixels[x, y] = pixels[x, y]

    return extracted_text, restored


if __name__ == "__main__":
    original_path = "../sets/set1/2.bmp"
    text_path = "text.txt"
    stego_path = "output/stego_text.bmp"
    restored_path = "output/restored.bmp"
    output_text_path = "output/extracted_text.txt"

    with open(text_path, 'r', encoding='utf-8') as f:
        original_text = f.read()

    print(f"Исходный текст: {len(original_text)} символов, {len(original_text.encode('utf-8'))} байт")

    result = hide_mppz_text(original_path, text_path, stego_path, peak_count=3)
    original = Image.open(original_path).convert('L')
    stego = Image.open(stego_path).convert('L')
    print(f"PSNR: {PSNR(original, stego):.2f} dB")

    extracted_text, restored = extract_mppz_text(stego_path, result['zero_points'], result['peak_points'])
    mse_og_rest, psnr_og_rest = MSE(original, restored), PSNR(original, restored)
    print(f"\nOriginal and restored:\n  MSE:  {mse_og_rest:.6f}\n  PSNR: {psnr_og_rest:.2f} dB")