"""Extract per-control sprites + layout from the panel artwork.
Outputs Resources/sprites/*.png and Source/UI/Layout.h"""
import cv2, numpy as np, sys, os, json
sys.path.insert(0,'tools'); from controls import CONTROLS, RADIUS
src=cv2.imread('Resources/panel_source.webp').astype(np.float32)
# optional AI-upscaled source (tools/upscale.py) -> sprites are written at 2x
_src2=cv2.imread('Resources/panel_source_2x.webp')
src2=_src2.astype(np.float32) if _src2 is not None else None
clean=cv2.imread('Resources/panel_clean.png').astype(np.float32)
H,W=src.shape[:2]; yy,xx=np.mgrid[0:H,0:W]
def feats(im):
    b,g,r=im[...,0],im[...,1],im[...,2]
    return np.maximum(b,g)-r, im.max(axis=2)
chroma,v=feats(src); cchroma,cv_=feats(clean)
os.makedirs('Resources/sprites',exist_ok=True)

def fill_holes(m):
    m=m.astype(np.uint8); ff=(1-m).copy(); tmp=np.zeros((m.shape[0]+2,m.shape[1]+2),np.uint8)
    cv2.floodFill(ff,tmp,(0,0),2); return (m|(ff==1)).astype(np.uint8)

