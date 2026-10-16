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

## Publish with GitHub Pages

In repository **Settings → Pages → Build and deployment**, select **GitHub
Actions** as the source. No `gh-pages` branch or committed generated HTML is
needed.

The **Generate documentation** workflow builds and deploys the site on pushes to
`master`, including merged pull requests. It also supports manual runs from the
Actions tab; select `master` to publish. Manual runs on other branches only build
artifacts. The `github-pages` environment must allow deployments from `master`.

After a successful deployment, the site is available at
<https://christophertubbs.github.io/owplog/>. The deployment job also exposes the
site URL in the Actions run. You can link to this URL from the repository's About
website field, README, or wiki.

The complete HTML site remains available as the `owplog-documentation` download
artifact for 30 days. Extract it before opening `index.html`. The generation
helper creates the output directory on fresh checkouts and includes `.nojekyll`
for compatibility with GitHub Pages hosting.
