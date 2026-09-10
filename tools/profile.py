#!/usr/bin/env python3
"""Where does the run's time go?  Run one gate case under the event timer.

    CCX_EXE=/path/to/ccx_2.23_pardiso tools/profile.py fast-wrapped

Reuses test/regress/cases.json so a profile is taken of a case whose purpose
somebody has written down, and pins OMP_NUM_THREADS/MKL_NUM_THREADS to 1 by
default: a timing comparison across thread counts is meaningless here, and so
is a timing comparison against a machine that was busy doing something else.

Cases are run ONE AT A TIME on purpose.  test/regress/run.py runs them
concurrently, which is right for a pass/fail gate and wrong for a stopwatch.

The solver's own instrument (src/logview.c) does the attribution; this script
only arranges the run, refuses to believe a perturbed one, and prints the
result.  --json writes the record so two arms can be diffed.
"""
import argparse,json,os,pathlib,re,shutil,subprocess,sys,time

HERE=pathlib.Path(__file__).resolve().parent
ROOT=HERE.parent
sys.path.insert(0,str(ROOT/'test/regress'))

def load_cases():
    return {c['name']:c for c in json.load(open(ROOT/'test/regress/cases.json'))['cases']}

def run(case,rundir,exe,threads,extra_env):
    env=dict(os.environ)
    env['OMP_NUM_THREADS']=str(threads); env['MKL_NUM_THREADS']=str(threads)
    env['CCX_LOG_VIEW']='1'; env['CCX_EXE']=exe
    for kv in case['env']+extra_env:
        k,_,v=kv.partition('='); env[k]=v
    rundir.mkdir(parents=True,exist_ok=True)
    if case['kind']=='fast':
        env['FAST_VARIANT']=case['variant']
        cmd='%s/test/fast/run_fast.sh %s'%(ROOT,rundir)
        cwd=None
    elif case['kind']=='close':
        subprocess.run('python3 %s/test/pathfollow/mkclose.py -o %s'
                       %(ROOT,rundir/'close.inp'),shell=True,check=True)
        env.setdefault('CCX_DAMAGE_AUTOSPC','1.e-3')
        cmd='%s -i close > run.log 2>&1'%exe; cwd=rundir
    else:
        shutil.copy(ROOT/'test/pathfollow/mixed.inp',rundir/'mixed.inp')
        env.setdefault('CCX_DAMAGE_AUTOSPC','1.e-3')
        cmd='%s -i mixed > run.log 2>&1'%exe; cwd=rundir
    t0=time.time()
    subprocess.run(cmd,shell=True,env=env,cwd=cwd,stdout=subprocess.DEVNULL,
                   stderr=subprocess.STDOUT)
    return time.time()-t0,rundir/'run.log'

def parse(log):
    txt=open(log,errors='replace').read()
    m=re.search(r'^\[LOGVIEW_JSON\] (.*)$',txt,re.M)
    if not m:
        bad=re.findall(r'^\[LOGVIEW\].*$',txt,re.M)
        raise SystemExit("no profile in %s%s"%(log,
            "\n  "+"\n  ".join(bad) if bad else
            "\n  the run printed no [LOGVIEW] block at all: is CCX_LOG_VIEW read "
            "by this binary?  check the [SWITCHES] banner."))
    return json.loads(m.group(1))

def show(name,prof,wall):
    ev=sorted(prof['events'],key=lambda e:-e['self'])
    tot=prof['total']
    acc=sum(e['self'] for e in ev)
    print("\n%s   %.2f s measured by the solver, %.2f s of wall clock"%(name,tot,wall))
    print("  %-28s %8s %10s %10s %7s %10s"%("event","calls","incl (s)","self (s)","%run","ms/call"))
    for e in ev:
        print("  %-28s %8d %10.3f %10.3f %6.1f%% %10.2f"
              %(e['name'],e['calls'],e['incl'],e['self'],100.*e['self']/tot,
                1000.*e['incl']/e['calls']))
    print("  %-28s %8s %10s %10.3f %6.1f%%"
          %("not instrumented","","",tot-acc,100.*(tot-acc)/tot))

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('case',nargs='+',help='case name(s) from test/regress/cases.json')
    ap.add_argument('-t','--threads',type=int,default=1)
    ap.add_argument('-e','--env',action='append',default=[],metavar='K=V',
                    help='extra environment, e.g. CCX_PARDISO_REUSE_SYMBOLIC=1')
    ap.add_argument('-o',default=None,help='where to put the runs')
    ap.add_argument('--json',default=None,help='write the profiles here')
    a=ap.parse_args()
    exe=os.environ.get('CCX_EXE')
    if not exe or not os.access(exe,os.X_OK):
        sys.exit("set CCX_EXE to a PARDISO-enabled ccx_2.23 binary")
    cases=load_cases()
    outroot=pathlib.Path(a.o or (ROOT/'test/regress/_runs'/
             ('profile-'+time.strftime('%Y%m%d-%H%M%S')))).resolve()
    out={}
    for name in a.case:
        if name not in cases: sys.exit("no such case: %s"%name)
        wall,log=run(cases[name],outroot/name,exe,a.threads,a.env)
        prof=parse(log)
        prof['wall']=wall; prof['threads']=a.threads; prof['env']=a.env
        out[name]=prof
        show(name,prof,wall)
    if a.json:
        pathlib.Path(a.json).write_text(json.dumps(out,indent=2,sort_keys=True)+"\n")
        print("\nwrote %s"%a.json)

if __name__=='__main__': main()
