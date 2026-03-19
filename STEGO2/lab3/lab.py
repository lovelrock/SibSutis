import os, json, struct, math, csv, time, tkinter as tk
from tkinter import filedialog, messagebox

import numpy as np
import cv2
from PIL import Image, ImageTk


# ===================== БИТЫ =====================
def bytes_to_bits(data: bytes) -> list[int]:
    out = []
    for b in data:
        for k in range(7, -1, -1):
            out.append((b >> k) & 1)
    return out

def bits_to_bytes(bits: list[int]) -> bytes:
    if len(bits) % 8 != 0:
        raise ValueError("Длина бит не кратна 8")
    out = bytearray()
    for i in range(0, len(bits), 8):
        v = 0
        for b in bits[i:i+8]:
            v = (v << 1) | (b & 1)
        out.append(v)
    return bytes(out)

def u32_to_bits(n: int) -> list[int]:
    return bytes_to_bits(struct.pack(">I", n))

def bits_to_u32(bits32: list[int]) -> int:
    return struct.unpack(">I", bits_to_bytes(bits32))[0]


# ===================== PSNR =====================
def psnr(original_u8: np.ndarray, stego_u8: np.ndarray) -> float:
    a = original_u8.astype(np.float64)
    b = stego_u8.astype(np.float64)
    mse = np.mean((a - b) ** 2)
    if mse == 0:
        return 100.0
    return 10.0 * math.log10((255.0 * 255.0) / mse)


# ===================== t-критическое (95% CI, df<=30) =====================
_T_CRIT_975 = {
    1: 12.706, 2: 4.303, 3: 3.182, 4: 2.776, 5: 2.571,
    6: 2.447, 7: 2.365, 8: 2.306, 9: 2.262, 10: 2.228,
    11: 2.201, 12: 2.179, 13: 2.160, 14: 2.145, 15: 2.131,
    16: 2.120, 17: 2.110, 18: 2.101, 19: 2.093, 20: 2.086,
    21: 2.080, 22: 2.074, 23: 2.069, 24: 2.064, 25: 2.060,
    26: 2.056, 27: 2.052, 28: 2.048, 29: 2.045, 30: 2.042
}

def mean_ci95(values: list[float]) -> tuple[float, float, float]:
    n = len(values)
    if n == 0:
        return (float("nan"), float("nan"), float("nan"))
    m = float(np.mean(values))
    if n == 1:
        return (m, m, m)
    s = float(np.std(values, ddof=1))
    df = n - 1
    tcrit = _T_CRIT_975.get(df, 2.0)
    margin = tcrit * s / math.sqrt(n)
    return (m, m - margin, m + margin)


# ===================== IO изображений =====================
def to_gray(path: str) -> np.ndarray:
    bgr = cv2.imread(path, cv2.IMREAD_COLOR)
    if bgr is None:
        raise RuntimeError(f"Не удалось прочитать изображение: {path}")
    return cv2.cvtColor(bgr, cv2.COLOR_BGR2GRAY)


# ===================== ВАРИАНТ 2 (исправленный, обратимый): ШАХМАТКА =====================
# A: (i+j)%2==0 — опорные, НЕ МЕНЯЕМ
# B: (i+j)%2==1 — сюда внедряем
# Предсказание для B только по соседям A (left/up), поэтому предиктор не "плывёт".
def is_A(i, j): return ((i + j) & 1) == 0
def is_B(i, j): return ((i + j) & 1) == 1

def predict_from_A(img_u8: np.ndarray, i: int, j: int) -> int:
    h, w = img_u8.shape
    vals = []
    if j - 1 >= 0 and is_A(i, j - 1):
        vals.append(int(img_u8[i, j - 1]))
    if i - 1 >= 0 and is_A(i - 1, j):
        vals.append(int(img_u8[i - 1, j]))
    if not vals:
        return 0
    if len(vals) == 1:
        return vals[0]
    return (vals[0] + vals[1] + 1) // 2

