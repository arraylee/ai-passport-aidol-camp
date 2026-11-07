"""Compile the production poll function with deterministic asynchronous queue stubs."""
from pathlib import Path
import subprocess,tempfile,os
ROOT=Path(__file__).resolve().parents[1]
s=(ROOT/'main/hr_platform.c').read_text();start=s.index('void hr_radio_poll(');end=s.index('{',start)+1;depth=1
while depth:
    depth+=(s[end]=='{')-(s[end]=='}');end+=1
with tempfile.TemporaryDirectory(prefix='hr-poll-test-') as td:
    p=Path(td);(p/'production_poll.inc').write_text(s[start:end])
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I',str(ROOT/'main'),'-I',str(p),str(ROOT/'tests/host/poll_fixture.c'),str(ROOT/'main/hr_core.c'),str(ROOT/'main/hr_identity.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
