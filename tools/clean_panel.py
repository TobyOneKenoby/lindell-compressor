import cv2, numpy as np, sys
sys.path.insert(0,'tools'); from controls import CONTROLS, RADIUS
SP='/tmp/claude-0/-home-user/55601520-fe6b-5a10-9263-b01ecc58796f/scratchpad/'
img=cv2.imread('Resources/panel_source.webp').astype(np.float32)
H,W=img.shape[:2]; yy,xx=np.mgrid[0:H,0:W]
b,g,r=img[...,0],img[...,1],img[...,2]
chroma=np.maximum(b,g)-r; v=img.max(axis=2)
label=(v>165)&(chroma<45)   # cream text / dots / scale

def fill_holes(m):
    m=m.astype(np.uint8); inv=(1-m).copy()
    ff=inv.copy(); h,w=m.shape; tmp=np.zeros((h+2,w+2),np.uint8)
    cv2.floodFill(ff,tmp,(0,0),2)
    return (m|(ff==1)).astype(np.uint8)

def control_mask(t,x,y):
    R=RADIUS[t]
    disc=((xx-x)**2+(yy-y)**2<R*R)
    if t=="screw": return np.zeros((H,W),np.uint8)
    obj=disc&((((chroma<32)&(v<150))|(v<90)))&~label
    obj=cv2.morphologyEx(obj.astype(np.uint8),cv2.MORPH_CLOSE,np.ones((7,7),np.uint8))
    if t in ("fluted",):
        # never swallow scale digits around the skirt
        outer=((xx-x)**2+(yy-y)**2>(64 if t=="fluted" else 30)**2)
        obj&=~(cv2.dilate(label.astype(np.uint8),np.ones((5,5),np.uint8)).astype(bool)&outer)
    # keep only the component(s) touching the centre region
    n,lab=cv2.connectedComponents(obj)
    keep=np.zeros_like(obj)
    core=((xx-x)**2+(yy-y)**2<(R*0.35)**2)
    for i in range(1,n):
        if (core&(lab==i)).any(): keep|=(lab==i).astype(np.uint8)
    keep=fill_holes(keep)
    raw=(disc&(chroma<25)&(v<140)).astype(np.uint8)
    n2,lab2=cv2.connectedComponents(raw); rk=np.zeros_like(raw)
    for i in range(1,n2):
        if (core&(lab2==i)).any(): rk|=(lab2==i).astype(np.uint8)
    objs[(t,x,y)]=fill_holes(rk)
    # drop shadow: light comes from top-left, shadow falls down-right
    sh=np.roll(np.roll(keep,8,axis=0),4,axis=1)
    big=cv2.dilate(keep|sh,cv2.getStructuringElement(cv2.MORPH_ELLIPSE,(15,15)))
    return big

mask=np.zeros((H,W),np.uint8)
masks={}; objs={}
for name,(t,x,y) in CONTROLS.items():
    m=control_mask(t,x,y); masks[name]=m; mask|=m
# labels that the dilated masks swallowed: keep them (only outside the object core)
core=np.zeros((H,W),bool)
for name,(t,x,y) in CONTROLS.items():
    rr={"fluted":67,"meterknob":40,"toggle":22}.get(t,0)
    if rr: core|=((xx-x)**2+(yy-y)**2<rr*rr)
objAll=np.zeros((H,W),np.uint8)
for o in objs.values(): objAll|=o
objAll=cv2.dilate(objAll,np.ones((5,5),np.uint8))
# metering selector: the arrow outline at the top belongs to the knob
for dy in (0,268):
    tri=np.array([[486,516+dy],[466,566+dy],[506,566+dy]],np.int32)
    cv2.fillPoly(objAll,[tri],1)
objAll=objAll.astype(bool)
restore=label&(mask>0)&~core&~objAll
restore=cv2.dilate(restore.astype(np.uint8),np.ones((5,5),np.uint8)).astype(bool)&(mask>0)&~objAll
# trim screws (ZERO/BAL) stay in the artwork
for name,(t,x,y) in CONTROLS.items():
    if t=='screw': restore|=((xx-x)**2+(yy-y)**2<14*14)
def needle_mask(px,py,x0,y0,x1,y1):
    best=None
    for ang in np.arange(-60,60,0.25):
        a=np.radians(ang); cnt=0; pts=[]
        for rr in range(25,150):
            X=int(round(px+rr*np.sin(a))); Y=int(round(py-rr*np.cos(a)))
            if not(x0<=X<x1 and y0<=Y<y1): continue
            pts.append((X,Y))
            if v[Y,X]<90: cnt+=1
        if best is None or cnt>best[0]: best=(cnt,ang,pts)
    m=np.zeros((H,W),np.uint8)
    for X,Y in best[2]: cv2.circle(m,(X,Y),2,1,-1)
    print("needle angle",best[1],"hits",best[0])
    return m
