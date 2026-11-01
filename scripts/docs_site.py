"""Keep the site's paths aligned with the repository's existing Markdown."""

from pathlib import Path

from mkdocs.config.defaults import MkDocsConfig
from mkdocs.structure.files import File, Files


def on_files(files: Files, config: MkDocsConfig) -> Files:
    root = str(Path(__file__).resolve().parent.parent)
    result = Files([])
    for source in files:
        if source.src_dir == config.docs_dir:
            result.append(
                File(
                    "docs/" + source.src_uri,
                    root,
                    config.site_dir,
                    config.use_directory_urls,
                    inclusion=source.inclusion,
                )
            )
        else:
            result.append(source)
    for name in (
        "README.md",
        "CONTRIBUTING.md",
        "CHANGELOG.md",
        "SECURITY.md",
        "CODE_OF_CONDUCT.md",
        "LICENSE",
    ):
        result.append(File(name, root, config.site_dir, config.use_directory_urls))
    return result