def silhouette(t,x,y):
    R=RADIUS[t]+(10 if t!="toggle" else 8)
    disc=((xx-x)**2+(yy-y)**2<R*R)
    if t=="toggle":
        m=(disc&((chroma<42)|(v>150))).astype(np.uint8)
    else:
        m=(disc&(chroma<28)&(v<150)).astype(np.uint8)
    m=cv2.morphologyEx(m,cv2.MORPH_OPEN,np.ones((2,2),np.uint8))
    n,lab=cv2.connectedComponents(m); keep=np.zeros_like(m)
    core=((xx-x)**2+(yy-y)**2<(RADIUS[t]*0.3)**2)
    for i in range(1,n):
        if (core&(lab==i)).any(): keep|=(lab==i).astype(np.uint8)
    keep=cv2.morphologyEx(keep,cv2.MORPH_CLOSE,np.ones((3,3),np.uint8))
    keep=fill_holes(keep)
    if t=="fluted":
        # the skirt is round: nothing outside it belongs to the knob (scale digits!)
        # round knob: use the convex outline (catches the lit flute tops)
        keep&=((xx-x)**2+(yy-y)**2<70*70).astype(np.uint8)
        cnts,_=cv2.findContours(keep,cv2.RETR_EXTERNAL,cv2.CHAIN_APPROX_SIMPLE)
        hull=np.zeros_like(keep); cv2.fillPoly(hull,[cv2.convexHull(np.vstack(cnts))],1)
        return hull
    if t!="toggle":
        # cream index stripes that touch the knob body belong to it; loose scale
        # digits (separated by a gap of panel paint) do not
        bright=(disc&(chroma<30)&(v>=150)).astype(np.uint8)
        nb,lb=cv2.connectedComponents(bright)
        near=cv2.dilate(keep,np.ones((3,3),np.uint8))
        ky,kx=np.nonzero(keep); reach=np.hypot(kx-x,ky-y).max()
        dist=np.hypot(xx-x,yy-y)
        for i in range(1,nb):
            comp=(lb==i)
            if (comp&(near>0)).sum()>=3: keep|=(comp&(dist<=reach-1.0)).astype(np.uint8)
        keep=fill_holes(keep)
        # trim anything beyond the reach of the solid black body (digit fragments)
        body=keep.astype(bool)&(v<85)
        by,bx=np.nonzero(body)
        if len(bx):
            ang=np.degrees(np.arctan2(xx-x,-(yy-y)))
            bang=np.degrees(np.arctan2(bx-x,-(by-y))); bd=np.hypot(bx-x,by-y)
            # per 6-degree sector: furthest body pixel
            reachS=np.zeros(60)
            np.maximum.at(reachS,((bang+180)//6).astype(int)%60,bd)
            reachS=np.maximum(reachS,np.roll(reachS,1)); reachS=np.maximum(reachS,np.roll(reachS,-1))
            sector=((ang+180)//6).astype(int)%60
            keep&=(np.hypot(xx-x,yy-y)<=reachS[sector]+1.5).astype(np.uint8)
        # bright specks that are not embedded in black knob material = printed digits
        dens=cv2.blur(body.astype(np.float32),(7,7))
        br=(keep.astype(bool)&(v>=120)).astype(np.uint8)
        nb2,lb2,st2,_=cv2.connectedComponentsWithStats(br)
        for i in range(1,nb2):
            comp=lb2==i
            if st2[i,4]<40 and dens[comp].mean()<0.45:
                keep[cv2.dilate(comp.astype(np.uint8),np.ones((3,3),np.uint8)).astype(bool)&(v>=95)]=0
        return keep
    if False:
        cnts,_=cv2.findContours(keep,cv2.RETR_EXTERNAL,cv2.CHAIN_APPROX_SIMPLE)
        hull=np.zeros_like(keep); cv2.fillPoly(hull,[cv2.convexHull(np.vstack(cnts))],1)
        hull=cv2.erode(hull,np.ones((3,3),np.uint8))
        bright=(disc&(chroma<30)&(v>=150)).astype(np.uint8)&hull
        keep=fill_holes(keep|bright)
    return keep

def label_angles(px,py,rmin,rmax,expect,descending=False, dots=False):
    ring=((xx-px)**2+(yy-py)**2>rmin*rmin)&((xx-px)**2+(yy-py)**2<rmax*rmax)
    lab=(ring&(cv_>150)&(cchroma<50)).astype(np.uint8)
    if dots:  # small ring markers 'o' for list selectors: keep compact blobs only
        lab=cv2.dilate(lab,np.ones((2,2),np.uint8))
    else:
        lab=cv2.dilate(lab,cv2.getStructuringElement(cv2.MORPH_ELLIPSE,(7,7)))
    n,l,st,ce=cv2.connectedComponentsWithStats(lab)
    angs=[]
    for i in range(1,n):
        if st[i,4]<6: continue
        if dots and (st[i,2]>9 or st[i,3]>9): continue
        cx,cy=ce[i]; angs.append(float(np.degrees(np.arctan2(cx-px,-(cy-py)))))
    angs.sort(reverse=descending)
    return angs

FIX_STEPS={
 "fluted":[-148,148], "smallchick":[-149,150],
 "attenSel":[-33,-2,31], "meter1":[-31,0,27], "meter2":[-31,0,26],
 "agcMode":[-44,-73,-101,-132], "scFilter1":[-130,-98,-72,-41], "scFilter2":[-131,-99,-73,-42],
}
# the step each control points at in the artwork -> angle filled in after steps known
BAKED_STEP={"lowCut":0,"lfFreq":0,"midFreq":5,"hfFreq":5,"highCut":0,"attenSel":2,"tc1":1,"tc2":1,
            "agcMode":1,"scFilter1":0,"scFilter2":0,"meter1":1,"meter2":1,
            "dcThr1":-1,"dcThr2":-1,"mix1":-1,"mix2":-1,"output1":-1,"output2":-1}
BAKED={}
# measured pointer axis (farthest silhouette point through the pivot) for the
# long-bar knobs - more exact than the label they point at in the artwork
BAKED_ABS={"lowCut":-156.7,"highCut":-153.4,"hfFreq":48.3,"lfFreq":-38.9,"midFreq":-3.1,"meter1":-3.4,"meter2":-3.5}
layout=[]
for name,(t,x0,y0) in CONTROLS.items():
    sil=silhouette(t,x0,y0)
    ys,xs=np.nonzero(sil)
    if len(xs)==0: print("EMPTY",name); continue
    # pivot = centre of the largest inscribed circle (the round body / nut)
    dist=cv2.distanceTransform(sil,cv2.DIST_L2,5)
    if t=="fluted":
        py,px=ys.mean(),xs.mean()
    else:
        py,px=np.unravel_index(np.argmax(dist),dist.shape)
    px,py=float(px),float(py)
    half=int(max(np.abs(xs-px).max(),np.abs(ys-py).max()))+4
    X0,Y0=int(round(px))-half,int(round(py))-half; S=2*half
    crop=src[Y0:Y0+S,X0:X0+S].copy(); a=sil[Y0:Y0+S,X0:X0+S].astype(np.float32)
    a=cv2.GaussianBlur(a,(0,0),0.7)
    # baked pointer angle = direction of the cream index line
    cream=(sil>0)&(src[...,2]>150)&(src[...,0]>100)&(chroma<10)
    if t=="fluted":
        ang=np.degrees(np.arctan2(xx-px,-(yy-py)))
        cream&=((xx-px)**2+(yy-py)**2>(0.55*half)**2)&(ang>-175)&(ang<-115)
    cy_,cx_=np.nonzero(cream)
    baked=float(np.degrees(np.arctan2(cx_.mean()-px,-(cy_.mean()-py)))) if len(cx_)>5 else 0.0
    cm=None
    if t=="fluted":
        # remove the baked index line (drawn at runtime instead)
        cm=cv2.dilate(cream[Y0:Y0+S,X0:X0+S].astype(np.uint8),np.ones((5,5),np.uint8))
        crop=cv2.inpaint(np.clip(crop,0,255).astype(np.uint8),cm*255,5,cv2.INPAINT_TELEA).astype(np.float32)
    K=1
    if src2 is not None:
        # 2x: take the pixels from the upscaled photo, upsample the matte
        K=2
        crop2=src2[2*Y0:2*Y0+2*S,2*X0:2*X0+2*S].copy()
        if t=="fluted":
            cm2=cv2.resize(cm,(2*S,2*S),interpolation=cv2.INTER_NEAREST)
            cm2=cv2.dilate(cm2,np.ones((5,5),np.uint8))
            crop2=cv2.inpaint(np.clip(crop2,0,255).astype(np.uint8),cm2*255,9,cv2.INPAINT_TELEA).astype(np.float32)
        a=np.clip(cv2.resize(a,(2*S,2*S),interpolation=cv2.INTER_CUBIC),0,1)
        crop=crop2
    rgba=np.dstack([np.clip(crop,0,255),np.clip(a*255,0,255)]).astype(np.uint8)
    cv2.imwrite(f'Resources/sprites/{name}.png',rgba)
    # shadow: blurred silhouette
    pad=16*K; SS=S*K; sh=np.zeros((SS+2*pad,SS+2*pad),np.float32); sh[pad:pad+SS,pad:pad+SS]=a
    sh=cv2.GaussianBlur(sh,(0,0),5.0*K)
    cv2.imwrite(f'Resources/sprites/{name}_sh.png',np.dstack([np.zeros_like(sh)]*3+[np.clip(sh*255,0,255)]).astype(np.uint8))

    # step / range angles from the printed scale
    steps=None
    if t=="fluted":   steps=label_angles(px,py,78,104,11)
    elif t=="chicken": steps=label_angles(px,py,38,80,None)
    elif t=="bar":     steps=label_angles(px,py,40,80,11)
    elif t=="smallchick": steps=label_angles(px,py,26,46,11)
    elif t=="meterknob":  steps=label_angles(px,py,50,85,3)
    elif t=="chicken_s":
        if name in ("agcMode",): steps=label_angles(px,py,30,60,4,descending=True,dots=True)
        elif name.startswith("scFilter"): steps=label_angles(px,py,25,60,4,dots=True)
        else: steps=label_angles(px,py,24,62,None)
    steps=FIX_STEPS.get(name,FIX_STEPS.get(t,steps))
    if name in BAKED_STEP and steps: BAKED[name]=steps[BAKED_STEP[name]]
    if name in BAKED_ABS: BAKED[name]=BAKED_ABS[name]
    if name in BAKED: baked=BAKED[name] if not isinstance(BAKED[name],int) or True else baked
    layout.append(dict(name=name,type=t,x=px,y=py,ox=X0,oy=Y0,size=S,baked=baked,steps=steps or []))
    print(f"{name:10s} {t:10s} pivot=({px:6.1f},{py:6.1f}) size={S:3d} baked={baked:7.1f} steps={len(steps or [])}: {[round(s) for s in (steps or [])]}")
json.dump(layout,open('Resources/layout.json','w'),indent=1)
# ---- C++ layout header ----
TYPES={"fluted":"Fluted","chicken":"Chicken","chicken_s":"Chicken","bar":"Chicken","smallchick":"Chicken","meterknob":"Chicken","toggle":"Toggle","screw":"Screw"}
with open('Source/UI/Layout.h','w') as f:
    f.write("// Generated by tools/make_assets.py from the panel artwork - do not edit.\n#pragma once\n#include <initializer_list>\n\nnamespace sa670::layout\n{\nenum class Kind { Fluted, Chicken, Toggle, Screw };\n")
    f.write("struct Control { const char* name; Kind kind; float x, y; int ox, oy, size; float baked; int numSteps; float steps[11]; };\n\n")
    f.write("inline constexpr Control controls[] = {\n")
    for c in layout:
        st=c['steps'][:11]; arr=", ".join(f"{a:.1f}f" for a in st) if st else "0"
        f.write(f'    {{ "{c["name"]}", Kind::{TYPES[c["type"]]}, {c["x"]:.1f}f, {c["y"]:.1f}f, {c["ox"]}, {c["oy"]}, {c["size"]}, {c["baked"]:.1f}f, {len(st)}, {{ {arr} }} }},\n')
    f.write("};\n} // namespace sa670::layout\n")
