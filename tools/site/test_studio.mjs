import test from 'node:test';
import assert from 'node:assert/strict';

import {
  ANCHOR_PROP_KEYS,
  MAX_DEPTH,
  MAX_NODES,
  STUDIO_CATALOG,
  STUDIO_CATEGORIES,
  STUDIO_TEMPLATES,
  buildDragPreviewDocument,
  canAcceptChild,
  cloneDoc,
  countNodes,
  createHistory,
  createNode,
  decodeShareState,
  duplicateNode,
  encodeShareState,
  findNode,
  findParent,
  generateCpp,
  generateDesignerUi,
  generatePython,
  insertNode,
  mapInteractionEventToProps,
  moveNode,
  removeNode,
  resetNodeProps,
  resolveAnchorPlacement,
  resolveDropTarget,
  updateNodeProps,
  validateDocument,
} from '../../site/studio/studio-core.mjs';
import {
  AI_PROVIDER_PRESETS,
  AI_STYLE_PRESETS,
  applyRefinedSubtree,
  buildAiMessages,
  buildCatalogSummary,
  changeAiEndpoint,
  extractJsonFromText,
  generateStudioPageWithAi,
  sanitizeAiDocument,
} from '../../site/studio/studio-ai.mjs';

test('catalog covers all 103 types across 13 official categories', () => {
  const categoryIds = new Set(STUDIO_CATEGORIES.map((c) => c.id));
  const types = Object.keys(STUDIO_CATALOG);
  assert.equal(categoryIds.size, 13);
  assert.equal(types.length, 103);
  for (const type of types) {
    const meta = STUDIO_CATALOG[type];
    assert.ok(categoryIds.has(meta.category), `Unknown category for ${type}`);
    const node = createNode(type);
    assert.equal(node.type, type);
    assert.ok(node.id.length > 0);
    assert.equal(typeof node.props, 'object');
  }
});

test('all built-in templates validate cleanly', () => {
  assert.deepEqual(Object.keys(STUDIO_TEMPLATES).sort(), ['account-form', 'analytics', 'blank', 'settings', 'starter']);
  for (const [id, tpl] of Object.entries(STUDIO_TEMPLATES)) {
    const res = validateDocument(tpl.document);
    assert.equal(res.ok, true, `Template ${id} invalid: ${res.error}`);
    const minCount = id === 'blank' ? 1 : (id === 'starter' ? 2 : 3);
    assert.ok(res.count >= minCount, `Template ${id} unexpectedly small: ${res.count}`);
  }
  assert.equal(STUDIO_TEMPLATES.starter.document.root.type, 'AnchorBox');
  assert.equal(STUDIO_TEMPLATES.starter.document.root.children.length, 1);
  assert.equal(STUDIO_TEMPLATES.starter.document.root.children[0].id, 'button_1');
  assert.equal(STUDIO_TEMPLATES.starter.document.root.children[0].props.anchorPosition, 'Center');
  assert.equal(STUDIO_TEMPLATES.blank.document.root.type, 'AnchorBox');
});

test('insertNode respects Field 1-child limit, MAX_NODES, and MAX_DEPTH', () => {
  const doc = cloneDoc(STUDIO_TEMPLATES.blank.document);
  const fieldNode = createNode('Field', doc);
  let res = insertNode(doc, 'root', 0, fieldNode);
  assert.equal(res.ok, true);

  const firstChild = createNode('LineEdit', res.document);
  res = insertNode(res.document, fieldNode.id, 0, firstChild);
  assert.equal(res.ok, true);

  const insertedField = findNode(res.document.root, fieldNode.id);
  assert.equal(canAcceptChild(insertedField, 1), false);

  const secondChild = createNode('Button', res.document);
  const denied = insertNode(res.document, fieldNode.id, 1, secondChild);
  assert.equal(denied.ok, false);

  // Depth limit check
  let deepDoc = cloneDoc(STUDIO_TEMPLATES.blank.document);
  let parentId = 'root';
  for (let d = 2; d <= MAX_DEPTH; d++) {
    const box = createNode('VBox', deepDoc);
    const step = insertNode(deepDoc, parentId, 0, box);
    assert.equal(step.ok, true, `Expected depth ${d} to succeed: ${step.error}`);
    deepDoc = step.document;
    parentId = box.id;
  }
  const tooDeep = insertNode(deepDoc, parentId, 0, createNode('Button', deepDoc));
  assert.equal(tooDeep.ok, false);

  // Max nodes check
  let wideDoc = { schemaVersion: 1, root: { id: 'root', type: 'VBox', props: { spacing: 8, padding: 8 }, children: [] } };
  while (countNodes(wideDoc.root) < MAX_NODES) {
    const step = insertNode(wideDoc, 'root', 0, createNode('Button', wideDoc));
    assert.equal(step.ok, true);
    wideDoc = step.document;
  }
  const overLimit = insertNode(wideDoc, 'root', 0, createNode('Button', wideDoc));
  assert.equal(overLimit.ok, false);
});

test('moveNode handles reordering within same parent, cross-container move, and self-cycle guard', () => {
  const doc = cloneDoc(STUDIO_TEMPLATES.settings.document);
  assert.equal(moveNode(doc, 'root', 'card_1', 0).ok, false);
  assert.equal(moveNode(doc, 'card_1', 'grid_1', 0).ok, false);

  // Forward reorder inside same parent adjusts index after splice
  const childrenBefore = doc.root.children.map((c) => c.id);
  const firstId = childrenBefore[0];
  const movedForward = moveNode(doc, firstId, 'root', 3);
  assert.equal(movedForward.ok, true);
  assert.equal(movedForward.document.root.children[2].id, firstId);

  // Cross-parent move
  const cross = moveNode(movedForward.document, 'btn_apply', 'card_1', 0);
  assert.equal(cross.ok, true);
  const info = findParent(cross.document.root, 'btn_apply');
  assert.equal(info?.parent?.id, 'card_1');
  assert.equal(info?.index, 0);
});

test('duplicateNode and removeNode maintain valid unique IDs', () => {
  const doc = cloneDoc(STUDIO_TEMPLATES.settings.document);
  const dup = duplicateNode(doc, 'card_1');
  assert.equal(dup.ok, true);
  assert.notEqual(dup.nodeId, 'card_1');
  assert.equal(validateDocument(dup.document).ok, true);
  assert.ok(countNodes(dup.document.root) > countNodes(doc.root));

  assert.equal(removeNode(doc, 'root').ok, false);
  const removed = removeNode(dup.document, dup.nodeId);
  assert.equal(removed.ok, true);
  assert.equal(countNodes(removed.document.root), countNodes(doc.root));
});

test('updateNodeProps and createHistory work cleanly without mutating old snapshots', () => {
  const doc = cloneDoc(STUDIO_TEMPLATES.settings.document);
  const history = createHistory(doc, 10);
  assert.equal(history.canUndo(), false);

  const patched = updateNodeProps(history.current(), 'btn_apply', { text: 'Ship Build' });
  assert.equal(patched.ok, true);
  history.push(patched.document);

  assert.equal(history.canUndo(), true);
  assert.equal(findNode(history.current().root, 'btn_apply').props.text, 'Ship Build');

  history.undo();
  assert.equal(findNode(history.current().root, 'btn_apply').props.text, 'Apply Settings');
  assert.equal(history.canRedo(), true);

  history.redo();
  assert.equal(findNode(history.current().root, 'btn_apply').props.text, 'Ship Build');
});

