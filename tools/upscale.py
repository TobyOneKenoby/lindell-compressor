"""AI super-resolution (Real-ESRGAN x4plus via ncnn, CPU) with overlapping tiles.
usage: python3 tools/upscale.py MODEL_DIR in.png out.png [final_scale=2]"""
import sys, ncnn, numpy as np, cv2, time
mdir, src, dst = sys.argv[1], sys.argv[2], sys.argv[3]
final = float(sys.argv[4]) if len(sys.argv) > 4 else 2.0
net = ncnn.Net(); net.opt.use_vulkan_compute = False; net.opt.num_threads = 4
net.load_param(f"{mdir}/realesrgan-x4plus.param"); net.load_model(f"{mdir}/realesrgan-x4plus.bin")
img = cv2.imread(src, cv2.IMREAD_COLOR)[:, :, ::-1].astype(np.float32) / 255.0   # RGB
H, W = img.shape[:2]; T, P, S = 160, 16, 4
out = np.zeros((H*S, W*S, 3), np.float32); wsum = np.zeros((H*S, W*S, 1), np.float32)
# feathered weight for blending overlaps
def weight(h, w):
    wy = np.minimum(np.arange(h)+1, np.arange(h)[::-1]+1).astype(np.float32); wx = np.minimum(np.arange(w)+1, np.arange(w)[::-1]+1).astype(np.float32)
    return np.minimum(np.minimum(wy[:, None], P*S), np.minimum(wx[None, :], P*S))[..., None]
t0 = time.time(); n = 0
ys = list(range(0, H, T)); xs = list(range(0, W, T))
for y in ys:
    for x in xs:
        y0, x0 = max(0, y-P), max(0, x-P); y1, x1 = min(H, y+T+P), min(W, x+T+P)
        tile = np.ascontiguousarray(img[y0:y1, x0:x1].transpose(2, 0, 1))
        ex = net.create_extractor(); ex.input("data", ncnn.Mat(tile))
        _, o = ex.extract("output"); o = np.array(o).transpose(1, 2, 0)
        wgt = weight(o.shape[0], o.shape[1])
        out[y0*S:y1*S, x0*S:x1*S] += o*wgt; wsum[y0*S:y1*S, x0*S:x1*S] += wgt
        n += 1
    print(f"row {y}/{H}  {time.time()-t0:.0f}s", flush=True)
out = np.clip(out/np.maximum(wsum, 1e-6), 0, 1)
res = (out[:, :, ::-1]*255+0.5).astype(np.uint8)
if final != 4:
    res = cv2.resize(res, (int(round(W*final)), int(round(H*final))), interpolation=cv2.INTER_AREA)
cv2.imwrite(dst, res)
print("done", res.shape, f"{time.time()-t0:.0f}s")
