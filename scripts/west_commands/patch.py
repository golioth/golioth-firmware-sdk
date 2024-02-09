from pathlib import Path
import subprocess
from textwrap import dedent

from west import log
from west.manifest import MANIFEST_PROJECT_INDEX
from west.app.project import _ProjectCommand

class Patch(_ProjectCommand):
    def __init__(self):
        super().__init__(
            'patch',
            'patch west subprojects',
            dedent('''
            Runs "git am <west_manifest_repo>/patches/<project>/*.patch" on each of the specified projects.

            Custom patches directories (instead of patches/) can be configured in west manifest:

            manifest:
              self:
                userdata:
                  patches_dirs:
                    - patches_1
                    - patches_2'''))

    def do_add_parser(self, parser_adder):
        parser = self._parser(parser_adder)
        parser.add_argument('-a', '--apply', action='store_true',
                            help='use "git apply" instead of "git am"')
        parser.add_argument('projects', metavar='PROJECT', nargs='*',
                            help='''projects (by name or path) to operate on;
                            defaults to active cloned projects''')
        return parser

    def do_run(self, args, ignored):
        manifest_project = self.manifest.projects[MANIFEST_PROJECT_INDEX]
        manifest_project_path = Path(manifest_project.abspath)
        am = 'apply' if args.apply else 'am --keep-cr'
        failed = []
        for project in self._cloned_projects(args):
            patches = []

            try:
                patches_dirs = manifest_project.userdata['patches_dirs']
            except (TypeError, AttributeError, KeyError):
                patches_dirs = ['patches']

            for patches_dir in patches_dirs:
                patches_path = manifest_project_path / patches_dir / project.name
                if not patches_path.exists():
                    continue

                patches += sorted(filter(Path.is_file, patches_path.glob('*.patch')))

            if not patches:
                continue

            log.banner(f'patching {project.name_and_path}):')

            for patch in patches:
                try:
                    log.inf(f'git {am} {patch.relative_to(manifest_project_path)}')
                    project.git(am.split() + [str(patch)])
                except subprocess.CalledProcessError:
                    failed.append(project)
                    continue
        self._handle_failed(args, failed)
