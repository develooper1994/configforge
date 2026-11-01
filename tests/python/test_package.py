"""Check an installed wheel, then run the binding suite against that install.

    python -I tests/python/test_package.py
"""

import importlib.metadata
import os
from pathlib import Path
import subprocess
import sys
import tempfile

os.environ.pop("CONFIGFORGE_LIB", None)

import configforge as cf  # noqa: E402
import configforge_doctor  # noqa: E402,F401


def main() -> None:
    package = Path(cf.__file__).resolve().parent
    library = Path(cf._lib._name).resolve()
    assert library.is_file() and library.parent == package, library
    assert (package / "py.typed").is_file()
    distribution = importlib.metadata.distribution("configforge")
    assert distribution.version == cf.__version__
    assert cf._lib.cf_version().decode("ascii") == distribution.version
    files = distribution.files
    assert files and any(path.name == "LICENSE" for path in files)
    assert package.parent == Path(configforge_doctor.__file__).resolve().parent

    with tempfile.TemporaryDirectory(prefix="configforge-package-") as work:
        subprocess.run(
            [
                sys.executable,
                "-I",
                str(Path(__file__).with_name("test_binding.py").resolve()),
                str(package.parent),
                work,
            ],
            cwd=work,
            check=True,
        )
    print("Installed package: bundled library, version, licence, typing and binding suite passed")


if __name__ == "__main__":
    main()
