import cv2, numpy as np, sys, json
img = cv2.imread(sys.argv[1]); H,W = img.shape[:2]
hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)
v = hsv[...,2].astype(float)
dark = (v < 60).astype(np.uint8)
def refine(x,y,r):
    x0,y0=int(x-r*1.3),int(y-r*1.3); x1,y1=int(x+r*1.3),int(y+r*1.3)
    m = dark[y0:y1,x0:x1]
    n,lab,stats,cent = cv2.connectedComponentsWithStats(m)
    best=None
    for i in range(1,n):
        cx,cy=cent[i]; a=stats[i,4]
        d=np.hypot(cx+x0-x,cy+y0-y)
        if a>r*r and (best is None or d<best[0]): best=(d,cx+x0,cy+y0,a)
    return best
def pointer(x,y,r):
    # bright cream pixels inside knob radius
    ys,xs=np.mgrid[0:H,0:W]
    sel=((xs-x)**2+(ys-y)**2<(r*0.98)**2)&((xs-x)**2+(ys-y)**2>(r*0.25)**2)
    b=img[...,2].astype(int); g=img[...,1].astype(int); bl=img[...,0].astype(int)
    cream=sel&(b>150)&(g>130)&(bl>90)
    yy,xx=np.nonzero(cream)
    if len(xx)==0: return None
    ang=np.degrees(np.arctan2(xx.mean()-x, -(yy.mean()-y)))
    return round(float(ang),1), int(len(xx))
pts=json.loads(sys.argv[2])
for name,(x,y,r) in pts.items():
    b=refine(x,y,r)
    if b: cx,cy=b[1],b[2]
    else: cx,cy=x,y
    print(name, round(cx,1), round(cy,1), b and int(b[3]), pointer(cx,cy,r))