test('C++, PySide6, and Qt Designer .ui exporters emit valid code for all controls', () => {
  const doc = cloneDoc(STUDIO_TEMPLATES.analytics.document);
  const patched = updateNodeProps(doc, 'lbl_dash', { text: 'Ops <Live> & "Ready"' }).document;

  const cpp = generateCpp(patched);
  assert.match(cpp, /#include <FluentQt\/Charts\.h>/);
  assert.match(cpp, /#include <FluentQt\/Layout\.h>/);
  assert.match(cpp, /QWidget\* createStudioPage/);
  assert.match(cpp, /using namespace fluent::charts;/);
  assert.match(cpp, /new LineChart\(/);
  assert.match(cpp, /new BarChart\(/);
  assert.match(cpp, /setIsActive\(true\)/);
  assert.match(cpp, /setBackgroundVisible\(true\)/);
  assert.match(cpp, /Ops <Live> & \\"Ready\\"/);

  const py = generatePython(patched);
  assert.match(py, /from PySide6\.QtWidgets import/);
  assert.match(py, /fluentqt\.charts/);
  assert.match(py, /LineChart/);
  assert.match(py, /setIsActive\(True\)/);
  assert.match(py, /setBackgroundVisible\(True\)/);

  const ui = generateDesignerUi(patched);
  assert.match(ui, /^<\?xml version="1\.0" encoding="UTF-8"\?>/);
  assert.match(ui, /<ui version="4\.0">/);
  assert.match(ui, /Ops &lt;Live&gt; &amp; &quot;Ready&quot;/);
  assert.doesNotMatch(ui, /<string>Ops <Live>/);

  // Also verify code export across all categories including extensions, Window & AnchorBox
  let rich = cloneDoc(STUDIO_TEMPLATES.blank.document);
  for (const t of ['DataGrid', 'PieChart', 'TabView', 'CommandBar', 'ContentDialog', 'ColorPicker', 'CalendarView', 'TitleBar', 'FontIcon', 'AnchorBox', 'Window', 'ParticleBackdrop', 'SpatialView', 'SpatialItem', 'QmlView']) {
    rich = insertNode(rich, 'root', rich.root.children.length, createNode(t, rich)).document;
  }
  const anchorNode = findNode(rich.root, 'anchor_box_1');
  rich = insertNode(rich, anchorNode.id, 0, createNode('Button', rich)).document;
  const richCpp = generateCpp(rich);
  for (const cls of ['DataGrid', 'PieChart', 'TabView', 'CommandBar', 'ColorPicker', 'CalendarView', 'TitleBar', 'FontIcon', 'AnchorLayout', 'Window', 'ParticleBackdrop', 'SpatialSession', 'SpatialView', 'QmlSession']) {
    assert.match(richCpp, new RegExp(`new ${cls}\\(`));
  }
  const richPy = generatePython(rich);
  for (const pyCls of ['DataGrid', 'PieChart', 'TabView', 'CommandBar', 'ColorPicker', 'CalendarView', 'TitleBar', 'FontIcon', 'AnchorLayout', 'Window', 'ParticleBackdrop', 'SpatialSession', 'QmlSession']) {
    assert.match(richPy, new RegExp(`\\b${pyCls}\\(`));
  }
  const richUi = generateDesignerUi(rich);
  assert.match(richUi, /FluentDataGrid/);
  assert.match(richUi, /name="fluentFormHost".*<string>Window<\/string>/);
  assert.match(richUi, /FluentParticleBackdrop/);
  assert.match(richUi, /FluentSpatialView/);
  assert.match(richUi, /FluentQmlView/);

  // SpatialItem enforces 1-child capacity just like Field
  const spItem = findNode(rich.root, 'spatial_item_1');
  const spFirst = insertNode(rich, spItem.id, 0, createNode('Card', rich));
  assert.equal(spFirst.ok, true);
  assert.equal(insertNode(spFirst.document, spItem.id, 1, createNode('Button', spFirst.document)).ok, false);
});

test('share state hash encoding and decoding round-trips safely with Unicode', () => {
  const doc = cloneDoc(STUDIO_TEMPLATES['account-form'].document);
  const withCjk = updateNodeProps(doc, 'title_form', { text: '创建中文开发者账号 🚀' }).document;

  const token = encodeShareState(withCjk);
  assert.ok(token.length > 20);
  const restored = decodeShareState(`#state=${token}`);
  assert.equal(restored.ok, true);
  assert.deepEqual(restored.document, withCjk);

  assert.equal(decodeShareState('#state=!!!not-valid-base64!!!').ok, false);
  assert.equal(decodeShareState('#other=123').ok, false);
});

test('updateNodeProps preserves null, deletes undefined, resetNodeProps preserves anchors, and exporters emit schema props', () => {
  let doc = cloneDoc(STUDIO_TEMPLATES.blank.document);
  const anchor = createNode('AnchorBox', doc);
  doc = insertNode(doc, 'root', 0, anchor).document;
  const btn = createNode('Button', doc);
  doc = insertNode(doc, anchor.id, 0, btn).document;

  doc = updateNodeProps(doc, btn.id, {
    anchorPosition: 'BottomRight',
    icon: 'Save',
    cornerRadius: null,
    enabled: false,
  }).document;
  assert.equal(findNode(doc.root, btn.id).props.cornerRadius, null);
  doc = updateNodeProps(doc, btn.id, { enabled: undefined }).document;
  assert.equal('enabled' in findNode(doc.root, btn.id).props, false);

  // Add cornerRadius override and tickPosition on slider for codegen check
  doc = updateNodeProps(doc, btn.id, { cornerRadius: 10, checkable: true, checked: true }).document;
  const slider = createNode('Slider', doc);
  doc = insertNode(doc, 'root', 0, slider).document;
  doc = updateNodeProps(doc, slider.id, {
    tickPosition: 'TicksBothSides',
    handleSize: 22,
  }).document;

  const extraSchemas = {
    Button: {
      properties: [
        { id: 'icon', type: 'enum', default: 'None' },
        { id: 'cornerRadius', type: 'number', nullable: true, default: null },
        { id: 'checkable', property: 'checkable', setter: 'setCheckable', type: 'boolean', default: false },
        { id: 'checked', property: 'checked', setter: 'setChecked', type: 'boolean', default: false },
      ],
    },
    Slider: {
      properties: [
        { id: 'tickPosition', property: 'tickPosition', setter: 'setTickPosition', type: 'enum', cppPrefix: 'QSlider::TickPosition::', pythonPrefix: 'QSlider.TickPosition.', default: 'NoTicks' },
        { id: 'handleSize', property: 'handleSize', setter: 'setHandleSize', type: 'number', integer: true, default: 18 },
      ],
    },
  };

  const cpp = generateCpp(doc, extraSchemas);
  assert.match(cpp, /#include <FluentQt\/Design\.h>/);
  assert.match(cpp, /setIconGlyph\(Typography::Icons::Save\)/);
  assert.match(cpp, /setCornerRadii\(QMargins\(10, 10, 10, 10\)\)/);
  assert.match(cpp, /setCheckable\(true\)/);
  assert.match(cpp, /setChecked\(true\)/);
  assert.match(cpp, /#include <QSlider>/);
  assert.match(cpp, /setTickPosition\(QSlider::TickPosition::TicksBothSides\)/);
  assert.match(cpp, /setHandleSize\(22\)/);

  const py = generatePython(doc, extraSchemas);
  assert.match(py, /setIconGlyph\(Typography\.Icons\.Save\)/);
  assert.match(py, /setCornerRadii\(QMargins\(10, 10, 10, 10\)\)/);
  assert.match(py, /from PySide6\.QtWidgets import .*QSlider/);
  assert.match(py, /setTickPosition\(QSlider\.TickPosition\.TicksBothSides\)/);
  assert.match(py, /setHandleSize\(22\)/);

  const reset = resetNodeProps(doc, btn.id, ['anchorPosition', 'anchorMargin', 'anchorOffsetX', 'anchorOffsetY']);
  assert.equal(reset.ok, true);
  const resetBtnProps = findNode(reset.document.root, btn.id).props;
  assert.equal(resetBtnProps.anchorPosition, 'BottomRight');
  assert.equal(resetBtnProps.icon, undefined);
  assert.equal(resetBtnProps.cornerRadius, undefined);
});

test('resolveDropTarget handles Field ancestry, edge-escape, Grid 2D slots, and AnchorBox snapping', () => {
  const settings = cloneDoc(STUDIO_TEMPLATES.settings.document);
  const rects = {
    root: { x: 0, y: 0, w: 860, h: 640, visible: true },
    card_1: { x: 20, y: 120, w: 820, h: 220, visible: true },
    grid_1: { x: 36, y: 136, w: 788, h: 120, visible: true },
    field_theme: { x: 36, y: 136, w: 386, h: 90, visible: true },
    combo_theme: { x: 36, y: 162, w: 386, h: 32, visible: true },
    field_port: { x: 438, y: 136, w: 386, h: 90, visible: true },
    slider_scale: { x: 438, y: 162, w: 386, h: 24, visible: true },
    row_actions: { x: 20, y: 400, w: 820, h: 40, visible: true },
    btn_apply: { x: 20, y: 400, w: 120, h: 36, visible: true },
    btn_reset: { x: 152, y: 400, w: 120, h: 36, visible: true },
  };

  // Hovering leaf slider_scale inside full Field (1/1) climbs to grid_1, NOT root.
  const fieldClimb = resolveDropTarget(settings, rects, 690, 174, { kind: 'palette', type: 'Button' }, 'expanded');
  assert.equal(fieldClimb.targetParentId, 'grid_1');
  assert.equal(fieldClimb.horizontal, true);
  assert.equal(fieldClimb.before, false);

  // Outer 6px top edge of card_1 escapes to root before card_1.
  const edgeEscape = resolveDropTarget(settings, rects, 200, 124, { kind: 'palette', type: 'Button' }, 'expanded');
  assert.equal(edgeEscape.targetParentId, 'root');
  assert.equal(edgeEscape.before, true);

  // Interior of card_1 drops inside card_1.
  const insideCard = resolveDropTarget(settings, rects, 200, 290, { kind: 'palette', type: 'Button' }, 'expanded');
  assert.equal(insideCard.targetParentId, 'card_1');

  // Compact viewport switches Grid resolution to vertical stacking.
  const gridCompact = resolveDropTarget(settings, rects, 100, 210, { kind: 'palette', type: 'Button' }, 'compact');
  assert.equal(gridCompact.horizontal, false);

  // Moving btn_apply onto itself is marked isNoop; moving past btn_reset commits real index change.
  const noopMove = resolveDropTarget(settings, rects, 80, 418, { kind: 'move', nodeId: 'btn_apply' }, 'expanded');
  assert.equal(noopMove.isNoop, true);
  const reorderMove = resolveDropTarget(settings, rects, 240, 418, { kind: 'move', nodeId: 'btn_apply' }, 'expanded');
  assert.equal(reorderMove.isNoop, false);
  assert.equal(reorderMove.targetParentId, 'row_actions');

  // AnchorBox magnetic snap & offset clamping on an 860px wide canvas.
  const anchorRect = { x: 20, y: 40, w: 860, h: 220, visible: true };
  const centerSnap = resolveAnchorPlacement(anchorRect, 450, 150, 120, 36, 12);
  assert.equal(centerSnap.anchorProps.anchorPosition, 'Center');
  assert.equal(centerSnap.anchorProps.anchorOffsetX, 0);
  assert.equal(centerSnap.anchorProps.anchorOffsetY, 0);
  assert.notEqual(centerSnap.snapX, null);
  assert.notEqual(centerSnap.snapY, null);

  const unsnapped = resolveAnchorPlacement(anchorRect, 720, 205, 120, 36, 12);
  assert.equal(unsnapped.anchorProps.anchorPosition, 'BottomRight');
  assert.ok(unsnapped.anchorProps.anchorOffsetX >= -400 && unsnapped.anchorProps.anchorOffsetX <= 400);

  // AnchorBox duplication offsets (+24, +24) and Python Center codegen respects offsets.
  let starterDupDoc = cloneDoc(STUDIO_TEMPLATES.starter.document);
  const dupStarter = duplicateNode(starterDupDoc, 'button_1');
  assert.equal(dupStarter.ok, true);
  const dupBtn = findNode(dupStarter.document.root, dupStarter.nodeId);
  assert.equal(dupBtn.props.anchorPosition, 'Center');
  assert.equal(dupBtn.props.anchorOffsetX, 24);
  assert.equal(dupBtn.props.anchorOffsetY, 24);
  const pyStarter = generatePython(dupStarter.document);
  assert.match(pyStarter, /center_in=root/);
  assert.match(pyStarter, /horizontal_center=\(root, 24\), vertical_center=\(root, 24\)/);

  // buildDragPreviewDocument strips stale Anchor props when moving a node out of AnchorBox into VBox.
  let anchorDoc = { schemaVersion: 1, root: { id: 'root', type: 'VBox', props: { spacing: 12, padding: 16 }, children: [] } };
  const ab = createNode('AnchorBox', anchorDoc);
  anchorDoc = insertNode(anchorDoc, 'root', 0, ab).document;
  const childBtn = createNode('Button', anchorDoc);
  anchorDoc = insertNode(anchorDoc, ab.id, 0, childBtn).document;
  anchorDoc = updateNodeProps(anchorDoc, childBtn.id, { anchorPosition: 'BottomRight', anchorOffsetX: -12 }).document;

  const leaveTarget = {
    targetParentId: 'root',
    targetParentType: 'VBox',
    targetIndex: 1,
    anchorPlacement: null,
  };
  const leftPreview = buildDragPreviewDocument(anchorDoc, { kind: 'move', nodeId: childBtn.id }, leaveTarget);
  assert.equal(leftPreview.ok, true);
  const movedProps = findNode(leftPreview.document.root, childBtn.id).props;
  for (const k of ANCHOR_PROP_KEYS) {
    assert.equal(k in movedProps, false);
  }

  // Smart sibling snapping & grab-offset resolveDropTarget support inside AnchorBox.
  const freeRootRect = { x: 0, y: 0, w: 860, h: 540, visible: true };
  const siblingRects = [{ x: 180, y: 120, w: 120, h: 36, visible: true }];
  const sibSnap = resolveAnchorPlacement(freeRootRect, 242, 300, 120, 36, 16, siblingRects);
  assert.equal(sibSnap.snapX, 240);
  assert.equal(sibSnap.previewRect.x, 180);
  assert.equal(sibSnap.previewRect.w, 120);

  let twoNodeAnchor = cloneDoc(STUDIO_TEMPLATES.starter.document);
  const sideLabel = createNode('Label', twoNodeAnchor);
  twoNodeAnchor = insertNode(twoNodeAnchor, 'root', 1, sideLabel).document;
  twoNodeAnchor = updateNodeProps(twoNodeAnchor, sideLabel.id, { anchorPosition: 'TopLeft', anchorMargin: 16, anchorOffsetX: 164, anchorOffsetY: 104 }).document;
  const twoRects = {
    root: freeRootRect,
    button_1: { x: 380, y: 252, w: 100, h: 36, visible: true },
    [sideLabel.id]: { x: 180, y: 120, w: 120, h: 36, visible: true },
  };
  const leftEdgeGrab = resolveDropTarget(twoNodeAnchor, twoRects, 262, 300, { kind: 'move', nodeId: 'button_1' }, 'expanded', {
    anchorX: 232,
    anchorY: 300,
    dragSize: { w: 100, h: 36 },
  });
  assert.equal(leftEdgeGrab.targetParentId, 'root');
  assert.equal(leftEdgeGrab.anchorPlacement.snapX, 180);
  assert.equal(leftEdgeGrab.anchorPlacement.previewRect.x, 180);

  const centerGrab = resolveDropTarget(twoNodeAnchor, twoRects, 272, 300, { kind: 'move', nodeId: 'button_1' }, 'expanded', {
    anchorX: 242,
    anchorY: 300,
    dragSize: { w: 100, h: 36 },
  });
  assert.equal(centerGrab.anchorPlacement.snapX, 240);
  assert.equal(centerGrab.anchorPlacement.previewRect.x, 190);
});

test('history replaceCurrent and mapInteractionEventToProps synchronize Play interactions accurately', () => {
  const base = cloneDoc(STUDIO_TEMPLATES.settings.document);
  const hist = createHistory(base);
  const step1 = updateNodeProps(base, 'switch_motion', { isOn: false }).document;
  hist.push(step1);
  assert.equal(hist.canUndo(), true);
  const step1Coalesced = updateNodeProps(step1, 'slider_scale', { value: 140 }).document;
  hist.replaceCurrent(step1Coalesced);
  assert.equal(findNode(hist.current().root, 'slider_scale').props.value, 140);
  hist.undo();
  assert.equal(findNode(hist.current().root, 'switch_motion').props.isOn, true);

  assert.deepEqual(mapInteractionEventToProps({ type: 'ToggleSwitch' }, 'toggled', false), { isOn: false });
  assert.deepEqual(mapInteractionEventToProps({ type: 'CheckBox' }, 'toggled', false), { checked: false, checkState: 'Unchecked' });
  assert.deepEqual(mapInteractionEventToProps({ type: 'CheckBox' }, 'checkStateChanged', 'PartiallyChecked'), { checked: false, checkState: 'PartiallyChecked' });
  assert.deepEqual(mapInteractionEventToProps({ type: 'Slider' }, 'valueChanged', 125), { value: 125 });
  assert.deepEqual(mapInteractionEventToProps({ type: 'RangeSlider' }, 'valuesChanged', '15..85'), { lowerValue: 15, upperValue: 85 });
  assert.deepEqual(mapInteractionEventToProps({ type: 'ComboBox' }, 'currentIndexChanged', 2), { currentIndex: 2 });
  assert.deepEqual(mapInteractionEventToProps({ type: 'Expander' }, 'expandedChanged', false), { expanded: false });
  assert.deepEqual(mapInteractionEventToProps({ type: 'CodeBlock' }, 'expandedChanged', false), { expanded: false });
  assert.deepEqual(mapInteractionEventToProps({ type: 'TextEdit' }, 'textChanged', 'hello'), { plainText: 'hello' });
  assert.deepEqual(mapInteractionEventToProps({ type: 'TabView' }, 'selectedIndexChanged', 1), { selectedIndex: 1 });
  assert.deepEqual(mapInteractionEventToProps({ type: 'Stepper' }, 'stepRequested', 2), { currentIndex: 2 });
  assert.deepEqual(mapInteractionEventToProps({ type: 'PipsPager' }, 'pageChanged', 3), { selectedPageIndex: 3 });
  assert.equal(mapInteractionEventToProps({ type: 'Button' }, 'clicked', true), null);
});

test('shared documents validate typed properties and budget before import or export', () => {
  const title = 'Research <notes> & "UI"';
  const doc = cloneDoc(STUDIO_TEMPLATES.starter.document);
  const node = doc.root.children[0];
  node.props.text = title;
  assert.deepEqual(decodeShareState(encodeShareState(doc)).document, doc);
  for (const patch of [{ fluentStyle: 'not a style' }, { anchorOffsetX: 'twenty' },
                        { text: {} }, { checked: 'false' }]) {
    const bad = cloneDoc(doc);
    Object.assign(bad.root.children[0].props, patch);
    assert.equal(validateDocument(bad).ok, false);
    for (const emit of [generateCpp, generatePython, generateDesignerUi, encodeShareState]) {
      assert.throws(() => emit(bad));
    }
  }
  assert.equal(decodeShareState('v1.' + 'A'.repeat(180000)).ok, false);
  node.props.text = title.repeat(16000);
  assert.throws(() => encodeShareState(doc), /too large/);
});

test('studio-ai catalog summary, JSON extraction, and sanitizer repair imperfect LLM output', () => {
  const summary = buildCatalogSummary();
  for (const type of Object.keys(STUDIO_CATALOG)) {
    assert.ok(summary.includes(type), `Catalog summary missing ${type}`);
  }

  const wrapped = 'Here is your Fluent-Qt layout:\n```json\n{"root":{"id":"bad-id","type":"PushButton","props":{"text":"Save {ok}","fluentStyle":"accent","unknownProp":42},"children":[{"id":"ignored","type":"Label"}]}}\n```\nHope it helps!';
  const extracted = extractJsonFromText(wrapped);
  assert.equal(extracted.ok, true);
  assert.equal(extracted.data.root.type, 'PushButton');
  assert.equal(extractJsonFromText('not json at all').ok, false);

  // Repair alias types, hyphenated duplicate IDs, Field >1 child, and coerced props
  const messy = {
    root: {
      id: 'page-root',
      type: 'Column',
      props: { spacing: '16', padding: 20 },
      children: [
        { id: 'dup', type: 'Heading', props: { text: 'Settings', typography: 'title' } },
        {
          id: 'dup',
          type: 'FormField',
          props: { labelText: 'Port', required: 'true' },
          children: [
            { id: 'num-1', type: 'SpinBox', props: { value: '8080', minimum: 1, maximum: 65535 } },
            { id: 'extra-child', type: 'Button', props: { text: 'Trimmed' } },
          ],
        },
        { id: 'sw-1', type: 'Switch', props: { isOn: 'true' } },
        { id: 'bad-widget', type: 'NonExistentWidget' },
      ],
    },
  };
  const sanitized = sanitizeAiDocument(messy);
  assert.equal(sanitized.ok, true, sanitized.error);
  assert.equal(sanitized.document.root.id, 'root');
  assert.equal(sanitized.document.root.type, 'VBox');
  assert.equal(sanitized.document.root.props.spacing, 16);
  const label = sanitized.document.root.children[0];
  assert.equal(label.type, 'Label');
  assert.equal(label.props.typography, 'Title');
  const field = sanitized.document.root.children[1];
  assert.equal(field.type, 'Field');
  assert.equal(field.props.required, true);
  assert.equal(field.children.length, 1);
  assert.equal(field.children[0].type, 'NumberBox');
  assert.equal(field.children[0].props.value, 8080);
  assert.equal(sanitized.document.root.children[2].type, 'ToggleSwitch');
  assert.equal(validateDocument(sanitized.document).ok, true);

  // Repair loose multi-child NavigationView into 1 VBox page and demote nested Card inside Card.
  const navLoose = sanitizeAiDocument({
    root: {
      id: 'root',
      type: 'VBox',
      children: [
        {
          id: 'nav',
          type: 'NavigationView',
          props: { items: ['Overview', 'Logs'] },
          children: [
            { id: 't1', type: 'Label', props: { text: 'Dashboard' } },
            {
              id: 'outer_card',
              type: 'Card',
              children: [
                { id: 'inner_card', type: 'Card', children: [{ id: 'b1', type: 'Button', props: { text: 'Run' } }] },
              ],
            },
          ],
        },
      ],
    },
  });
  assert.equal(navLoose.ok, true, navLoose.error);
  const navNode = findNode(navLoose.document.root, 'nav');
  assert.equal(navNode.children.length, 1);
  assert.equal(navNode.children[0].type, 'VBox');
  const outerCard = findNode(navLoose.document.root, 'outer_card');
  assert.equal(outerCard.type, 'Card');
  assert.equal(outerCard.children[0].type, 'VBox');

  // Preserve properly wrapped multi-page NavigationView containers.
  const navPaged = sanitizeAiDocument({
    root: {
      id: 'root',
      type: 'VBox',
      children: [
        {
          id: 'nav',
          type: 'NavigationView',
          props: { items: ['Page 1', 'Page 2'] },
          children: [
            { id: 'p0', type: 'VBox', children: [{ id: 'l0', type: 'Label', props: { text: 'One' } }] },
            { id: 'p1', type: 'VBox', children: [{ id: 'l1', type: 'Label', props: { text: 'Two' } }] },
          ],
        },
      ],
    },
  });
  assert.equal(navPaged.ok, true);
  assert.equal(findNode(navPaged.document.root, 'nav').children.length, 2);
  assert.equal(findNode(navPaged.document.root, 'nav').children[0].id, 'p0');
});

test('studio-ai builds desktop architect prompt and presets cleanly', () => {
  assert.ok(AI_PROVIDER_PRESETS.deepseek?.baseUrl);
  assert.ok(AI_PROVIDER_PRESETS.ollama?.baseUrl);

  const msgs = buildAiMessages({ prompt: 'Build a GPU monitor', lang: 'en' });
  assert.equal(msgs.length, 2);
  assert.equal(msgs[0].role, 'system');
  assert.match(msgs[0].content, /NavigationView/);
  assert.match(msgs[0].content, /SplitView/);
  assert.match(msgs[0].content, /DataGrid\.modelData/);
  assert.match(msgs[1].content, /create new desktop page/);
  assert.match(msgs[1].content, /Build a GPU monitor/);
});

function makeSseStreamResponse(payloadStrings) {
  const encoder = new TextEncoder();
  const stream = new ReadableStream({
    start(controller) {
      for (const item of payloadStrings) {
        controller.enqueue(encoder.encode(`data: ${item}\n\n`));
      }
      controller.close();
    },
  });
  return new Response(stream, {
    status: 200,
    headers: { 'content-type': 'text/event-stream' },
  });
}

function makeOpenAiResponsesSse({
  text,
  reasoning = '',
  model = 'gpt-6-luna',
  finishIncomplete = false,
  usage = { input_tokens: 1800, output_tokens: 240, input_tokens_details: { cached_tokens: 1440 } },
}) {
  const events = [
    JSON.stringify({
      type: 'response.created',
      response: { id: 'resp_1', created_at: 1700000000, model },
    }),
  ];
  let outIdx = 0;
  if (reasoning) {
    events.push(JSON.stringify({
      type: 'response.output_item.added',
      output_index: outIdx,
      item: { type: 'reasoning', id: 'rs_1' },
    }));
    events.push(JSON.stringify({
      type: 'response.reasoning_summary_text.delta',
      item_id: 'rs_1',
      output_index: outIdx,
      summary_index: 0,
      delta: reasoning,
    }));
    events.push(JSON.stringify({
      type: 'response.output_item.done',
      output_index: outIdx,
      item: { type: 'reasoning', id: 'rs_1' },
    }));
    outIdx++;
  }
  events.push(JSON.stringify({
    type: 'response.output_item.added',
    output_index: outIdx,
    item: { type: 'message', id: 'msg_1' },
  }));
  const mid = Math.max(1, Math.floor(text.length / 2));
  for (const part of [text.slice(0, mid), text.slice(mid)].filter(Boolean)) {
    events.push(JSON.stringify({
      type: 'response.output_text.delta',
      item_id: 'msg_1',
      output_index: outIdx,
      delta: part,
    }));
  }
  events.push(JSON.stringify({
    type: 'response.output_item.done',
    output_index: outIdx,
    item: { type: 'message', id: 'msg_1' },
  }));
  events.push(JSON.stringify({
    type: finishIncomplete ? 'response.incomplete' : 'response.completed',
    response: {
      ...(finishIncomplete ? { incomplete_details: { reason: 'max_output_tokens' } } : {}),
      usage,
    },
  }));
  return makeSseStreamResponse(events);
}

function makeChatSse({
  text,
  model = 'deepseek-chat',
  finishReason = 'stop',
  usage = { prompt_tokens: 1800, completion_tokens: 240, prompt_tokens_details: { cached_tokens: 1440 } },
}) {
  const mid = Math.max(1, Math.floor(text.length / 2));
  const chunks = [text.slice(0, mid), text.slice(mid)].filter(Boolean);
  const events = chunks.map((chunk, i) => JSON.stringify({
    id: 'chatcmpl_1',
    object: 'chat.completion.chunk',
    model,
    choices: [{
      index: 0,
      delta: i === 0 ? { role: 'assistant', content: chunk } : { content: chunk },
      finish_reason: null,
    }],
  }));
  events.push(JSON.stringify({
    id: 'chatcmpl_1',
    object: 'chat.completion.chunk',
    model,
    choices: [{ index: 0, delta: {}, finish_reason: finishReason }],
    ...(usage ? { usage } : {}),
  }));
  events.push('[DONE]');
  return makeSseStreamResponse(events);
}

test('studio-ai generateStudioPageWithAi stays honest without config, retries custom JSON, and handles worker limits', async () => {
  const unconfigured = await generateStudioPageWithAi({
    prompt: 'Create a dashboard',
    config: { provider: 'auto', workerUrl: '' },
  });
  assert.equal(unconfigured.ok, false);
  assert.equal(unconfigured.needsConfig, true);

  let customCalls = 0;
  const seenChatUrls = [];
  const customFetch = async (url) => {
    seenChatUrls.push(url);
    customCalls++;
    const content = customCalls === 1
      ? 'Not valid JSON'
      : JSON.stringify({
          schemaVersion: 1,
          root: {
            id: 'root',
            type: 'VBox',
            children: [
              { id: 'title_1', type: 'Label', props: { text: 'Remote Custom Page' } },
              { id: 'btn_1', type: 'Button', props: { text: 'Apply' } },
            ],
          },
        });
    return makeChatSse({ text: content, model: 'test-model' });
  };
  const customRes = await generateStudioPageWithAi({
    prompt: 'Build a page',
    config: { provider: 'custom', baseUrl: 'https://api.example.com/v1', model: 'test-model', apiKey: 'sk-test' },
    fetchImpl: customFetch,
  });
  assert.equal(customRes.ok, true, customRes.error);
  assert.equal(customCalls, 2);
  assert.deepEqual(seenChatUrls, [
    'https://api.example.com/v1/chat/completions',
    'https://api.example.com/v1/chat/completions',
  ]);
  assert.equal(customRes.source, 'custom');
  assert.equal(findNode(customRes.document.root, 'title_1').props.text, 'Remote Custom Page');

  // Worker route success
  const workerRes = await generateStudioPageWithAi({
    prompt: 'Create login form',
    config: { provider: 'auto', workerUrl: 'https://worker.example.dev' },
    fetchImpl: async () => ({
      ok: true,
      status: 200,
      json: async () => ({
        ok: true,
        provider: 'cloudflare-workers-ai',
        model: '@cf/meta/llama-3.1-8b-instruct',
        content: '{"root":{"type":"VBox","children":[{"id":"l1","type":"Label","props":{"text":"Worker Login"}}]}}',
      }),
    }),
  });
  assert.equal(workerRes.ok, true);
  assert.equal(workerRes.source, 'worker');
  assert.equal(workerRes.document.root.children[0].props.text, 'Worker Login');

  // Worker rate-limit reports explicit error rather than silently faking an AI response
  const limitRes = await generateStudioPageWithAi({
    prompt: 'Create a settings page',
    config: { provider: 'auto', workerUrl: 'https://worker.example.dev' },
    fetchImpl: async () => ({ ok: false, status: 429, json: async () => ({ error: 'Daily free quota reached.' }) }),
  });
  assert.equal(limitRes.ok, false);
  assert.equal(limitRes.rateLimited, true);
  assert.match(limitRes.error, /Daily free quota/);
});

test('studio-ai routes OpenAI Responses, Chat Completions, keyless Ollama, and formats SDK API errors', async () => {
  const responsesDoc = {
    schemaVersion: 1,
    root: {
      id: 'root',
      type: 'VBox',
      children: [
        { id: 'editor_title', type: 'Label', props: { text: 'Markdown Notes' } },
        { id: 'editor_area', type: 'TextEdit', props: { placeholderText: 'Write here...' } },
      ],
    },
  };

  // 1. Both https://api.openai.com/v1 and https://api.openai.com/v1/responses/ route cleanly to Responses API
  for (const baseUrl of ['https://api.openai.com/v1', 'https://api.openai.com/v1/responses/']) {
    let seenUrl = '';
    let seenBody = null;
    const progressEvents = [];
    const res = await generateStudioPageWithAi({
      prompt: '生成一个文本编辑器',
      lang: 'zh',
      config: {
        provider: 'custom',
        baseUrl,
        model: 'gpt-6-luna',
        apiKey: 'sk-openai-test',
      },
      fetchImpl: async (url, init) => {
        seenUrl = url;
        seenBody = JSON.parse(init.body);
        return makeOpenAiResponsesSse({
          reasoning: 'Planning layout',
          text: JSON.stringify(responsesDoc),
          usage: { input_tokens: 1920, output_tokens: 210, input_tokens_details: { cached_tokens: 1536 } },
        });
      },
      onProgress: (evt) => progressEvents.push(evt),
    });
    assert.equal(res.ok, true, res.error);
    assert.equal(seenUrl, 'https://api.openai.com/v1/responses');
    assert.equal(seenBody.temperature, undefined);
    assert.equal(seenBody.max_output_tokens, 8192);
    assert.equal(seenBody.prompt_cache_key, 'fluentqt_studio_v1');
    assert.equal(seenBody.reasoning?.effort, 'medium');
    assert.equal(seenBody.reasoning?.summary, undefined);
    assert.equal(seenBody.text?.format?.type, 'json_object');
    assert.ok(progressEvents.some((e) => e.phase === 'thinking'));
    assert.ok(progressEvents.some((e) => e.phase === 'generating' && e.widgets >= 2));
    assert.deepEqual(res.usage, {
      inputTokens: 1920,
      outputTokens: 210,
      cachedTokens: 1536,
      cacheHitRate: 80,
    });
    assert.equal(findNode(res.document.root, 'editor_area').type, 'TextEdit');
  }

  // 2. Full /chat/completions URL & localhost keyless Ollama do not double-append paths
  let seenChatUrl = '';
  let seenChatBody = null;
  const directChatRes = await generateStudioPageWithAi({
    prompt: 'Create a text editor',
    config: {
      provider: 'custom',
      baseUrl: 'https://api.example.com/v1/chat/completions',
      model: 'gpt-4o-mini',
      apiKey: 'sk-test',
    },
    fetchImpl: async (url, init) => {
      seenChatUrl = url;
      seenChatBody = JSON.parse(init.body);
      return makeChatSse({ text: JSON.stringify(responsesDoc), model: 'gpt-4o-mini' });
    },
  });
  assert.equal(directChatRes.ok, true, directChatRes.error);
  assert.equal(seenChatUrl, 'https://api.example.com/v1/chat/completions');
  assert.equal(seenChatBody.stream_options?.include_usage, true);

  let seenOllamaUrl = '';
  let ollamaAuthHeader;
  const ollamaRes = await generateStudioPageWithAi({
    prompt: 'Local page',
    config: {
      provider: 'custom',
      baseUrl: 'http://localhost:11434/v1',
      model: 'qwen2.5:7b',
      apiKey: '',
    },
    fetchImpl: async (url, init) => {
      seenOllamaUrl = url;
      ollamaAuthHeader = init.headers?.authorization ?? init.headers?.Authorization;
      return makeChatSse({ text: JSON.stringify(responsesDoc), model: 'qwen2.5:7b', usage: null });
    },
  });
  assert.equal(ollamaRes.ok, true, ollamaRes.error);
  assert.equal(seenOllamaUrl, 'http://localhost:11434/v1/chat/completions');
  assert.equal(ollamaAuthHeader, undefined);

  // 3. SDK APICallError 401 formats cleanly without retries
  let authCalls = 0;
  const authErrRes = await generateStudioPageWithAi({
    prompt: 'Create a page',
    config: {
      provider: 'custom',
      baseUrl: 'https://api.openai.com/v1',
      model: 'gpt-4o-mini',
      apiKey: 'sk-bad',
    },
    fetchImpl: async () => {
      authCalls++;
      return new Response(
        JSON.stringify({ error: { message: 'Incorrect API key provided', type: 'invalid_request_error' } }),
        { status: 401, headers: { 'content-type': 'application/json' } },
      );
    },
  });
  assert.equal(authCalls, 1);
  assert.equal(authErrRes.ok, false);
  assert.equal(authErrRes.error, 'HTTP 401: Incorrect API key provided');
});

test('studio-ai static system prompt maximizes cache hit rate, rescues stray braces, and skips truncated retries', async () => {
  const mZh = buildAiMessages({ prompt: '做一个文本编辑器', lang: 'zh' });
  const mEn = buildAiMessages({ prompt: 'Create a dashboard', lang: 'en' });
  assert.equal(mZh[0].content, mEn[0].content);
  assert.ok(mZh[0].content.length >= 4200 && mZh[0].content.length <= 9200, `Unexpected prompt length: ${mZh[0].content.length}`);
  for (const type of Object.keys(STUDIO_CATALOG)) {
    assert.ok(mZh[0].content.includes(type), `System prompt missing ${type}`);
  }

  // Design metadata, extended Button/FontIcon glyphs, DataGrid headers, and safe/unsafe SVG handling
  const designPayload = JSON.stringify({
    schemaVersion: 1,
    design: { theme: 'dark', accentColor: '#8764b8' },
    root: {
      id: 'root',
      type: 'VBox',
      props: { spacing: 12, padding: 14 },
      children: [
        { id: 'btn_cam', type: 'Button', props: { text: 'Shot', icon: 'Camera', fluentStyle: 'Accent' } },
        { id: 'fi_mus', type: 'FontIcon', props: { glyph: 'Music' } },
        {
          id: 'dg',
          type: 'DataGrid',
          props: { headers: ['Node', 'Status'], modelData: [['node-1', 'Online']] },
        },
        {
          id: 'iv_ok',
          type: 'ImageViewer',
          props: { svg: '<svg viewBox="0 0 120 80" xmlns="http://www.w3.org/2000/svg"><rect width="120" height="80" fill="#8764B8"/></svg>' },
        },
        {
          id: 'iv_xss',
          type: 'ImageViewer',
          props: {
            svg: '<svg viewBox="0 0 10 10"><script>alert(1)</script></svg>',
            imageUrl: 'https://evil.example.com/payload.png',
            mediaTheme: 'Music',
          },
        },
      ],
    },
  });
  const cleaned = sanitizeAiDocument(designPayload);
  assert.equal(cleaned.ok, true, cleaned.error);
  assert.deepEqual(cleaned.document.design, { theme: 'dark', accentColor: '#8764B8' });
  assert.equal(validateDocument(cleaned.document).ok, true);
  const ivOk = findNode(cleaned.document.root, 'iv_ok');
  const ivXss = findNode(cleaned.document.root, 'iv_xss');
  assert.match(ivOk.props.svg, /^<svg\b/);
  assert.equal(ivXss.props.svg, undefined);
  assert.equal(ivXss.props.imageUrl, undefined);
  assert.equal(ivXss.props.mediaTheme, 'Music');

  const cppOut = generateCpp(cleaned.document);
  const pyOut = generatePython(cleaned.document);
  assert.match(cppOut, /setImage\(\w+\.read\(\)\)/);
  assert.match(pyOut, /setImage\(\w+\.read\(\)\)/);
  assert.ok(cppOut.includes('viewBox=\\"0 0 120 80\\"'));
  assert.ok(pyOut.includes('viewBox=\\"0 0 120 80\\"'));
  assert.doesNotMatch(cppOut, /applyAccentOverride/);
  assert.match(cppOut, /setIconGlyph\(Typography::Icons::Camera\)/);
  assert.match(cppOut, /setHorizontalHeaderLabels\(\{QStringLiteral\("Node"\), QStringLiteral\("Status"\)\}\)/);
  assert.doesNotMatch(pyOut, /set_accent_color/);
  assert.match(pyOut, /setIconGlyph\(Typography\.Icons\.Camera\)/);
  assert.match(pyOut, /setHorizontalHeaderLabels\(\["Node", "Status"\]\)/);

  // Stray unclosed `{` in prose before valid JSON must not trigger Unclosed JSON failure
  const strayText = 'Thinking note {draft layout:\n' + JSON.stringify({
    root: { id: 'root', type: 'VBox', children: [{ id: 't1', type: 'Label', props: { text: 'Rescued' } }] },
  });
  const rescued = extractJsonFromText(strayText);
  assert.equal(rescued.ok, true);
  assert.equal(rescued.data.root.children[0].props.text, 'Rescued');

  // Truncated Chat output (finish_reason: length) must NOT make a second costly retry request
  let chatTruncCalls = 0;
  const chatTruncRes = await generateStudioPageWithAi({
    prompt: 'Build giant workspace',
    lang: 'zh',
    config: {
      provider: 'custom',
      baseUrl: 'https://api.deepseek.com/v1',
      model: 'deepseek-chat',
      apiKey: 'sk-test',
    },
    fetchImpl: async () => {
      chatTruncCalls++;
      return makeChatSse({
        text: '{"root":{"id":"root","type":"VBox"',
        finishReason: 'length',
        usage: { prompt_tokens: 1500, completion_tokens: 4096 },
      });
    },
  });
  assert.equal(chatTruncCalls, 1);
  assert.equal(chatTruncRes.ok, false);
  assert.equal(chatTruncRes.truncated, true);

  // Truncated Responses output (response.incomplete) must also skip second retry
  let respTruncCalls = 0;
  const respTruncRes = await generateStudioPageWithAi({
    prompt: 'Build giant workspace',
    lang: 'en',
    config: {
      provider: 'custom',
      baseUrl: 'https://api.openai.com/v1',
      model: 'gpt-6-luna',
      apiKey: 'sk-openai-test',
    },
    fetchImpl: async () => {
      respTruncCalls++;
      return makeOpenAiResponsesSse({
        text: '{"root":{"id":"root","type":"VBox"',
        finishIncomplete: true,
        usage: { input_tokens: 1600, output_tokens: 4096 },
      });
    },
  });
  assert.equal(respTruncCalls, 1);
  assert.equal(respTruncRes.ok, false);
  assert.equal(respTruncRes.truncated, true);
});

test('studio-ai conversational iteration, selected-node refine, and style presets keep static system prompt cached', async () => {
  const baseDoc = {
    schemaVersion: 1,
    design: { theme: 'dark', accentColor: '#00B7C3' },
    root: {
      id: 'root',
      type: 'VBox',
      props: { spacing: 12, padding: 14 },
      children: [
        { id: 'hdr_lbl', type: 'Label', props: { text: 'Agent Workbench', typography: 'Title' } },
        {
          id: 'composer_card',
          type: 'Card',
          props: { padding: 12, spacing: 8 },
          children: [
            { id: 'composer_input', type: 'TextEdit', props: { placeholderText: 'Ask anything...' } },
            { id: 'send_btn', type: 'Button', props: { text: 'Send', fluentStyle: 'Accent' } },
          ],
        },
      ],
    },
  };

  const baseSys = buildAiMessages({ prompt: 'a', lang: 'zh' })[0].content;
  for (const mode of ['create', 'iterate', 'refine']) {
    for (const preset of Object.keys(AI_STYLE_PRESETS)) {
      const msgs = buildAiMessages({
        prompt: 'Add clear button',
        lang: mode === 'refine' ? 'en' : 'zh',
        mode,
        currentDoc: baseDoc,
        targetNodeId: 'composer_card',
        stylePreset: preset,
      });
      assert.equal(msgs[0].content, baseSys);
    }
  }

  const refineMsgs = buildAiMessages({
    prompt: 'Add an attachment button next to Send',
    lang: 'en',
    mode: 'refine',
    currentDoc: baseDoc,
    targetNodeId: 'composer_card',
  });
  assert.match(refineMsgs[1].content, /Mode: refine selected node subtree/);
  assert.match(refineMsgs[1].content, /"id":"composer_card"/);

  // applyRefinedSubtree replaces only the targeted node and preserves siblings + design
  const refined = applyRefinedSubtree(baseDoc, 'composer_card', JSON.stringify({
    id: 'composer_card',
    type: 'Card',
    props: { padding: 12, spacing: 8 },
    children: [
      { id: 'composer_input', type: 'TextEdit', props: { placeholderText: 'Ask or attach...' } },
      {
        id: 'composer_actions',
        type: 'HBox',
        props: { spacing: 8, align: 'Center' },
        children: [
          { id: 'attach_btn', type: 'Button', props: { text: 'Attach', icon: 'Link' } },
          { id: 'send_btn', type: 'Button', props: { text: 'Send', icon: 'Send', fluentStyle: 'Accent' } },
        ],
      },
    ],
  }));
  assert.equal(refined.ok, true, refined.error);
  assert.equal(refined.focusNodeId, 'composer_card');
  assert.deepEqual(refined.document.design, { theme: 'dark', accentColor: '#00B7C3' });
  assert.equal(findNode(refined.document.root, 'hdr_lbl').props.text, 'Agent Workbench');
  assert.equal(findNode(refined.document.root, 'attach_btn').props.icon, 'Link');

  // Style preset applies default design when AI omits design in create mode
  let seenIterateUser = '';
  const iterateRes = await generateStudioPageWithAi({
    prompt: 'Rename title to Ops Console',
    lang: 'en',
    mode: 'iterate',
    currentDoc: baseDoc,
    config: { provider: 'custom', baseUrl: 'https://api.deepseek.com/v1', model: 'deepseek-chat', apiKey: 'sk-test' },
    fetchImpl: async (_url, init) => {
      const body = JSON.parse(init.body);
      seenIterateUser = body.messages[1].content;
      const updated = structuredClone(baseDoc);
      updated.root.children[0].props.text = 'Ops Console';
      delete updated.design;
      return new Response(
        `data: ${JSON.stringify({ id: 'c1', model: 'deepseek-chat', choices: [{ index: 0, delta: { content: JSON.stringify(updated) }, finish_reason: 'stop' }] })}\n\ndata: [DONE]\n\n`,
        { status: 200, headers: { 'content-type': 'text/event-stream' } },
      );
    },
  });
  assert.equal(iterateRes.ok, true, iterateRes.error);
  assert.match(seenIterateUser, /Mode: iterate current desktop page/);
  assert.deepEqual(iterateRes.document.design, { theme: 'dark', accentColor: '#00B7C3' });

  const createPresetRes = await generateStudioPageWithAi({
    prompt: 'Simple note card',
    lang: 'zh',
    mode: 'create',
    stylePreset: 'editorial-coral',
    config: { provider: 'custom', baseUrl: 'https://api.deepseek.com/v1', model: 'deepseek-chat', apiKey: 'sk-test' },
    fetchImpl: async () => new Response(
      `data: ${JSON.stringify({ id: 'c2', model: 'deepseek-chat', choices: [{ index: 0, delta: { content: JSON.stringify({ root: { id: 'root', type: 'VBox', children: [{ id: 'l1', type: 'Label', props: { text: '笔记' } }] } }) }, finish_reason: 'stop' }] })}\n\ndata: [DONE]\n\n`,
      { status: 200, headers: { 'content-type': 'text/event-stream' } },
    ),
  });
  assert.equal(createPresetRes.ok, true, createPresetRes.error);
  assert.deepEqual(createPresetRes.document.design, { theme: 'light', accentColor: '#D84A38' });
});

test('Worker iteration includes the current document and refinement changes only the selected subtree', async () => {
  const { default: worker } = await import('./studio-ai-worker/worker.js');
  const original = { schemaVersion: 1, design: { theme: 'light' },
    root: { id: 'root', type: 'VBox', children: [
      { id: 'save', type: 'Button', props: { text: 'Save' } },
      { id: 'heading', type: 'Label', props: { text: 'Original heading' } },
    ] } };
  const updated = structuredClone(original);
  updated.root.children[0].props.text = 'Confirm';
  updated.root.children.pop();
  let userMessage;
  const fetchImpl = (_url, init) => worker.fetch(new Request('https://worker.example', {
    ...init, headers: { ...init.headers, Origin: 'https://studio.example' },
  }), {
    ALLOWED_ORIGINS: 'https://studio.example',
    STUDIO_QUOTA: { idFromName: id => id, get: () => ({ fetch: () => new Response('') }) },
    AI: { run: async (_model, { messages }) => {
      userMessage = messages[1].content;
      return { response: JSON.stringify(updated) };
    } },
  });
  const options = { currentDoc: original, prompt: 'Rename the button', lang: 'en',
    config: { provider: 'auto', workerUrl: 'https://worker.example' }, fetchImpl };
  const iteration = await generateStudioPageWithAi({ ...options, mode: 'iterate' });
  assert.ok(iteration.ok, iteration.error);
  assert.match(userMessage, /Original heading/);
  const refined = await generateStudioPageWithAi({ ...options, mode: 'refine', targetNodeId: 'save' });
  assert.ok(refined.ok, refined.error);
  assert.match(userMessage, /Target node id: "save"/);
  assert.equal(refined.document.root.children[0].props.text, 'Confirm');
  assert.deepEqual(refined.document.root.children[1], original.root.children[1]);
  assert.deepEqual(refined.document.design, original.design);
  assert.equal(applyRefinedSubtree(original, 'heading', updated).ok, false);
});

test('custom key belongs to its service origin', () => {
  const config = { baseUrl: 'https://provider.example/v1', keyOrigin: 'https://provider.example', apiKey: 'test-key' };
  changeAiEndpoint(config, 'https://provider.example/custom');
  assert.equal(config.apiKey, 'test-key');
  changeAiEndpoint(config, 'https://other.example/v1');
  assert.equal(config.apiKey, '');
  assert.equal(config.keyOrigin, 'https://other.example');
});

test('gateway quota survives a new instance and keeps rolling windows across UTC midnight', async (t) => {
  const sqlite = process.getBuiltinModule?.('node:sqlite');
  if (!sqlite) return t.skip('requires Node with built-in SQLite');

  const { StudioQuota } = await import('./studio-ai-worker/worker.js');
  const db = new sqlite.DatabaseSync(':memory:');
  t.after(() => db.close());
  const storage = {
    sql: { exec: (query, ...args) => db.prepare(query).all(...args) },
    transactionSync: (callback) => {
      db.exec('BEGIN');
      try {
        const result = callback();
        db.exec('COMMIT');
        return result;
      } catch (error) {
        db.exec('ROLLBACK');
        throw error;
      }
    },
  };
  const limits = { RATE_LIMIT_PER_MIN: 1, RATE_LIMIT_PER_HOUR: 2, MAX_DAILY_REQUESTS: 2 };
  let now = Date.UTC(2026, 0, 1, 23, 59, 30);
  t.mock.method(Date, 'now', () => now);
  let quota = new StudioQuota({ storage }, limits);
  const admit = async (ip) => (await quota.fetch(new Request('https://quota.internal/admit', {
    method: 'POST', body: ip,
  }))).status;
  assert.equal(await admit('a'), 200);
  assert.equal(await admit('b'), 200);
  quota = new StudioQuota({ storage }, limits);
  assert.equal(await admit('c'), 429);
  now += 30_000;
  assert.equal(await admit('a'), 429);
  now += 30_000;
  assert.equal(await admit('a'), 200);
  now += 60_000;
  assert.equal(await admit('a'), 429);
  assert.equal(await admit('c'), 200);
});
