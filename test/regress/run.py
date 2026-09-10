#!/usr/bin/env python3
"""Run every regression that finishes in minutes and compare it to the record.

    CCX_EXE=/path/to/ccx_2.23_pardiso test/regress/run.py [-j N] [-k NAME]

Exit status is the number of failed cases, so it is usable from a hook or a
CI step.  Cases are defined in cases.json, which also carries what each one
is FOR - a case whose purpose nobody can state is a case nobody will fix.

Why this exists: the architecture audit's finding was that validation cost
2.5 hours, so nothing could be checked cheaply and every fix was a point fix
made blind.  These cases cover the load-path judgement, the crack-face kink,
bulk damage with deletion and cutbacks, and the analytical mixed-mode branch,
and they run on one core in the time it takes to read a diff.
"""
import argparse,concurrent.futures as cf,json,os,pathlib,re,shutil,subprocess,sys,time

HERE=pathlib.Path(__file__).resolve().parent
ROOT=HERE.parent.parent

def sh(cmd,env,cwd=None,timeout=3600):
    return subprocess.run(cmd,shell=True,env=env,cwd=cwd,timeout=timeout,
                          stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)

def base_env(extra):
    e=dict(os.environ)
    e.setdefault('OMP_NUM_THREADS','1'); e.setdefault('MKL_NUM_THREADS','1')
    for kv in extra:
        k,_,v=kv.partition('='); e[k]=v
    return e

def last_sta(p):
    try: lines=[l for l in open(p) if l.strip()]
    except OSError: return None,None
    if not lines: return None,None
    f=lines[-1].split()
    return int(f[1]),f[4]

def ndel(p):
    try: return sum(1 for l in open(p) if not l.startswith('#') and l.strip())
    except OSError: return None

def delset(p):
    try: return {l.split()[0] for l in open(p) if not l.startswith('#') and l.strip()}
    except OSError: return set()

def census(log):
    """max nodes masked at the standard threshold, and the worst ratio."""
    mx=0; worst=None
    try: txt=open(log,errors='replace').read()
    except OSError: return 0,None
    for m in re.finditer(r'below_1e-3=(\d+)',txt): mx=max(mx,int(m.group(1)))
    for m in re.finditer(r'worst=node_\d+_at_([0-9.eE+-]+)',txt):
        v=float(m.group(1))
        if worst is None or v<worst: worst=v
    return mx,worst

def selftests(log,required):
    """every named self test must report PASSED and none may report a failure."""
    try: txt=open(log,errors='replace').read()
    except OSError: return ["no log"]
    bad=[]
    for name in required:
        if not re.search(re.escape(name)+r'.*(PASSED|0 failure)',txt):
            if name in txt: bad.append("%s did not report PASSED"%name)
    for m in re.finditer(r'\[([A-Z0-9 _]+)\][^\n]*?([1-9]\d*) failure',txt):
        bad.append("%s reported %s failure(s)"%(m.group(1),m.group(2)))
    return bad

def run_fast(case,rundir,exe):
    env=base_env(case['env']); env['FAST_VARIANT']=case['variant']; env['CCX_EXE']=exe
    r=sh('%s/test/fast/run_fast.sh %s'%(ROOT,rundir),env)
    return r.returncode,rundir/'run.log',rundir/'m.sta',rundir/'m.damage'

def run_mixed(case,rundir,exe):
    rundir.mkdir(parents=True,exist_ok=True)
    shutil.copy(ROOT/'test/pathfollow/mixed.inp',rundir/'mixed.inp')
    env=base_env(case['env']); env.setdefault('CCX_DAMAGE_AUTOSPC','1.e-3')
    r=sh('%s -i mixed > run.log 2>&1'%exe,env,cwd=rundir)
    return r.returncode,rundir/'run.log',rundir/'mixed.sta',None

