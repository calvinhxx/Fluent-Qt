# FluentQt documentation

> **Status:** Current guide (documentation home)

FluentQt documentation is organized by task: use Gallery or the website's interactive **Property preview** to test controls, the website docs for foundations and searchable API reference, and the development tree when changing the library itself.

![FluentQt Gallery on Windows with Mica, showing navigation, component cards, and live samples](assets/readme/gallery-home.jpg)

## Choose a path

| Goal | Start here | Continue with |
|---|---|---|
| Try controls, tweak properties live, and copy code | [WebAssembly Gallery](https://calvinhxx.github.io/Fluent-Qt/#gallery) | [Component documentation & Property preview](https://calvinhxx.github.io/Fluent-Qt/api/checkbox/?playground=1) |
| Learn themes, layout, models and extension lifetimes | [FluentQt foundations](https://calvinhxx.github.io/Fluent-Qt/api/foundation/) | Use the document directory to open a guide or API reference |
| Add FluentQt to a C++ or PySide6 application | [Project README](../README.md) | [Onboarding tools](../tools/onboarding/README.md) |
| Add a GUI with a coding agent | [AI-assisted development](ai/README.md) | [Integration workflow](ai/add-gui-to-project.md) |
| Add or change a component | [Development tree](development/README.md) | [Component API conventions](development/component-api-conventions.md) |
| Understand behavior guarantees | [Architecture contracts](architecture/README.md) | [Compatibility policy](development/compatibility-policy.md) |
| Package or release FluentQt | [Packaging workflow](development/packaging-workflow.md) | [Release governance](development/release-governance.md) |
| Ask a question or contribute | [Community](community/README.md) | [Contributing guide](../CONTRIBUTING.md) |

## Documentation tree

Open the [complete table of contents](SUMMARY.md) for every guide, contract,
record, and release note. Leaf pages include generated breadcrumbs and
previous/next links. It also links to the PySide6, onboarding, and community
guides kept alongside their code.

```text
docs/
├── README.md                 this page
├── ai/                       agent discovery and GUI delivery
├── architecture/             runtime and component behavior contracts
├── design-languages/         Fluent visual contract and design sources
├── development/              contributor and maintainer workflows
├── releases/                 immutable release notes
└── community/                support and participation routes
```

## How to read document status

| Status | Meaning |
|---|---|
| **Current guide** | Follow it for work on the active branch. |
| **Living reference** | Generated or maintained alongside the implementation. |
| **Accepted contract** | Describes a public or architectural decision that remains in force. |
| **Historical record** | Preserves dated evidence; do not treat its counts or open items as current. |

Documents that contain changing totals should link to generated data instead
of copying the numbers. The current machine-readable sources are:

- [AI component catalog](ai/generated/fluentqt-ai-catalog.json)
- [Public API catalog](../site/api/catalog.json)
- [Accessibility inventory](development/accessibility-inventory.md)
- [PySide6 API manifest](../bindings/pyside6/api-manifest.json)

Documentation changes follow the concise writing and diagram rules in the
[documentation style guide](development/documentation-style.md).
