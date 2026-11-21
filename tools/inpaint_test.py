import cv2, numpy as np, sys
sys.path.insert(0,'tools'); from controls import CONTROLS, RADIUS
img=cv2.imread('Resources/panel_source.webp')
b,g,r=[img[...,i].astype(int) for i in range(3)]
chroma=np.maximum(b,g)-r
v=np.max(img,axis=2).astype(int)
H,W=img.shape[:2]; yy,xx=np.mgrid[0:H,0:W]
mask=np.zeros((H,W),np.uint8)
for name,(t,x,y) in CONTROLS.items():
    R=RADIUS[t]
    if t=="screw": continue
    disc=(xx-x)**2+(yy-y)**2<R*R
    # knob (neutral) or shadow (darker than panel); labels are bright & excluded
    m=disc&((chroma<30)|(v<95))&~((v>170)&(chroma<40)&(r>150) & ~((xx-x)**2+(yy-y)**2<(R*0.55)**2))
    mask|=m.astype(np.uint8)
mask=cv2.morphologyEx(mask,cv2.MORPH_CLOSE,np.ones((5,5),np.uint8))
mask=cv2.dilate(mask,np.ones((5,5),np.uint8))
out=cv2.inpaint(img,mask*255,6,cv2.INPAINT_TELEA)
cv2.imwrite('/tmp/claude-0/-home-user/55601520-fe6b-5a10-9263-b01ecc58796f/scratchpad/inp.png',out)
cv2.imwrite('/tmp/claude-0/-home-user/55601520-fe6b-5a10-9263-b01ecc58796f/scratchpad/mask.png',mask*255)