needles=[]
for (px,py,box) in [(262,640,(190,505,362,628)),(262,905,(190,775,362,895))]:
    nm=needle_mask(px,py,*box); needles.append(nm)
# smooth panel estimate by normalised convolution of clean panel pixels
panel=(chroma>35)&(v>60)&(v<175)
w=((mask==0)&~label&panel).astype(np.float32)
sig=25
num=cv2.GaussianBlur(img*w[...,None],(0,0),sig); den=cv2.GaussianBlur(w,(0,0),sig)[...,None]
smooth=num/np.maximum(den,1e-3)
# powder-coat texture from a clean patch, tiled with random offsets
patch=img[778:812,1012:1072]; hp=patch-cv2.GaussianBlur(patch,(0,0),6)
tex=np.zeros_like(img); rng=np.random.default_rng(3)
ph,pw=hp.shape[:2]
for y0 in range(0,H,ph):
    for x0 in range(0,W,pw):
        blk=hp[::rng.choice([1,-1]),::rng.choice([1,-1])]
        tex[y0:y0+ph,x0:x0+pw]=blk[:min(ph,H-y0),:min(pw,W-x0)]
fill=smooth+tex
m3=cv2.GaussianBlur(mask.astype(np.float32),(0,0),1.2)[...,None]
out=img*(1-m3)+fill*m3
out[restore]=img[restore]
# ---- text patches ------------------------------------------------------
src=cv2.imread('Resources/panel_source.webp').astype(np.float32)
def bbox_of_label(x0,y0,x1,y1):
    m=label[y0:y1,x0:x1]; ys,xs=np.nonzero(m)
    return x0+xs.min()-1, y0+ys.min()-1, x0+xs.max()+2, y0+ys.max()+2
def paste_text(box, cx, cy, scale=1.0):
    x0,y0,x1,y1=box; crop=src[y0:y1,x0:x1]
    lab=label[y0:y1,x0:x1].astype(np.float32)
    a=cv2.GaussianBlur(cv2.dilate(lab,np.ones((2,2),np.uint8)),(0,0),0.6)
    if scale!=1.0:
        crop=cv2.resize(crop,None,fx=scale,fy=scale,interpolation=cv2.INTER_CUBIC)
        a=cv2.resize(a,None,fx=scale,fy=scale,interpolation=cv2.INTER_CUBIC)
    h,w=a.shape; X=int(round(cx-w/2)); Y=int(round(cy-h/2))
    col=np.array([214,226,232],np.float32)  # cream text colour (BGR)
    region=out[Y:Y+h,X:X+w]
    out[Y:Y+h,X:X+w]=region*(1-a[...,None])+col*a[...,None]
def text_alpha(x0,y0,x1,y1):
    vv=v[y0:y1,x0:x1]
    return np.clip((vv-110.0)/(205.0-110.0),0,1)*(chroma[y0:y1,x0:x1]<60)
def paste(box, cx=None, cy=None, scale=1.0):
    x0,y0,x1,y1=box; a=text_alpha(x0,y0,x1,y1).astype(np.float32)
    if scale!=1.0: a=cv2.resize(a,None,fx=scale,fy=scale,interpolation=cv2.INTER_CUBIC).clip(0,1)
    h,w=a.shape
    X=x0 if cx is None else int(round(cx-w/2)); Y=y0 if cy is None else int(round(cy-h/2))
    col=np.array([212,224,230],np.float32)
    out[Y:Y+h,X:X+w]=out[Y:Y+h,X:X+w]*(1-a[...,None])+col*a[...,None]
for box in [(404,662,452,680),(532,662,562,680),(404,931,452,949),(532,931,562,949)]:
    paste(box)
paste((404,662,452,680),486,521); paste((404,931,452,949),486,788)
# BYPASS (left, under the meters) becomes LINK = L/R control link
bp=(104,838,160,860)
txt=np.zeros((H,W),np.uint8); txt[bp[1]:bp[3],bp[0]:bp[2]]=(label[bp[1]:bp[3],bp[0]:bp[2]]|(v[bp[1]:bp[3],bp[0]:bp[2]]>130))
o8=cv2.inpaint(np.clip(out,0,255).astype(np.uint8),cv2.dilate(txt,np.ones((3,3),np.uint8))*255,4,cv2.INPAINT_TELEA)
out=o8.astype(np.float32)
paste((1059,707,1089,721),132,848,13.0/11.0)
# needles: inpaint on the meter face (small area -> Telea is fine there)
o8=np.clip(out,0,255).astype(np.uint8)
for nm in needles: o8=cv2.inpaint(o8,nm*255,3,cv2.INPAINT_TELEA)
cv2.imwrite(SP+'clean.png',o8)
cv2.imwrite('Resources/panel_clean.png',o8)
np.save(SP+'mask.npy',mask)
print("patch std",hp.std())
