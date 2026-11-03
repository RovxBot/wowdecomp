"""Create the extract-beside-Data distribution from a qualified application."""
from pathlib import Path
import shutil
import tarfile
import tempfile

from prepare import HERE, digest


def desktop_entry(script):
    # %k is supplied as one argument by the desktop, including URI paths.
    bootstrap = ("import pathlib,subprocess,sys,urllib.parse; "
                 "s=sys.argv[1]; "
                 "p=pathlib.Path(urllib.parse.unquote(urllib.parse.urlparse(s).path) if s.startswith('file:') else s).resolve().parent; "
                 "sys.exit(subprocess.call([str(p/'" + script + "')]))")
    return ('[Desktop Entry]\nType=Application\nVersion=1.0\n'
            'Name=Play WoW Native TEST-SERVER\nIcon=applications-games\n'
            'Comment=Native 3.3.5a Linux alpha for compatible isolated servers\n'
            'Terminal=true\nCategories=Game;\nExec=python3 -I -B -c "' + bootstrap + '" %k\n')


def distribution(application):
    name = application.stem
    script = 'Launch ' + name + '.sh'
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        shutil.copy2(application, root / application.name)
        shutil.copy2(application.with_suffix('.AppImage.sha256'), root / (application.name + '.sha256'))
        shutil.copy2(HERE / 'ALPHA_README.txt', root / 'README-Linux.txt')
        for file in ['manifest.json', 'source-audit.json', 'upstream.json']:
            shutil.copy2(application.parent / 'AppDir' / file, root / file)
        (root / script).write_text(
            '#!/bin/sh\nset -u\numask 077\n'
            'application_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
            'export APPIMAGE_EXTRACT_AND_RUN=1\n'
            '"$application_dir/' + application.name + '" "$@"\n'
            'result=$?\nif [ "$result" -ne 0 ] && [ -t 0 ]; then\n'
            '  printf "%s\\n" "Launch failed. See the message and session log above. Press Enter to close." >&2\n'
            '  read -r reply\nfi\nexit "$result"\n')
        (root / script).chmod(0o755)
        desktop = root / 'Play WoW TEST-SERVER.desktop'
        desktop.write_text(desktop_entry(script))
        desktop.chmod(0o755)
        archive = application.parent / (name + '.tar.gz')
        # Files extract directly beside Data, with executable mode preserved.
        with tarfile.open(archive, 'w:gz') as output:
            for path in sorted(root.iterdir()):
                output.add(path, arcname=path.name)
    archive.with_name(archive.name + '.sha256').write_text(digest(archive) + '  ' + archive.name + '\n')
    return archive
