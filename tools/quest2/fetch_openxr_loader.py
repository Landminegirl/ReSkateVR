import urllib.request,hashlib,zipfile
from pathlib import Path
p=Path('openxr_loader_windows-1.1.63.zip')
url='https://github.com/KhronosGroup/OpenXR-SDK-Source/releases/download/release-1.1.63/openxr_loader_windows-1.1.63.zip'
if not p.exists(): urllib.request.urlretrieve(url,p)
assert hashlib.sha256(p.read_bytes()).hexdigest()=='01c631aeabbfe0879540f77ef833416c532a20746285b494630160c23588b771'
with zipfile.ZipFile(p) as z:
 b=z.read('x64/bin/openxr_loader.dll')
assert hashlib.sha256(b).hexdigest()=='66946ea6e9cc2649e299ec412521c5eafe56c635d05055097fe4db81e11932df'
out=Path('dist');out.mkdir(exist_ok=True)
(out/'openxr_loader.dll').write_bytes(b)
print('Official OpenXR loader archive and x64 DLL hashes verified.')
