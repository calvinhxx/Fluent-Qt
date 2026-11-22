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