def estimate_capacity_bits(img_u8: np.ndarray) -> int:
    # ёмкость = кол-во B-пикселей, где residual==0 и pred!=255 (там реально можно кодировать 0/1)
    h, w = img_u8.shape
    cap = 0
    for i in range(h):
        for j in range(w):
            if not is_B(i, j):
                continue
            pred = predict_from_A(img_u8, i, j)
            x = int(img_u8[i, j])
            r = x - pred
            if r == 0 and pred != 255:
                cap += 1
    return cap

def embed_variant2(img_u8: np.ndarray, payload_bytes: bytes) -> tuple[np.ndarray, dict]:
    if img_u8.ndim != 2 or img_u8.dtype != np.uint8:
        raise ValueError("Картинка должна быть grayscale uint8")

    h, w = img_u8.shape

    bits = u32_to_bits(len(payload_bytes)) + bytes_to_bits(payload_bytes)
    need = len(bits)

    stego = img_u8.copy().astype(np.int16)
    no_shift = []  # координаты B, где r>=1, но x==255 и x+1 сделать нельзя
    k = 0
    

    # pred считаем строго по A из исходного img_u8 (A не меняются вообще)
    for i in range(h):
        for j in range(w):
            if not is_B(i, j):
                continue

            pred = predict_from_A(img_u8, i, j)
            x = int(stego[i, j])
            r = x - pred

            # histogram shifting для r>=1: r->r+1 (это x->x+1)
            if r >= 1:
                if x == 255:
                    no_shift.append([i, j])
                else:
                    r += 1

            # embed на пике r==0
            if r == 0 and k < need:
                b = bits[k]
                if b == 1:
                    if pred != 255:
                        r = 1
                        k += 1
                    else:
                        # нельзя сделать pred+1, пропускаем эту позицию
                        pass
                else:
                    k += 1

            new_x = pred + r
            if not (0 <= new_x <= 255):
                raise RuntimeError(f"Переполнение при внедрении ({i},{j}) -> {new_x}")
            stego[i, j] = new_x

            if k >= need:
                break
        if k >= need:
            break

    if k < need:
        raise RuntimeError(f"Не хватает ёмкости: встроено {k} бит из {need}")

    meta = {
        "algo": "variant2_checkerboard_predict_A + histogram_shift",
        "shape": [int(h), int(w)],
        "bits": int(need),
        "payload_len_bytes": int(len(payload_bytes)),
        "no_shift": no_shift
    }
    return stego.astype(np.uint8), meta

def extract_variant2(stego_u8: np.ndarray, meta: dict) -> tuple[bytes, np.ndarray]:
    if stego_u8.ndim != 2 or stego_u8.dtype != np.uint8:
        raise ValueError("Stego должно быть grayscale uint8")

    h, w = stego_u8.shape
    if meta.get("shape") != [int(h), int(w)]:
        raise RuntimeError("meta.shape не совпадает с изображением")

    need = int(meta["bits"])
    no_shift = set(map(tuple, meta.get("no_shift", [])))

    rec = stego_u8.copy().astype(np.int16)
    out_bits = []

    # pred можно считать по A из stego_u8: A не менялись и совпадают с исходником
    for i in range(h):
        for j in range(w):
            if not is_B(i, j):
                continue

            pred = predict_from_A(stego_u8, i, j)
            x = int(rec[i, j])
            r = x - pred

            if len(out_bits) < need:
                if r == 0:
                    out_bits.append(0)
                elif r == 1:
                    out_bits.append(1)
                    r = 0  # восстановить пик

            # откат shift: r>=2 -> r-1 (если shift реально делали)
            if (i, j) not in no_shift and r >= 2:
                r -= 1

            new_x = pred + r
            if not (0 <= new_x <= 255):
                raise RuntimeError(f"Переполнение при восстановлении ({i},{j}) -> {new_x}")
            rec[i, j] = new_x

    if len(out_bits) < 32:
        raise RuntimeError("Не удалось извлечь заголовок (32 бита)")

    payload_len = bits_to_u32(out_bits[:32])
    payload_bits = out_bits[32:32 + payload_len * 8]
    if len(payload_bits) < payload_len * 8:
        raise RuntimeError("Извлечённых бит меньше, чем нужно по заголовку")

    payload = bits_to_bytes(payload_bits)
    return payload, rec.astype(np.uint8)


