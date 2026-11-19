# The market for configuration tools

Most people who change configuration do not write the program that
reads it. They are operations staff, site reliability engineers, IT
staff, release engineers and embedded integrators. They edit a file, a
key in a store or a build option, and they need to know what that change
does before it reaches a machine. ConfigForge means to be a lens over the
stores they already use (git, files, Consul, etcd), not a store itself.
The plan for this is in [Roadmap.md](Roadmap.md), section "For the people
who change configuration".

This page records what exists today and where a field looks open. It is
research from 2026-11-19, kept so that the decisions on the roadmap rest
on something. Rows marked "(unconfirmed)" come from memory or secondary
pages and have not been checked against the tool's own site.

## How large the problem is

- **Yin et al., SOSP 2011.** 546 real misconfigurations were studied:
  309 from a commercial storage system deployed at thousands of
  customers, and 237 from four open source systems. 70.0% to 85.5% were
  mistakes in setting parameters.
  [NetApp page](https://www.netapp.com/atg/publications/publications-an-empirical-study-on-configuration-errors-in-commercial-and-open-source-systems-20115823),
  [Illinois repository](https://www.ideals.illinois.edu/items/31156).
  Two later papers quote "27% of customer cases" as configuration
  related; the paper's own wording was not found (unconfirmed).
- **Google SRE Workbook, appendix C.** In postmortems from 2010 to 2017,
  a configuration push triggered 31% of outages and a binary push 37%.
  [Table C-1](https://sre.google/workbook/postmortem-analysis/). The
  often quoted "about 70% of outages come from changes to a live system"
  is from the SRE book; the page with that sentence was not found
  (unconfirmed).
- **CrowdStrike, 19 July 2024.** A content update (Channel File 291) got
  through a content validator with a logic error, and Microsoft
  estimated 8.5 million Windows devices were affected.
  [Root cause analysis](https://www.crowdstrike.com/wp-content/uploads/2024/08/Channel-File-291-Incident-Root-Cause-Analysis-08.06.2024.pdf),
  [TechTarget](https://techtarget.com/searchsecurity/news/366602392/CrowdStrike-details-errors-that-led-to-mass-IT-outage).
- **Cloudflare, 18 November 2025.** A generated Bot Management feature
  file doubled in size and passed the proxy's limit of 200 features. The
  result was HTTP 5xx errors from about 11:20 to 17:06 UTC, nearly six
  hours. Cloudflare now hardens its own configuration files "in the same
  way" as user input.
  [Cloudflare post](https://blog.cloudflare.com/18-november-2025-outage/).
- **Meta, 4 October 2021.** An outage of about six hours, attributed to a
  faulty configuration change on backbone routers.
  [Report in The Star](https://www.thestar.com.my/tech/tech-news/2021/10/05/facebook-says-root-cause-of-outage-was-faulty-configuration-change).
  This is a secondary source; Meta's own post was not read.
- **Xu et al., FSE 2015.** Systems carry far more parameters than users
  need: for parameters with many options, real users typically use two or
  three.
  [Summary](https://neverworkintheory.org/2016/06/09/too-many-knobs.html).
  The parameter counts in the paper were not read (unconfirmed).
- **arXiv 2024, "Rethinking software misconfigurations in the real
  world".** [Paper](https://arxiv.org/pdf/2412.11121). Not yet read.

## Who serves what

Columns: "fields" lists which of the five fields below the tool covers;
"p" means in part, "-" means none. "Active" is the state seen on
2026-11-19 where a source was read, otherwise a guess from memory.

### Configuration languages

They are languages for writing configuration. The user adopts one as the
source of truth; none reads an existing estate of YAML, INI or TOML as a
lens. License and activity for all six are unconfirmed.

| Tool | Kind | License | Active | Fields |
| ---- | ---- | ------- | ------ | ------ |
| CUE | language, validator | Apache-2 (unconfirmed) | yes (unconfirmed) | 3 p |
| KCL | language | Apache-2 (unconfirmed) | slower (unconfirmed) | - |
| Pkl | language | Apache-2 (unconfirmed) | yes (unconfirmed) | - |
| Jsonnet | templating | Apache-2 (unconfirmed) | maintenance (unconfirmed) | - |
| Dhall | language | BSD-3 (unconfirmed) | low (unconfirmed) | - |
| Nickel | language | MIT (unconfirmed) | yes (unconfirmed) | - |

### Policy and validation

License and activity are unconfirmed for all rows.

| Tool | Kind | License | Active | Fields |
| ---- | ---- | ------- | ------ | ------ |
| OPA, conftest | policy engine, CLI | Apache-2 (unconfirmed) | yes (unconfirmed) | 3 p |
| Kyverno | Kubernetes policy | Apache-2 (unconfirmed) | yes (unconfirmed) | 3 p |
| Checkov | IaC scanner | Apache-2 (unconfirmed) | yes (unconfirmed) | 3 p |
| kubeconform | Kubernetes schema check | Apache-2 (unconfirmed) | yes (unconfirmed) | 3 p |
| yamllint | YAML style and syntax | GPL-3 (unconfirmed) | yes (unconfirmed) | - |
| JSON Schema Store | catalogue of schemas | Apache-2 (unconfirmed) | yes (unconfirmed) | - |
| Datree | Kubernetes policy | CLI was open source; company closed 2023 (unconfirmed) | no (unconfirmed) | - |

### Semantic diff

| Tool | Kind | License | Active | Fields |
| ---- | ---- | ------- | ------ | ------ |
| [dyff](https://github.com/homeport/dyff) | YAML and JSON diff | MIT | yes, v1.12.0 seen | 1 p, 2 p (as git external diff) |
| [diffx](https://lib.rs/crates/diffx) | JSON, YAML, TOML, XML, INI, CSV diff | MIT | yes, v0.7.1 of 2026-03-18 | 1 p (no units, schema or secrets mentioned) |
| [confdiff](https://zeli.app/story/49464310) | JSON, JSONC, YAML, TOML, INI, .env, .properties, CSV, XML diff, across formats | MIT per a package listing (unconfirmed) | yes, v0.17.4 | 1 p, 3 p (secrets redacted, as claimed by its own listing) |
| [configdiff](https://pypi.org/project/configdiff/) | JSON, YAML, TOML, INI tree compare | unconfirmed | unconfirmed | 1 p |
| graphtage, difftastic, jd, json-diff, yq-based diffs | structure diff | unconfirmed | unconfirmed | 1 p, 2 p |

The README of dyff shows use through `git log --ext-diff` and
`KUBECTL_EXTERNAL_DIFF`
([README](https://raw.githubusercontent.com/homeport/dyff/main/README.md)).
No tool seen in the search compares "30s" with "30000ms".

### Stores and services

They store values and keep a history; none reads a contract across
formats. Licenses and activity are unconfirmed for all rows.

| Tool | Kind | License | Active | Fields |
| ---- | ---- | ------- | ------ | ------ |
| Consul KV | store | BUSL-1.1 since 2023 (unconfirmed) | yes (unconfirmed) | - |
| etcd | store | Apache-2 (unconfirmed) | yes (unconfirmed) | - |
| AWS AppConfig | service | commercial (unconfirmed) | yes (unconfirmed) | 3 p (schema and function validators, staged rollout) |
| Azure App Configuration | service | commercial (unconfirmed) | yes (unconfirmed) | - |
| Google Runtime Config | service | commercial (unconfirmed) | largely retired (unconfirmed) | - |
| Spring Cloud Config | server, git-backed | Apache-2 (unconfirmed) | yes (unconfirmed) | - |
| Apollo (Ctrip) | server | Apache-2 (unconfirmed) | yes (unconfirmed) | - |
| Nacos | server | Apache-2 (unconfirmed) | yes (unconfirmed) | - |

### Feature flags (neighbours only)

They target users or percentages, not files. Fields: none.

| Tool | Kind | License | Active |
| ---- | ---- | ------- | ------ |
| LaunchDarkly | service | commercial (unconfirmed) | yes (unconfirmed) |
| Unleash | server | Apache-2 core (unconfirmed) | yes (unconfirmed) |
| Flagsmith | server | BSD-3 (unconfirmed) | yes (unconfirmed) |

### GitOps and drift

| Tool | Kind | License | Active | Fields |
| ---- | ---- | ------- | ------ | ------ |
| Argo CD | Kubernetes, live against git | Apache-2 (unconfirmed) | yes (unconfirmed) | 4 p |
| Flux | Kubernetes, live against git | Apache-2 (unconfirmed) | yes (unconfirmed) | 4 p |
| Terraform plan | resource drift | BUSL (unconfirmed) | yes (unconfirmed) | 4 p |
| driftctl | resource drift | discontinued (unconfirmed) | no (unconfirmed) | - |
| AWS Config | service | commercial (unconfirmed) | yes (unconfirmed) | 4 p |
| Ansible, Puppet, Chef, Salt | configuration management | open source cores (unconfirmed) | yes (unconfirmed) | 4 p (desired against actual) |

### Secrets in configuration

They find secrets. None masks them in a diff or compares them by digest,
except as confdiff claims above.

| Tool | Kind | License | Active | Fields |
| ---- | ---- | ------- | ------ | ------ |
| gitleaks | scanner | MIT (unconfirmed) | yes (unconfirmed) | 3 p |
| trufflehog | scanner | AGPL (unconfirmed) | yes (unconfirmed) | 3 p |
| detect-secrets | scanner | Apache-2 (unconfirmed) | yes (unconfirmed) | 3 p |

### "Why is this value what it is"

Each answers for one system and one format. License and activity are
unconfirmed for all rows.

| Tool | Kind | License | Active | Fields |
| ---- | ---- | ------- | ------ | ------ |
| `git config --show-origin` | git option | GPL-2 (unconfirmed) | yes | 1 p (provenance) |
| Spring Boot actuator `/env`, `/configprops` | endpoint | Apache-2 (unconfirmed) | yes (unconfirmed) | 1 p (provenance) |
| .NET `GetDebugView` | debug text | MIT (unconfirmed) | yes (unconfirmed) | 1 p (provenance) |
| `systemd-analyze cat-config` | command | LGPL (unconfirmed) | yes (unconfirmed) | 1 p (provenance) |

### Embedded configuration

Each works inside its own ecosystem. Licenses and activity are
unconfirmed for all rows.

| Tool | Kind | License | Active | Fields |
| ---- | ---- | ------- | ------ | ------ |
| Zephyr Kconfig and devicetree | build-time options | Apache-2 (unconfirmed) | yes (unconfirmed) | 5 p |
| Linux Kconfig | build-time options | GPL (unconfirmed) | yes (unconfirmed) | 5 p |
| Yocto | build system | MIT and others (unconfirmed) | yes (unconfirmed) | 5 p |
| ESP-IDF menuconfig, sdkconfig | build-time options | Apache-2 (unconfirmed) | yes (unconfirmed) | 5 p |
| AUTOSAR configuration tools | ARXML tools | commercial (unconfirmed) | yes (unconfirmed) | 5 p |

## Five fields, and how open each is

1. **A diff of two files of any formats, by value, that knows a
   contract (units, ranges, secrets by digest) and gives provenance.**
   Served by: dyff, diffx, configdiff, jd and similar for structure
   across one to a few formats; confdiff for secret fingerprints (its
   own claim). Missing: unit equivalence, range checks, any contract,
   and provenance across layers (file, environment, default). Rating:
   partly served for structure; open for contract and provenance.
2. **git textconv or difftool drivers that show changes by value.**
   Served by: dyff as an external diff, and per-user scripts with `jq`
   or `yq`. Missing: one maintained driver for many formats that
   normalises and masks secrets, so that `git log -p` and `git blame`
   read by value. The search found no dedicated project (unconfirmed).
   Rating: partly served, thin.
3. **Review of configuration-only pull requests: keys changed, values out
   of range, secrets touched, no program run.** Served by: conftest/OPA,
   Checkov, kubeconform, Kyverno, yamllint (pass or fail on rules), and
   confdiff (claims PR output; not checked). Missing: a per-key report
   of what changed in value, for INI, `.properties`, TOML, XML and mixed
   estates, from a schema of keys and not a policy language. Rating:
   crowded for Kubernetes; open for other formats.
4. **Drift of one configuration across many hosts, grouped by value.**
   Served by: Ansible, Puppet, Chef and Salt reports; Argo CD and Flux
   for Kubernetes; AWS Config. Missing: a read-only comparison of the
   same files over many hosts, grouped by distinct value ("12 hosts have
   30 s, 3 have 45 s"), with no collector tied to a management tool. No
   dedicated tool was found (unconfirmed). Rating: partly served inside
   management tools; the read-only view looks open.
5. **Embedded configuration validated at build time, with provenance, and
   no parser in the shipped binary.** Served by: Kconfig, devicetree,
   Yocto, sdkconfig and AUTOSAR tools, each inside its own ecosystem.
   Missing: runtime configuration files (JSON, INI, TLV) checked against
   a contract at build time and compiled to constants or an image, with
   provenance, for C89 and MISRA-style code. Rating: partly served per
   ecosystem; open across formats.

## Where the project stands

These are decisions to weigh, not settled ones.

- **Fields 1 and 3 together.** A diff by value that knows the contract
  and gives provenance, delivered as a pull request comment, for the
  formats nobody covers (INI, `.properties`, TOML, XML, mixed estates).
  This is where the gap looks widest.
- **Field 5.** Build-time-checked configuration for embedded C, in C89
  with no parser in the binary. No cross-format tool was found.
- **Field 4.** Drift as an export from tools that already collect it
  (Ansible, osquery, Consul), not a collector of its own.
- **Field 2.** The git textconv as a feature of the diff, not a product.
- **Do not compete where the field is crowded.** Structural diff for
  YAML and policy for Kubernetes are served. Call dyff for display of
  YAML, conftest/OPA for policy, gitleaks for finding secrets, and sit
  beside Kconfig, Zephyr and Yocto instead of replacing them.

## To check before building

- confdiff: its license (a package listing says MIT; the repository's
  LICENSE file was not read), its real feature set, and whether its
  pull request output goes beyond redaction. The npm and Socket pages
  could not be fetched.
- diffx: the MIT license and the formats are read from
  [its page](https://lib.rs/crates/diffx); no support for units, schema
  or secrets is mentioned there, so check the source.
- The arXiv 2024 study, and the percentages of Yin et al. and the
  parameter counts of Xu et al. from the papers themselves.
- The activity date (last release) of every row marked
  "(unconfirmed)", and the licenses with it.
- The sentence "about 70% of outages come from changes" in the SRE
  book, and Meta's own post on the 2021 outage.
