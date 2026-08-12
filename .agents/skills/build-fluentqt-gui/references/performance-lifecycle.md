# Performance and Lifecycle

Performance and visual quality are co-equal acceptance gates. Use this
reference together with Visual refinement; it does not authorize visually
generic, poorly aligned, low-contrast, or unfinished UI in exchange for lower
memory use.

Treat data cardinality, update frequency, and object lifetime as component
selection inputs. A visually correct component is still the wrong component
when it materializes unbounded content, rebuilds stable rows for every event,
or keeps one-shot surfaces alive for the lifetime of the window.

## Classify the data before choosing a viewport

Record the expected and stress cardinality, whether growth is bounded, and the
update shape for every repeated surface:

| Data shape | Preferred ownership | Required performance contract |
| --- | --- | --- |
| Small finite composition | Layout inside `ScrollView` when it can exceed the viewport | Child count is intentionally bounded |
| Long or growing flat collection | `ListView` + `QAbstractItemModel` + delegate | Rows are virtualized; inserts and changes are incremental |
| Long hierarchy | `TreeView` + model + delegate | Expansion does not instantiate the whole subtree |
| Two-dimensional collection | `GridView` + model + delegate | Cell materialization is viewport-bounded |
| One large document/canvas | `ScrollView` or a document/canvas control | The content is one logical surface, not a widget per record |
| Live stream | Item model plus batching, paging/windowing, and tail-follow policy | Bursts do not rebuild the full history or force-scroll a reader |

`ScrollView` virtualizes neither child widgets nor caller-owned data. Do not use
`ScrollView + QVBoxLayout + one QWidget per record` for sessions, messages,
events, logs, search results, transfers, notifications, or any other collection
that can grow with user activity.

## Preserve item-view virtualization

Using `ListView`, `TreeView`, or `GridView` is necessary but not sufficient.
Keep these rules together:

- store row state in a `QAbstractItemModel` or a tested proxy model;
- paint rich rows with a delegate; do not call `setIndexWidget()` for every row
  or open an unbounded set of persistent editors;
- if interactive editors are essential, create them only for visible rows and
  recycle or close them as rows leave the viewport;
- use `beginInsertRows`, `beginRemoveRows`, `dataChanged`, and targeted layout
  changes; do not reset the model or rebuild every row for an append or token
  update;
- coalesce high-frequency stream updates to a frame-sized interval when the
  protocol can deliver faster than the display needs to repaint;
- preserve the reader's scroll anchor. Follow the tail only when the user was
  already near the tail or explicitly requested it;
- keep delegate caches bounded and key them by content, width, scale, and theme.
  Invalidate them when any key changes;
- avoid a persistent `QTextDocument`, image decoder, syntax highlighter, or
  other heavy renderer per historical row. Cache only a bounded working set.

Variable-height rich rows still need measured height. Cache compact height
metadata separately from heavy render objects, and exercise long Markdown,
CJK, emoji, code blocks, and width changes. A fixed row height is valid only
when the product deliberately previews or elides content and provides a clear
way to inspect the full value.

## Bound the data, not only the widgets

An item view bounds presentation objects; it does not automatically bound the
model, transport response, decoded images, or retained rich-text cache. For an
unbounded or server-owned history, define one of these policies explicitly:

1. cursor/keyset pagination with `canFetchMore()`/`fetchMore()` or an equivalent
   controller;
2. a documented recent-item window with an older-history affordance;
3. a domain-specific retention limit where older data is durably available
   elsewhere;
4. a proven finite upper bound.

Prefer cursor/keyset pagination for mutable histories. Do not silently discard
older rows. If the current backend only exposes a full snapshot, record that as
an integration limitation, cap the client projection when correctness permits,
and do not claim end-to-end bounded memory until the transport is paged.

Apply backpressure to producers independently of painting. Cap queued work,
cancel stale requests on scope changes, reject late results by request/session
identity, and move parsing or expensive formatting off the GUI thread when it
can exceed an interaction frame.

## Choose transient lifetime deliberately

Classify each secondary surface by frequency, state, and cost:

| Surface | Default lifecycle |
| --- | --- |
| One-shot confirmation, question, picker, or error dialog | Construct when requested; parent or guard it; delete after finish/close |
| Anchored menu, flyout, teaching tip, or contextual preview | Construct on demand; destroy on dismissal unless rapid reuse is measured |
| Temporary drawer with cheap derived content | Create lazily on first open; release on close when rebuilding is cheap |
| Frequently toggled, stateful inspector | Create lazily and cache only while its retained state/cost justifies it |
| Persistent primary pane | Construct with the shell or lazily with an explicit cache policy |

For Qt C++, use a parent plus `QPointer` (or another explicit guarded owner),
connect `finished`/`closed` to cleanup, clear controller pointers on destruction,
and make repeated close/session-switch paths idempotent. `WA_DeleteOnClose` is
acceptable when every access is guarded. Never keep a raw pointer that can
survive deferred deletion.

For PySide6, keep one explicit Python owner while open, call `deleteLater()` on
finish when appropriate, clear references, and avoid signal closures that keep
the surface alive accidentally.

Do not destroy a surface before its exit animation or callback completes. Use
the component's `closed`/`finished` signal rather than guessing with a timer.

## Set measurable budgets

Add deterministic tests proportional to the surface:

- load at least 10,000 lightweight rows (or a domain-realistic equivalent) and
  verify the number of row widgets/heavy render objects stays viewport-bounded;
- append and update rows and assert the model emits targeted insert/change
  signals rather than `modelReset`;
- replay a burst and verify batching, cancellation, and stale-result rejection;
- scroll away from the tail, append data, and verify the reader is not dragged
  back; then explicitly follow and verify convergence;
- open and close a one-shot dialog repeatedly and require the live instance
  count to return to baseline after deferred deletion;
- switch theme and width under dense data and require bounded caches to
  invalidate without clipping or stale colors.

Wall-clock and RSS checks are useful diagnostics but can be noisy in CI. Prefer
deterministic invariants first: model signals, retained-row limits, object
counts, cache limits, request identities, and teardown completion.

## Acceptance gate

- Performance acceptance is independent of visual acceptance; both must pass.
- Every repeated surface declares finite composition, model/view virtualization,
  pagination/windowing, or another explicit upper bound.
- No unbounded collection is implemented as a layout full of child widgets.
- Stream updates change only affected model rows and are coalesced when needed.
- Tail-follow respects deliberate user scrolling.
- Heavy delegate/document/image caches have measured limits and invalidation.
- One-shot transient surfaces are absent from the idle object tree and return
  to zero live instances after close.
- Cached drawers or inspectors have a written reuse reason and are lazy-loaded.
- Session/workspace changes cancel or ignore stale asynchronous work.
- Performance tests exercise realistic dense data, not only 0/1/2/8 rows.
