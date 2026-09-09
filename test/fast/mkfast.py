#!/usr/bin/env python3
"""Generate a small 3-D fracture specimen that reproduces the wall classes in
minutes instead of hours.

The target deck s3rad has 42807 elements and takes ~2.5 h to reach its walls,
so no refactor of the damage module can be validated cheaply.  The other
decks in this tree (cohesive.inp, mixed.inp, snapback.inp) are 1-D chains:
they exercise the cohesive law and the continuation, but a 1-D chain cannot
produce a node hanging by one tetrahedron, which is the mechanism behind the
second s3rad wall.

This builds the smallest thing that can: a 3-D bar of tetrahedra, split by a
cohesive plane, with a brittle band across it to localise the crack, pulled
in displacement control.  Fragments arise naturally in a tet mesh once
deletion starts.

    ./mkfast.py -o fast.inp [--nx 8 --ny 5 --nz 5]
"""
import argparse,sys

def build(nx,ny,nz,lx,ly,lz,seed):
    """Bar of tetrahedra with a PARTIAL cohesive plane at mid-span.

    The plane is cohesive only over the first `seed` fraction of the y range;
    the rest stays continuous.  A full-section interface is the wrong model
    here - it fails first, the bulk unloads and never damages, and the run
    finishes at theta=1 with zero deletions (measured).  A partial one is a
    pre-crack: the remaining ligament has to tear through the bulk, which is
    what produces deletion, fragments and the wall classes."""
    dx,dy,dz=lx/nx,ly/ny,lz/nz
    mid=nx//2
    jseed=max(1,int(round(seed*ny)))          # cohesive rows 0..jseed-1
    def in_seed(j): return j<=jseed           # node row index
    nid={}; nodes=[]
    def add(i,j,k,side):
        # only nodes inside the seeded part of the mid plane are duplicated
        if not(i==mid and in_seed(j)): side=0
        key=(i,j,k,side)
        if key in nid:
            return nid[key]
        nid[key]=len(nodes)+1
        nodes.append((i*dx,j*dy,k*dz))
        return nid[key]
    def N(i,j,k,elem_i):
        side = 0 if elem_i<mid else 1
        return add(i,j,k,side)
    TETS=[(0,1,3,7),(0,1,7,5),(0,5,7,4),(0,3,2,7),(0,6,4,7),(0,2,6,7)]
    bulk=[]
    for i in range(nx):
        for j in range(ny):
            for k in range(nz):
                c=[N(i+(m&1),j+((m>>1)&1),k+((m>>2)&1),i) for m in range(8)]
                brittle = (jseed <= j < jseed+max(1,ny//4)) and (abs(i-mid)<=1)
                for t in TETS:
                    bulk.append(([c[t[0]],c[t[1]],c[t[2]],c[t[3]]],2 if brittle else 1))
    coh=[]
    for j in range(ny):
        if not(in_seed(j) and in_seed(j+1)): continue
        for k in range(nz):
            l=[add(mid,j+a,k+b,0) for a,b in ((0,0),(1,0),(1,1),(0,1))]
            r=[add(mid,j+a,k+b,1) for a,b in ((0,0),(1,0),(1,1),(0,1))]
            coh.append([l[0],l[1],l[2], r[0],r[1],r[2]])
            coh.append([l[0],l[2],l[3], r[0],r[2],r[3]])
    return nodes,bulk,coh,nid,mid

def write(path,nodes,bulk,coh,nid,nx,ny,nz,mid,lx,materials):
    matrix=[i+1 for i,(n,m) in enumerate(bulk) if m==1]
    plate =[i+1 for i,(n,m) in enumerate(bulk) if m==2]
    off=len(bulk)
    x0=sorted({v for (i,j,k,s),v in nid.items() if i==0})
    xl=sorted({v for (i,j,k,s),v in nid.items() if i==nx})
    def lines(vals,per=8):
        return "\n".join(", ".join(str(v) for v in vals[i:i+per]) for i in range(0,len(vals),per))
    with open(path,'w') as f:
        f.write("** Fast fracture regression specimen - see mkfast.py\n*Node\n")
        for i,(x,y,z) in enumerate(nodes): f.write("%d, %.6f, %.6f, %.6f\n"%(i+1,x,y,z))
        f.write("*Element, Type=C3D4\n")
        for i,(n,m) in enumerate(bulk): f.write("%d, %d, %d, %d, %d\n"%(i+1,*n))
        f.write("*User Element, Type=UC6, Nodes=6, Integration Points=3, MaxDof=3\n")
        f.write("*Element, Type=UC6\n")
        for i,n in enumerate(coh): f.write("%d, %d, %d, %d, %d, %d, %d\n"%(off+i+1,*n))
        f.write("*Elset, Elset=MATRIX\n%s\n"%lines(matrix))
        f.write("*Elset, Elset=PLATETANGENTIAL\n%s\n"%lines(plate))
        f.write("*Elset, Elset=INTERFACE\n%s\n"%lines([off+i+1 for i in range(len(coh))]))
        f.write("*Nset, Nset=FACE_X0_NSET\n%s\n"%lines(x0))
        f.write("*Nset, Nset=FACE_XL_NSET\n%s\n"%lines(xl))
        f.write("*Nset, Nset=FIXPOINTA\n%d\n"%x0[0])
        f.write("*Nset, Nset=FIXPOINTB\n%d\n"%x0[-1])
        f.write(materials)
        f.write("""*Step, Nlgeom, Inc=20000
*Static, Solver=Pardiso
1.000000e-03, 1., 1.000000e-09, 2.000000e-03
*Boundary
FACE_X0_NSET, 1, 1, 0.
FACE_XL_NSET, 1, 1, %.4f
FIXPOINTA, 2, 3, 0.
FIXPOINTB, 3, 3, 0.
*Output, Frequency=50
*Node File
U, RF
*El File
S, E, PEEQ, SDV
*Node Print, Nset=FACE_XL_NSET, Totals=Yes, Frequency=50
RF, U
*End Step
"""%(0.25*lx))

def lift_materials(src):
    """Copy the material and section cards verbatim from the target deck, so
    this specimen can never drift from the physics it is meant to stand in
    for.  Only the element-set names on the *Solid/User Section cards are
    reused; the mesh above defines those sets itself."""
    txt=open(src,errors='replace').read()
    a=txt.index('*Material, Name=ZR')
    b=txt.index('*Step,')
    block=txt[a:b]
    # this specimen has a single INTERFACE set; the target deck has two, so
    # drop the seed section entirely (card plus its constants line) and point
    # the regular one at our set.
    out=[];skip=0
    for ln in block.split('\n'):
        if skip: skip-=1; continue
        if 'Elset=INTERFACE_SEED' in ln: skip=1; continue
        out.append(ln.replace('Elset=INTERFACE_REGULAR','Elset=INTERFACE'))
    return '\n'.join(out)

if __name__=='__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('-o',required=True); ap.add_argument('--nx',type=int,default=8)
    ap.add_argument('--ny',type=int,default=5); ap.add_argument('--nz',type=int,default=5)
    ap.add_argument('--lx',type=float,default=4.0); ap.add_argument('--ly',type=float,default=2.0)
    ap.add_argument('--lz',type=float,default=2.0)
    ap.add_argument('--from-deck',default='test/s3rad/m12_s3rad_gc24_w.inp')
    ap.add_argument('--seed',type=float,default=0.5,help='fraction of the mid plane that is a pre-crack')
    a=ap.parse_args()
    nodes,bulk,coh,nid,mid=build(a.nx,a.ny,a.nz,a.lx,a.ly,a.lz,a.seed)
    mats=lift_materials(a.from_deck)
    write(a.o,nodes,bulk,coh,nid,a.nx,a.ny,a.nz,mid,a.lx,mats)
    print("wrote %s: %d nodes, %d C3D4, %d UC6"%(a.o,len(nodes),len(bulk),len(coh)))
