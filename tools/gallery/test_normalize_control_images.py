"""Regression coverage for transparent silhouette quality checks."""

import unittest

from PIL import Image, ImageDraw

from normalize_control_images import _aliased_silhouette_sides


class SilhouetteTests(unittest.TestCase):
    def test_binary_rounded_mask_is_rejected(self):
        image = Image.new("RGBA", (72, 72))
        ImageDraw.Draw(image).rounded_rectangle((3, 3, 68, 68), 16, fill="purple")
        self.assertEqual(
            set(_aliased_silhouette_sides(image)), {"left", "right", "top", "bottom"}
        )

    def test_partial_alpha_inside_does_not_hide_aliased_outline(self):
        image = Image.new("RGBA", (72, 72))
        draw = ImageDraw.Draw(image)
        draw.rounded_rectangle((3, 3, 68, 68), 16, fill="purple")
        draw.rectangle((20, 20, 50, 50), fill=(255, 255, 255, 160))
        self.assertGreaterEqual(len(_aliased_silhouette_sides(image)), 2)

    def test_antialiased_rounded_mask_is_accepted(self):
        image = Image.new("RGBA", (288, 288))
        ImageDraw.Draw(image).rounded_rectangle((12, 12, 272, 272), 64, fill="purple")
        image = image.resize((72, 72), Image.Resampling.LANCZOS)
        self.assertEqual(_aliased_silhouette_sides(image), [])

    def test_pixel_aligned_rectangles_do_not_need_translucent_edges(self):
        image = Image.new("RGBA", (72, 72))
        draw = ImageDraw.Draw(image)
        draw.rectangle((4, 4, 67, 30), fill="purple")
        draw.rectangle((16, 40, 55, 67), fill="purple")
        self.assertEqual(_aliased_silhouette_sides(image), [])

    def test_diagnostics_identify_the_aliased_side(self):
        image = Image.new("RGBA", (72, 72))
        draw = ImageDraw.Draw(image)
        draw.rounded_rectangle((3, 3, 68, 68), 16, fill="purple")
        draw.rectangle((3, 36, 68, 68), fill="purple")
        sides = _aliased_silhouette_sides(image)
        self.assertIn("top", sides)
        self.assertNotIn("bottom", sides)


if __name__ == "__main__":
    unittest.main()