# ===================== PAYLOAD ДЛЯ >= 50% ЁМКОСТИ =====================
def make_payload_for_half_capacity(cap_bits: int, seed: int = 123) -> bytes:
    if cap_bits < 32:
        raise RuntimeError("Ёмкость < 32 бит: нельзя даже заголовок")

    target = max(int(math.ceil(0.5 * cap_bits)), 32)
    payload_bytes = int(math.ceil((target - 32) / 8))
    if payload_bytes < 0:
        payload_bytes = 0

    total_bits = 32 + payload_bytes * 8
    while total_bits > cap_bits and payload_bytes > 0:
        payload_bytes -= 1
        total_bits = 32 + payload_bytes * 8
        

    if total_bits > cap_bits:
        raise RuntimeError("Не получается уместить даже заголовок")
    if total_bits < int(math.ceil(0.5 * cap_bits)):
        raise RuntimeError("Невозможно выполнить условие >= 50% ёмкости для этого изображения")

    rng = np.random.default_rng(seed)
    return rng.integers(0, 256, size=payload_bytes, dtype=np.uint8).tobytes()


# ===================== ИССЛЕДОВАНИЕ (папка) =====================
def list_images(folder: str) -> list[str]:
    exts = (".png", ".bmp", ".jpg", ".jpeg", ".tif", ".tiff")
    out = []
    for name in os.listdir(folder):
        if name.lower().endswith(exts):
            out.append(os.path.join(folder, name))
    out.sort()
    return out

def run_research_on_folder(folder: str, out_dir: str, limit: int = 20) -> dict:
    os.makedirs(out_dir, exist_ok=True)

    paths = list_images(folder)[:limit]
    if not paths:
        raise RuntimeError("В папке нет изображений (png/bmp/jpg/tif).")

    rows = []
    psnrs = []
    caps_bpp = []
    ok_restore = 0
    ok_extract = 0

    for idx, p in enumerate(paths, 1):
        name = os.path.basename(p)
        t0 = time.time()
        try:
            orig = to_gray(p)
            h, w = orig.shape
            cap_bits = estimate_capacity_bits(orig)

            payload = make_payload_for_half_capacity(cap_bits, seed=idx * 1000 + 7)
            total_bits = 32 + len(payload) * 8
            bpp = total_bits / (h * w)

            stego, meta = embed_variant2(orig, payload)
            val_psnr = psnr(orig, stego)

            extracted, recovered = extract_variant2(stego, meta)

            restore_ok = bool(np.array_equal(orig, recovered))
            extract_ok = bool(extracted == payload)

            if restore_ok:
                ok_restore += 1
            if extract_ok:
                ok_extract += 1

            psnrs.append(val_psnr)
            caps_bpp.append(bpp)

            rows.append({
                "image": name,
                "h": h,
                "w": w,
                "capacity_bits_est": cap_bits,
                "embedded_bits": total_bits,
                "embedded_bytes": len(payload),
                "bpp": bpp,
                "psnr": val_psnr,
                "restore_ok": int(restore_ok),
                "extract_ok": int(extract_ok),
                "time_sec": time.time() - t0,
                "error": ""
            })

        except Exception as e:
            rows.append({
                "image": name,
                "h": "",
                "w": "",
                "capacity_bits_est": "",
                "embedded_bits": "",
                "embedded_bytes": "",
                "bpp": "",
                "psnr": "",
                "restore_ok": 0,
                "extract_ok": 0,
                "time_sec": time.time() - t0,
                "error": str(e)
            })

    n = len(paths)
    psnr_mean, psnr_lo, psnr_hi = mean_ci95(psnrs)
    bpp_mean = float(np.mean(caps_bpp)) if caps_bpp else float("nan")
    bpp_max = float(np.max(caps_bpp)) if caps_bpp else float("nan")

    summary = {
        "folder": folder,
        "n_images": n,
        "restore_success": ok_restore,
        "restore_success_pct": (ok_restore / n) * 100.0,
        "extract_success": ok_extract,
        "extract_success_pct": (ok_extract / n) * 100.0,
        "psnr_mean": psnr_mean,
        "psnr_ci95": [psnr_lo, psnr_hi],
        "bpp_mean": bpp_mean,
        "bpp_max": bpp_max
    }

    csv_path = os.path.join(out_dir, "results.csv")
    fieldnames = [
        "image", "h", "w", "capacity_bits_est", "embedded_bits", "embedded_bytes",
        "bpp", "psnr", "restore_ok", "extract_ok", "time_sec", "error"
    ]
    with open(csv_path, "w", newline="", encoding="utf-8") as f:
        wri = csv.DictWriter(f, fieldnames=fieldnames)
        wri.writeheader()
        for r in rows:
            wri.writerow(r)

    json_path = os.path.join(out_dir, "summary.json")
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2)

    return summary


