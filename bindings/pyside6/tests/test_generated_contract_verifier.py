"""Tests for the generated-wrapper contract verifier."""

from pathlib import Path
import subprocess
import sys
import tempfile
import textwrap
import unittest


VERIFIER = (
    Path(__file__).resolve().parents[1]
    / "tools"
    / "verify_generated_contracts.py"
)
WINDOW_WRAPPER = "fluent_windowing_window_wrapper.cpp"
WINDOWING_NAMESPACE_WRAPPER = "fluent_windowing_wrapper.cpp"
ENUM_CONVERTER = (
    "static void Enum_PythonToCpp_fluent_windowing_BackdropEffect"
)


def window_source(
    limited_api_arguments=True,
    expose_result_pointer=False,
    split_override=True,
):
    limited_api = (
        'auto args = Py_BuildValue("(NN)", first, second);'
        if limited_api_arguments
        else ""
    )
    pointer_exposure = (
        "auto exposed = Shiboken::Conversions::copyToPython(converter, result);"
        if expose_result_pointer
        else ""
    )
    if split_override:
        native_event = """
        bool WindowWrapper::nativeEvent(
            const QByteArray &, void *, long long *result)
        {{
            return this->::fluent::windowing::Window::nativeEvent(
                QByteArray(), nullptr, result);
        }}

        bool WindowWrapper::sbk_o_nativeEvent(
            const char *, const char *, Gil &, const Ref &,
            const QByteArray &, void *, long long *result)
        {{
            PyObject *pyArgArray[2] = {{first, second}};
            {limited_api}
            {pointer_exposure}
            return false;
        }}
        """
    else:
        native_event = """
        bool WindowWrapper::nativeEvent(
            const QByteArray &, void *, long long *result)
        {{
            if (useFallback)
                return this->::fluent::windowing::Window::nativeEvent(
                    QByteArray(), nullptr, result);
            {limited_api}
            {pointer_exposure}
            return false;
        }}
        """
    native_event = textwrap.dedent(native_event).format(
        limited_api=limited_api,
        pointer_exposure=pointer_exposure,
    )
    return textwrap.dedent(
        """
        bool WindowWrapper::eventFilter(QObject *, QEvent *)
        {{
            auto decoy = Py_BuildValue("(NN)", first, second);
            return decoy != nullptr;
        }}

        {native_event}
        """
    ).format(
        native_event=native_event,
    )


class GeneratedContractVerifierTest(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.generated_dir = Path(self.temporary_directory.name)
        self.write_window()
        self.write_namespace(1)

    def tearDown(self):
        self.temporary_directory.cleanup()

    def write_window(
        self,
        limited_api_arguments=True,
        expose_result_pointer=False,
        split_override=True,
    ):
        (self.generated_dir / WINDOW_WRAPPER).write_text(
            window_source(
                limited_api_arguments=limited_api_arguments,
                expose_result_pointer=expose_result_pointer,
                split_override=split_override,
            ),
            encoding="utf-8",
        )

    def write_namespace(self, converter_count):
        (self.generated_dir / WINDOWING_NAMESPACE_WRAPPER).write_text(
            "\n".join(ENUM_CONVERTER for _ in range(converter_count)),
            encoding="utf-8",
        )

    def run_verifier(self, *extra_arguments):
        return subprocess.run(
            [
                sys.executable,
                str(VERIFIER),
                "--generated-dir",
                str(self.generated_dir),
                *extra_arguments,
            ],
            check=False,
            capture_output=True,
            text=True,
        )

    def test_safe_native_event_and_single_converter_pass(self):
        result = self.run_verifier("--check-backdrop-converter")
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_shiboken_62_inline_native_event_passes(self):
        self.write_window(split_override=False)
        result = self.run_verifier()
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_event_filter_decoy_does_not_satisfy_native_event(self):
        self.write_window(limited_api_arguments=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("limited-API override is missing", result.stderr)

    def test_shiboken_62_event_filter_decoy_does_not_satisfy_native_event(self):
        self.write_window(
            limited_api_arguments=False,
            split_override=False,
        )
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("limited-API override is missing", result.stderr)

    def test_result_pointer_exposure_is_rejected(self):
        self.write_window(expose_result_pointer=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("exposed its result pointer", result.stderr)

    def test_shiboken_62_inline_result_pointer_exposure_is_rejected(self):
        self.write_window(
            expose_result_pointer=True,
            split_override=False,
        )
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("exposed its result pointer", result.stderr)

    def test_duplicate_converter_is_rejected(self):
        self.write_namespace(2)
        result = self.run_verifier("--check-backdrop-converter")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("found 2", result.stderr)

    def test_native_only_mode_does_not_require_namespace_wrapper(self):
        (self.generated_dir / WINDOWING_NAMESPACE_WRAPPER).unlink()
        result = self.run_verifier()
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_protected_access_macro_hack_is_rejected(self):
        (self.generated_dir / "fluent_dummy_wrapper.h").write_text(
            "#  define protected public\n",
            encoding="utf-8",
        )
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("protected-access macro hack", result.stderr)


if __name__ == "__main__":
    unittest.main()
