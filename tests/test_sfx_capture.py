"""Exercise the actual mic worker with its synthesized speaker feedback."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'main/hr_platform.c').read_text()
a=source.index('static void capture_task(');b=source.index('\nstatic void playback_task(',a)
with tempfile.TemporaryDirectory(prefix='hr-sfx-capture-') as tmp:
 p=Path(tmp);(p/'capture.inc').write_text(source[a:b])
 (p/'playback.inc').write_text(source[b:source.index('\nbool hr_audio_pause(',b)])
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I',str(root/'main'),'-I',str(p),str(root/'tests/host/sfx_capture_fixture.c'),str(root/'main/hr_sfx.c'),str(root/'main/hr_game.c'),str(root/'main/hr_core.c'),str(root/'main/hr_codec.c'),str(root/'main/hr_welcome.c'),'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,timeout=10)
