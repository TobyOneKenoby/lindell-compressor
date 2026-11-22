"""Restore the fine powder-coat grain that super-resolution smooths away and
write the 2x panel JPEG.  usage: finish_2x.py clean_2x.png out.jpg"""
import sys, cv2, numpy as np
img = cv2.imread(sys.argv[1]).astype(np.float32)
H, W = img.shape[:2]
b, g, r = img[..., 0], img[..., 1], img[..., 2]
paint = ((np.maximum(b, g) - r) > 30) & (img.max(axis=2) < 175)       # blue paint, not legends/hardware
paint = cv2.erode(paint.astype(np.uint8), np.ones((3, 3), np.uint8)).astype(np.float32)
paint = cv2.GaussianBlur(paint, (0, 0), 1.0)[..., None]
rng = np.random.default_rng(670)
fine = rng.normal(0, 1, (H, W)).astype(np.float32)
fine = fine - cv2.GaussianBlur(fine, (0, 0), 2.0)                       # 1-2 px grain
peel = cv2.GaussianBlur(rng.normal(0, 1, (H, W)).astype(np.float32), (0, 0), 3.0)  # orange peel
tex = fine * 5.0 + peel * 9.0
out = img + (tex[..., None] * np.array([1.0, 0.95, 0.8], np.float32)) * paint
cv2.imwrite(sys.argv[2], np.clip(out, 0, 255).astype(np.uint8), [cv2.IMWRITE_JPEG_QUALITY, 93])
print("ok", out.shape)
