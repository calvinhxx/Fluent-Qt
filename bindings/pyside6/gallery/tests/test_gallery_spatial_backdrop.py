"""Bounded backdrop cache policy and native texture-lifetime contracts."""
import os
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

from PySide6.QtCore import QSizeF, Qt
from PySide6.QtGui import QPixmap
from PySide6.QtWidgets import QApplication

from fluentqt_gallery.spatial_support import SPATIAL_AVAILABLE
from fluentqt._qt_compat import delete_qobject


@unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
class GallerySpatialBackdropTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])

    def setUp(self):
        self.errors, self.old_hook = [], sys.excepthook
        sys.excepthook = lambda *error: self.errors.append(error)

    def tearDown(self):
        sys.excepthook = self.old_hook
        self.assertEqual(self.errors, [], "unhandled Qt callback error")

    def test_reservation_shortens_paint_strip_without_lowering_density(self):
        from fluentqt_gallery.spatial_controller import (
            _cache_plan, _CACHE_BUDGET_BYTES, _PAINT_SAMPLES, _GLYPH_SCRATCH_BYTES,
        )
        panels = [QSizeF(240, 900), QSizeF(1360, 900)]
        normal = _cache_plan(panels, 2, 16384)
        backdrop_bytes = 3200 * 1800 * 4
        plan = _cache_plan(panels, 2, 16384, backdrop_bytes=backdrop_bytes,
                           glyph_scratch_bytes=_GLYPH_SCRATCH_BYTES)
        self.assertIsNotNone(plan)
        self.assertEqual(plan[0], 4)
        self.assertEqual(plan[1], normal[1])
        self.assertLess(plan[2].height(), normal[2].height())
        panel_bytes = sum(size.width() * size.height() * 4 for size in plan[1])
        paint_bytes = plan[2].width() * plan[2].height() * (12 * _PAINT_SAMPLES + 4)
        self.assertLessEqual(backdrop_bytes + _GLYPH_SCRATCH_BYTES + panel_bytes + paint_bytes,
                             _CACHE_BUDGET_BYTES)
        for invalid in (-1, _CACHE_BUDGET_BYTES, _CACHE_BUDGET_BYTES + 1):
            self.assertIsNone(_cache_plan(panels, 2, 16384, backdrop_bytes=invalid))
        for invalid in (-1, _CACHE_BUDGET_BYTES - 1, _CACHE_BUDGET_BYTES):
            self.assertIsNone(_cache_plan(panels, 2, 16384, backdrop_bytes=1,
                                         glyph_scratch_bytes=invalid))

    def test_upload_is_keyed_by_content_size_dpr_and_context(self):
        from fluentqt_gallery import spatial_controller as compositor
        pixmap = QPixmap(240, 160)
        pixmap.fill(Qt.red)
        pixmap.setDevicePixelRatio(2)
        context = Mock()
        context.functions.return_value.glGetError.return_value = 0
        surface = Mock(owner=SimpleNamespace(backdrop=pixmap), backdrop_cache={}, backdrop_uploads=0,
                       max_dimension=16384)
        surface.context.return_value = context
        surface.clear_backdrop.side_effect = lambda: compositor._Surface.clear_backdrop(surface)
        texture = Mock()
        texture.create.return_value = True
        texture.isStorageAllocated.return_value = True
        real_type = compositor.QOpenGLTexture
        with patch.object(compositor, "QOpenGLTexture", wraps=real_type) as constructor:
            constructor.return_value = texture
            self.assertTrue(compositor._Surface.update_backdrop(surface))
            self.assertEqual(surface.backdrop_uploads, 1)
            texture.setSize.assert_called_once_with(240, 160)
            self.assertIsInstance(texture.setData.call_args.args[2], compositor.VoidPtr)
            compositor._Surface.invalidate_backdrop(surface)
            self.assertTrue(compositor._Surface.update_backdrop(surface))
            self.assertEqual(surface.backdrop_uploads, 1)
            self.assertEqual(constructor.call_count, 1)
            pixmap.setDevicePixelRatio(1.25)
            compositor._Surface.invalidate_backdrop(surface)
            texture.destroy.assert_called_once()
            self.assertTrue(compositor._Surface.update_backdrop(surface))
            self.assertEqual(surface.backdrop_uploads, 2)
            pixmap.fill(Qt.blue)
            compositor._Surface.invalidate_backdrop(surface)
            self.assertTrue(compositor._Surface.update_backdrop(surface))
            self.assertEqual(surface.backdrop_uploads, 3)
            replacement_context = Mock()
            replacement_context.functions.return_value.glGetError.return_value = 0
            surface.context.return_value = replacement_context
            compositor._Surface.invalidate_backdrop(surface)
            self.assertTrue(compositor._Surface.update_backdrop(surface))
            self.assertEqual(surface.backdrop_uploads, 4)
            compositor._Surface.clear_frame_caches(surface)
            self.assertEqual(surface.backdrop_cache, {})
            self.assertEqual(surface.caches, [{}, {}])
            self.assertIsNone(surface.plan)

    def test_failed_upload_releases_storage_and_does_not_publish_cache(self):
        from fluentqt_gallery import spatial_controller as compositor
        pixmap = QPixmap(16, 16)
        pixmap.fill(Qt.red)
        surface = Mock(owner=SimpleNamespace(backdrop=pixmap), backdrop_cache={}, backdrop_uploads=0,
                       max_dimension=16384)
        surface.context.return_value.functions.return_value.glGetError.return_value = 0x0505
        real_type = compositor.QOpenGLTexture
        with patch.object(compositor, "QOpenGLTexture", wraps=real_type) as constructor:
            texture = Mock()
            constructor.return_value = texture
            texture.create.return_value = True
            texture.isStorageAllocated.return_value = True
            self.assertFalse(compositor._Surface.update_backdrop(surface))
            texture.destroy.assert_called_once()
        self.assertEqual(surface.backdrop_cache, {})
        self.assertEqual(surface.backdrop_uploads, 0)

    def test_binding_upload_failure_releases_texture(self):
        from fluentqt_gallery import spatial_controller as compositor
        pixmap = QPixmap(16, 16)
        pixmap.fill(Qt.red)
        surface = Mock(owner=SimpleNamespace(backdrop=pixmap), backdrop_cache={}, backdrop_uploads=0,
                       max_dimension=16384)
        texture = Mock()
        texture.setData.side_effect = ValueError("unsupported pointer overload")
        with patch.object(compositor, "QOpenGLTexture", wraps=compositor.QOpenGLTexture) as constructor:
            constructor.return_value = texture
            self.assertFalse(compositor._Surface.update_backdrop(surface))
            texture.destroy.assert_called_once()
        self.assertEqual(surface.backdrop_cache, {})
        self.assertEqual(surface.backdrop_uploads, 0)

    def test_native_parent_destruction_releases_texture_with_current_context(self):
        if QApplication.platformName() in ("offscreen", "minimal", "vnc"):
            self.assertNotEqual(os.environ.get("FLUENTQT_REQUIRE_NATIVE_TEST"), "1",
                                "native lifetime lane requires a desktop QPA platform")
            self.skipTest("requires a native OpenGL texture; offscreen is not visual approval")
        from PySide6.QtCore import QEventLoop, QTimer
        from PySide6.QtTest import QTest
        from PySide6.QtWidgets import QWidget
        from shiboken6 import isValid
        from fluentqt_gallery import spatial_controller as compositor

        class BackdropSurface(compositor._Surface):
            def paintGL(self):
                self.invalidate_backdrop()
                self.upload_ok = self.update_backdrop()

        def wait_for(predicate):
            # PySide's QtTest does not bind the C++ qWaitFor template.
            for _ in range(60):
                if predicate():
                    return True
                loop = QEventLoop()
                QTimer.singleShot(50, loop.quit)
                loop.exec()
            return predicate()

        window = QWidget()
        pixmap = QPixmap(32, 32)
        pixmap.fill(Qt.red)
        owner = SimpleNamespace(window=window, backdrop=pixmap, queue_check=lambda: None,
                                context_lost=lambda: None, render_failure_timer=QTimer(window),
                                _capture_content=Mock())
        try:
            surface = BackdropSurface(owner, window)
            surface.resize(64, 64)
            window.show()
            self.assertTrue(QTest.qWaitForWindowExposed(window))
            self.assertTrue(wait_for(lambda: surface.backdrop_uploads > 0))
            self.assertTrue(surface.upload_ok)
            texture = surface.backdrop_cache["texture"]
            self.assertTrue(texture.isCreated())
            surface.clear_frame_caches()
            self.assertFalse(texture.isCreated())
            surface.update()
            self.assertTrue(wait_for(lambda: surface.backdrop_uploads == 2))
            texture = surface.backdrop_cache["texture"]
            delete_qobject(window)
            self.assertFalse(texture.isCreated(), "parent destruction must free the texture before its context")
        finally:
            if isValid(window):
                delete_qobject(window)


if __name__ == "__main__":
    unittest.main()