# ===================== GUI =====================
class App:
    def __init__(self, root: tk.Tk):
        self.root = root
        root.title("RDH (вариант 2) — GUI + исследование (обратимо)")

        self.img_path = None
        self.payload_path = None
        self.tkimg = None

        frm = tk.Frame(root, padx=10, pady=10)
        frm.pack(fill="both", expand=True)

        tk.Label(
            frm,
            text=("RDH вариант 2 (исправлено): шахматное разбиение + предсказание по A + histogram shifting ошибок.\n"
                  "Кнопка «Исследование» делает анализ по папке (до 20 изображений)."),
            justify="left"
        ).pack(anchor="w")

        row = tk.Frame(frm)
        row.pack(fill="x", pady=6)

        tk.Button(row, text="Контейнер", command=self.pick_img).pack(side="left")
        tk.Button(row, text="Файл данных", command=self.pick_payload).pack(side="left", padx=6)
        tk.Button(row, text="Встроить", command=self.do_embed).pack(side="left", padx=6)
        tk.Button(row, text="Извлечь", command=self.do_extract).pack(side="left", padx=6)

        row2 = tk.Frame(frm)
        row2.pack(fill="x", pady=6)
        tk.Button(row2, text="Исследование (папка)", command=self.do_research).pack(side="left")

        self.lbl_paths = tk.Label(frm, text="Контейнер: -\nФайл: -", justify="left")
        self.lbl_paths.pack(anchor="w", pady=4)

        self.lbl_cap = tk.Label(frm, text="Ёмкость: -", justify="left")
        self.lbl_cap.pack(anchor="w", pady=4)

        self.preview = tk.Label(frm)
        self.preview.pack(pady=8)

    def upd(self):
        self.lbl_paths.config(text=f"Контейнер: {self.img_path or '-'}\nФайл: {self.payload_path or '-'}")
        if self.img_path:
            g = to_gray(self.img_path)
            c = estimate_capacity_bits(g)
            self.lbl_cap.config(text=f"Ёмкость (бит): {c}  (~{c//8} байт)")
            disp = g
            h, w = disp.shape
            s = min(1.0, 520 / max(1, w))
            if s < 1.0:
                disp = cv2.resize(disp, (int(w*s), int(h*s)), interpolation=cv2.INTER_AREA)
            self.tkimg = ImageTk.PhotoImage(Image.fromarray(disp))
            self.preview.config(image=self.tkimg)

    def pick_img(self):
        p = filedialog.askopenfilename(
            title="Выберите контейнер",
            filetypes=[("Изображения", "*.png *.bmp *.jpg *.jpeg *.tif *.tiff"), ("Все файлы", "*.*")]
        )
        if p:
            self.img_path = p
            self.upd()

    def pick_payload(self):
        p = filedialog.askopenfilename(title="Выберите файл данных", filetypes=[("Все файлы", "*.*")])
        if p:
            self.payload_path = p
            self.upd()

    def do_embed(self):
        if not self.img_path or not self.payload_path:
            return messagebox.showwarning("Нужно выбрать", "Выберите контейнер и файл данных.")

        try:
            orig = to_gray(self.img_path)
            cap_bits = estimate_capacity_bits(orig)

            payload = open(self.payload_path, "rb").read()
            need_bits = 32 + len(payload) * 8
            if cap_bits <= 0:
                raise RuntimeError("Ёмкость 0: выберите другое изображение.")
            if need_bits < int(math.ceil(0.5 * cap_bits)):
                raise RuntimeError(
                    f"Файл слишком маленький для условия >= 50%.\n"
                    f"Нужно >= {int(math.ceil(0.5*cap_bits))} бит, есть {need_bits} бит."
                )

            stego, meta = embed_variant2(orig, payload)

            out_img = filedialog.asksaveasfilename(
                title="Сохранить stego",
                defaultextension=".png",
                filetypes=[("PNG", "*.png"), ("BMP", "*.bmp"), ("Все файлы", "*.*")]
            )
            if not out_img:
                return

            cv2.imwrite(out_img, stego)
            out_meta = os.path.splitext(out_img)[0] + ".rdh.json"
            with open(out_meta, "w", encoding="utf-8") as f:
                json.dump(meta, f, ensure_ascii=False, indent=2)
                

            messagebox.showinfo("Готово", f"Сохранено:\n{out_img}\n{out_meta}")

        except Exception as e:
            messagebox.showerror("Ошибка", str(e))

    def do_extract(self):
        try:
            stego_path = filedialog.askopenfilename(
                title="Выберите stego-изображение",
                filetypes=[("Изображения", "*.png *.bmp *.jpg *.jpeg *.tif *.tiff"), ("Все файлы", "*.*")]
            )
            if not stego_path:
                return

            meta_path = filedialog.askopenfilename(
                title="Выберите meta (.rdh.json)",
                filetypes=[("JSON", "*.json"), ("Все файлы", "*.*")]
            )
            if not meta_path:
                return

            stego = to_gray(stego_path)
            meta = json.load(open(meta_path, "r", encoding="utf-8"))

            payload, recovered = extract_variant2(stego, meta)

            out_payload = filedialog.asksaveasfilename(
                title="Сохранить извлечённый файл как",
                defaultextension=".bin",
                filetypes=[("Все файлы", "*.*")]
            )
            if not out_payload:
                return
            open(out_payload, "wb").write(payload)

            out_rec = filedialog.asksaveasfilename(
                title="Сохранить восстановленное изображение как",
                defaultextension=".png",
                filetypes=[("PNG", "*.png"), ("BMP", "*.bmp"), ("Все файлы", "*.*")]
            )
            if not out_rec:
                return
            cv2.imwrite(out_rec, recovered)

            messagebox.showinfo("Готово", f"Файл: {out_payload}\nИзображение: {out_rec}")

        except Exception as e:
            messagebox.showerror("Ошибка", str(e))

    def do_research(self):
        try:
            folder = filedialog.askdirectory(title="Выберите папку с изображениями (один набор)")
            if not folder:
                return
            out_dir = filedialog.askdirectory(title="Куда сохранить отчёт (results.csv, summary.json)")
            if not out_dir:
                return

            summary = run_research_on_folder(folder, out_dir, limit=20)

            messagebox.showinfo(
                "Исследование готово",
                f"Папка: {summary['folder']}\n"
                f"Изображений: {summary['n_images']}\n"
                f"Восстановление: {summary['restore_success_pct']:.1f}%\n"
                f"Извлечение: {summary['extract_success_pct']:.1f}%\n"
                f"PSNR среднее: {summary['psnr_mean']:.3f}\n"
                f"95% CI: [{summary['psnr_ci95'][0]:.3f}; {summary['psnr_ci95'][1]:.3f}]\n"
                f"bpp среднее: {summary['bpp_mean']:.4f}\n"
                f"bpp max: {summary['bpp_max']:.4f}\n\n"
                f"Файлы: results.csv и summary.json сохранены в выбранной папке."
            )
        except Exception as e:
            messagebox.showerror("Ошибка исследования", str(e))


if __name__ == "__main__":
    root = tk.Tk()
    App(root)
    root.mainloop()