def one(case,outroot,exe,required):
    rundir=outroot/case['name']
    if rundir.exists(): shutil.rmtree(rundir)
    t0=time.time()
    if case['kind']=='fast': rc,log,sta,dam=run_fast(case,rundir,exe)
    else:                    rc,log,sta,dam=run_mixed(case,rundir,exe)
    got={'rc':rc}
    inc,theta=last_sta(sta)
    got['last_inc'],got['theta']=inc,theta
    if dam is not None: got['deletions']=ndel(dam)
    exp=case['expect']
    if 'masked_max' in exp or 'worst_ratio' in exp:
        got['masked_max'],got['worst_ratio']=census(log)
    if 'check_mixed' in exp:
        r=sh('python3 %s/test/pathfollow/check_mixed.py %s'%(ROOT,rundir),base_env([]))
        got['check_mixed']='PASSED' if 'PASSED' in r.stdout else 'FAILED'
        m=re.search(r'(\d+) accepted increments, (\d+) of them post-peak',r.stdout)
        if m: got['accepted'],got['postpeak']=int(m.group(1)),int(m.group(2))
    fails=[]
    for k,want in exp.items():
        have=got.get(k)
        if k=='worst_ratio':
            ok = have is not None and abs(have-want)<=1e-4*abs(want)
        else:
            ok = have==want
        if not ok: fails.append("%s: want %r, got %r"%(k,want,have))
    fails+=selftests(log,required)
    return {'name':case['name'],'what':case['what'],'seconds':round(time.time()-t0,1),
            'got':got,'fails':fails,'rundir':str(rundir)}

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('-j',type=int,default=2,help='cases to run at once')
    ap.add_argument('-k',default=None,help='run only cases whose name contains this')
    ap.add_argument('-o',default=None,help='where to put the runs')
    a=ap.parse_args()
    exe=os.environ.get('CCX_EXE')
    if not exe or not os.access(exe,os.X_OK):
        sys.exit("set CCX_EXE to a PARDISO-enabled ccx_2.23 binary")
    spec=json.load(open(HERE/'cases.json'))
    cases=[c for c in spec['cases'] if not a.k or a.k in c['name']]
    outroot=pathlib.Path(a.o or (HERE/'_runs'/time.strftime('%Y%m%d-%H%M%S'))).resolve()
    outroot.mkdir(parents=True,exist_ok=True)
    print("binary %s\nruns   %s\ncases  %d, %d at a time\n"%(exe,outroot,len(cases),a.j))
    res={}
    with cf.ThreadPoolExecutor(max_workers=a.j) as ex:
        futs={ex.submit(one,c,outroot,exe,spec['selftests_required']):c for c in cases}
        for f in cf.as_completed(futs):
            r=f.result(); res[r['name']]=r
            print("  %-24s %6.1fs  %s"%(r['name'],r['seconds'],
                  "ok" if not r['fails'] else "FAILED"))
    # cross-case relations, checked after everything has run
    for c in cases:
        r=res.get(c['name'])
        if r is None: continue
        other=c.get('same_deletion_set_as')
        if other and other in res:
            A=delset(pathlib.Path(res[other]['rundir'])/'m.damage')
            B=delset(pathlib.Path(r['rundir'])/'m.damage')
            if A!=B: r['fails'].append("deletion set differs from %s by %d element(s)"
                                       %(other,len(A^B)))
        other=c.get('identical_sta_as')
        if other and other in res:
            A=(pathlib.Path(res[other]['rundir'])/'mixed.sta').read_bytes()
            B=(pathlib.Path(r['rundir'])/'mixed.sta').read_bytes()
            if A!=B: r['fails'].append("mixed.sta is not identical to %s"%other)
    print()
    nbad=0
    for c in cases:
        r=res[c['name']]
        print("%-24s %s"%(r['name'],r['what']))
        print("   %s"%"  ".join("%s=%s"%(k,v) for k,v in r['got'].items()))
        if r['fails']:
            nbad+=1
            for f in r['fails']: print("   FAIL %s"%f)
        else:
            print("   ok")
    print("\n%d of %d case(s) failed"%(nbad,len(cases)))
    return nbad

if __name__=='__main__':
    sys.exit(main())
