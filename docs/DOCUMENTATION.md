# Generate the documentation

Install Doxygen 1.9.8 or newer and Graphviz. On Ubuntu:

```bash
sudo apt-get install doxygen graphviz
```

From the repository root, run:

```bash
bash scripts/docs.sh
```

Open `build/docs/html/index.html` in a browser. The site includes searchable API
references, source browsing, architecture and interoperability guides, and SVG
class diagrams. Generated files stay in the ignored `build/` directory. Review
`build/docs/warnings.log` for documentation diagnostics; existing incomplete
comments do not prevent generation.
The helper also copies the README illustration to its expected relative path.

The vendored [Doxygen Awesome](https://github.com/jothepro/doxygen-awesome-css)
v2.3.4 stylesheet provides responsive layouts and automatic light/dark styling
based on the reader's system preference. It is a documentation-only dependency,
licensed under MIT; see [theme/LICENSE](theme/LICENSE). The upstream revision is
`568f56cde6ac78b6dfcc14acd380b2e745c301ea`. To update it, replace
`docs/theme/doxygen-awesome.css` and its license from a reviewed upstream release.

The **Generate documentation** GitHub Actions workflow runs when a pull request
into `master` is merged. Closed, unmerged PRs are skipped. It builds the merge
commit and uploads the complete HTML site as the `owplog-documentation` artifact,
available from the workflow run for 30 days. Download and extract the artifact
before opening `index.html`. The workflow also supports manual runs from the
Actions tab. It does not publish to GitHub Pages.
