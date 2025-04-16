from pathlib import Path
import sys

import west.configuration
import pytest

from twister_harness.helpers.domains_helper import get_default_domain_name

WEST_TOPDIR = Path(west.configuration.west_dir()).parent
sys.path.insert(0, str(WEST_TOPDIR / 'zephyr' / 'scripts' / 'west_commands'))
from runners.core import BuildConfiguration

@pytest.fixture(scope='session')
def anyio_backend():
    return 'trio'

@pytest.fixture(scope='session')
def build_conf(request):
    build_dir = Path(request.config.option.build_dir)
    domains = build_dir / 'domains.yaml'
    app_build_dir = build_dir / get_default_domain_name(domains)
    return BuildConfiguration(str(app_build_dir))
