"""Compile and qualify the native 12340 packet prerequisite subset in Actions."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
from audit_build_graph import audit, read_targets

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--preset',choices=['linux-packets','linux-packets-asan','macos-arm64-packets','windows-packets'],required=True)
    args=parser.parse_args()
    if os.environ.get('GITHUB_ACTIONS')!='true':
        raise SystemExit('Client/dependency compilation must run in GitHub Actions.')
    root=Path(__file__).resolve().parents[2];os.chdir(root)
    build=root/'build'/args.preset;evidence=root/'artifacts'/args.preset;evidence.mkdir(parents=True,exist_ok=True)
    target_os,cpu={'linux-packets':('LINUX','X86_64'),'linux-packets-asan':('LINUX','X86_64'),
        'macos-arm64-packets':('MACOS','ARM64'),'windows-packets':('WINDOWS','X86_64')}[args.preset]
    manifest=dict(schema_version=1,scope='M3 native 12340 packet buffers, ARC4 and SHA1/HMAC prerequisites; no sockets/login/gameplay',
        source_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),preset=args.preset,
        workflow_run=f"{os.environ['GITHUB_SERVER_URL']}/{os.environ['GITHUB_REPOSITORY']}/actions/runs/{os.environ['GITHUB_RUN_ID']}",
        target_os=target_os,target_cpu=cpu,host_machine=platform.machine(),runner_arch=os.environ.get('RUNNER_ARCH'),
        runner_image=os.environ.get('ImageVersion'),status='preflight')
    def run(name,command):
        manifest['status']=name
        with (evidence/(name+'.log')).open('w') as log:
            process=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1)
            for line in process.stdout:sys.stdout.write(line);log.write(line)
            if process.wait():raise RuntimeError(name+' failed')
    try:
        if cpu=='ARM64' and (platform.machine()!='arm64' or os.environ.get('RUNNER_ARCH')!='ARM64'):
            raise RuntimeError('Packet qualification requires an actual ARM64 macOS runner.')
        query=build/'.cmake/api/v1/query/codemodel-v2';query.parent.mkdir(parents=True,exist_ok=True);query.touch()
        run('cmake-version',['cmake','--version']);run('configure',['cmake','--preset',args.preset])
        targets=read_targets(build);errors=audit(targets,'packets',target_os,cpu)
        (evidence/'build-graph.json').write_text(json.dumps(dict(errors=errors,targets=targets),indent=2)+'\n')
        if errors:raise RuntimeError('\n'.join(errors))
        cache=(build/'CMakeCache.txt').read_text()
        manifest['toolchain_cache']=[line for line in cache.splitlines() if line.startswith(('CMAKE_C_COMPILER:','CMAKE_CXX_COMPILER:','CMAKE_BUILD_TYPE:','CMAKE_GENERATOR:','WOWDECOMP_'))]
        run('build',['cmake','--build',str(build),'--config','Release','--parallel','2'])
        run('tests',['ctest','--test-dir',str(build),'-C','Release','--output-on-failure','--no-tests=error'])
        (evidence/'bin').mkdir(exist_ok=True);manifest['binaries']=[]
        for name in ['NativePacketCheck','NativeCore','CoreServicesCheck','PortableCoreCheck']:
            actual=name+('.exe' if target_os=='WINDOWS' else '')
            found=list((build/'bin').rglob(actual))
            if len(found)!=1:raise RuntimeError('Expected one packaged '+actual)
            destination=evidence/'bin'/actual;shutil.copy2(found[0],destination)
            manifest['binaries'].append(dict(name=actual,sha256=hashlib.sha256(destination.read_bytes()).hexdigest()))
        manifest['status']='Hosted native packet semantics passed; transport/authentication/gameplay remain open'
    except Exception as error:
        manifest['failed_phase']=manifest['status'];manifest['status']='failed';manifest['failure']=str(error);raise
    finally:
        detail=build/'Testing/Temporary/LastTest.log'
        if detail.exists():shutil.copy2(detail,evidence/'ctest-detail.log')
        (evidence/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
if __name__=='__main__':main()
