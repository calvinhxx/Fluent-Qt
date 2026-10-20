# Form state and validation

> **Status:** Accepted contract

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Architecture](README.md) › Runtime contracts

[← Files and feedback](files-and-feedback.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md) · [CodeBlock →](code-block.md)
<!-- docs-nav:top:end -->

`fluent::FormController` coordinates existing `layout::Field` widgets. It keeps
edit, interaction, validation and submission state in the shared C++ runtime;
PySide6 exposes that same runtime as `fluentqt.FormController`, and WebAssembly
compiles the same implementation. Link the core `FluentQt::FluentQt` target.

## Ownership and values

Register each Field with a unique nonempty string key. The controller borrows
Fields without reparenting them or their editors. A Field can be registered only
once in a controller. Field destruction unregisters it. PySide6 retains borrowed
wrappers while registered and releases them on removal or destruction.

The application owns values, validators, network operations and persistence.
Call `markFieldChanged(key)` for every value change, including programmatic
changes, and `setFieldTouched(key)` on the desired interaction boundary, such as
editing completion. Dirty means edited since `markClean()`, `reset()` or a
successful submission; it does not compare values or store a copy of secrets.
Reverting an editor to an earlier value does not automatically mark it clean.

`reset()` clears tracking and validation without changing any editor values.
Reset application values separately. `markClean()` only clears dirty flags.
Replacing a registered Field's editor invalidates its validation and marks it
edited. All controller calls and completion callbacks must run on its GUI thread;
marshal worker-thread results through queued Qt connections.

## Validation and stale results

`validateField(key)` or `validate()` emits `validationRequested(key, token)`.
The application calls `completeValidation(key, token, valid, message)` either
synchronously or later. The token is opaque, nonzero and single-use. Unknown,
removed, replaced, reset or superseded requests return `false` on completion.
An unconnected validator remains pending; it is never treated as successful.

Every edit cancels all in-flight validation and submission tokens, since another
field's pending validator may depend on the edited value. The changed field
returns to unknown validity; other completed results remain visible. A submit
always revalidates every registered field. The application remains responsible
for cancelling expensive underlying requests; token rejection only prevents
stale UI updates.

The controller sets Field presentation while registered. Pending validation uses
`Field::ValidationState::Validating`; invalid results use Error, and a successful
message uses Success. Empty successful messages restore the helper text. The
Field shows one feedback message at a time and preserves its editor's value,
focus proxy, ownership and native input behavior.

```cpp
auto* form = new fluent::FormController(this);
form->addField(QStringLiteral("address"), addressField);
connect(addressEditor, &fluent::textfields::LineEdit::textChanged,
        form, [form] { form->markFieldChanged(QStringLiteral("address")); });
connect(form, &fluent::FormController::validationRequested, this,
        [form, addressEditor] (const QString& key, quint64 token) {
    const QUrl url(addressEditor->text(), QUrl::StrictMode);
    const bool valid = url.isValid() && url.scheme() == QStringLiteral("https")
        && !url.host().isEmpty();
    form->completeValidation(key, token, valid,
        valid ? QString() : QStringLiteral("Enter a valid HTTPS address."));
});
```

## Submission and errors

`submit()` rejects an empty form or an already active submission. It marks all
fields touched, requests validation, then emits `submitRequested(token)` once
all results are valid. Synchronous validators cannot accidentally bypass later
fields. `isSubmitting()` covers both validation and the application save step.

Complete persistence with `completeSubmit(token, success, message)`. Success
marks current values clean; failure keeps edits and exposes `submissionError()`.
Editing during a save invalidates its completion, so a late response cannot mark
new input clean. `submissionFinished(false)` also reports validation rejection;
an edit or reset cancellation is observable through `stateChanged()`.

`errorMessages()` and `invalidFieldKeys()` follow registration order. Failed
submission focuses the first visible, enabled invalid editor; applications can
also call `focusFirstError()` from their error summary. Screen-reader behavior
still needs native platform review; focused repository tests establish value,
focus and lifecycle behavior only.

## Gallery and design

Open **Layout → Field → Connection settings**. The C++, Python and browser
examples demonstrate dirty state, local and delayed validation, failure,
stale-result rejection and saving. The example uses a deterministic local
simulation and never sends or stores credentials.

The [editable Figma design](https://www.figma.com/design/OfdJQXvCiex396fPOCTwrA)
records the Field states and Light/Dark connection layout. Native rendering uses
FluentQt typography, colors and input controls, including High Contrast.

Owner tests: `test_form_controller`, `test_field`, the connection-form Gallery
contracts, and the PySide6 binding and sample contracts.

<!-- docs-nav:bottom:start -->
---
[← Files and feedback](files-and-feedback.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md) · [CodeBlock →](code-block.md)
<!-- docs-nav:bottom:end -->
