"""Verify safety-sensitive contracts in generated FluentQt wrappers."""

import argparse
from pathlib import Path
import re
import sys


WINDOW_WRAPPER = "fluent_windowing_window_wrapper.cpp"
WINDOWING_NAMESPACE_WRAPPER = "fluent_windowing_wrapper.cpp"
PROTECTED_HACK = re.compile(
    r"^\s*#\s*define\s+protected\s+public\b",
    re.MULTILINE,
)


def extract_function(source, signature):
    start = source.find(signature)
    if start < 0:
        raise RuntimeError("Generated function was not found: {0}".format(signature))

    opening_brace = source.find("{", start)
    if opening_brace < 0:
        raise RuntimeError("Generated function has no body: {0}".format(signature))

    depth = 0
    for index in range(opening_brace, len(source)):
        character = source[index]
        if character == "{":
            depth += 1
        elif character == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]

    raise RuntimeError("Generated function body is incomplete: {0}".format(signature))


def require_text(source, expected, context):
    if expected not in source:
        raise RuntimeError(
            "{0} is missing generated contract: {1}".format(context, expected)
        )


def verify_no_protected_hack(generated_dir):
    for generated_path in sorted(generated_dir.iterdir()):
        if generated_path.suffix not in {".cpp", ".h"}:
            continue
        source = generated_path.read_text(encoding="utf-8")
        if PROTECTED_HACK.search(source):
            raise RuntimeError(
                "Generated wrapper uses the protected-access macro hack: "
                "{0}".format(generated_path)
            )


def verify_contracts(generated_dir, check_backdrop_converter):
    verify_no_protected_hack(generated_dir)

    window_path = generated_dir / WINDOW_WRAPPER
    if not window_path.is_file():
        raise RuntimeError(
            "Generated Window wrapper was not found: {0}".format(window_path)
        )

    window_source = window_path.read_text(encoding="utf-8")
    native_event = extract_function(
        window_source,
        "bool WindowWrapper::nativeEvent(",
    )
    native_event_override_signature = "bool WindowWrapper::sbk_o_nativeEvent("
    if native_event_override_signature in window_source:
        native_event_override = extract_function(
            window_source,
            native_event_override_signature,
        )
        require_text(
            native_event_override,
            "PyObject *pyArgArray[2]",
            "Window::nativeEvent override",
        )
    else:
        # Shiboken 6.2 emits the Python override inline in nativeEvent().
        # Newer generators move it into sbk_o_nativeEvent().
        native_event_override = native_event

    require_text(
        native_event,
        "return this->::fluent::windowing::Window::nativeEvent",
        "Window::nativeEvent fallback",
    )
    require_text(
        native_event_override,
        'Py_BuildValue("(NN)"',
        "Window::nativeEvent limited-API override",
    )

    pointer_exposure = re.search(
        r"copyToPython\([^\n;]*\bresult\b",
        native_event_override,
    )
    if pointer_exposure:
        raise RuntimeError(
            "Window::nativeEvent exposed its result pointer to Python: {0}".format(
                pointer_exposure.group(0)
            )
        )

    verified_contracts = ["nativeEvent"]
    if check_backdrop_converter:
        namespace_path = generated_dir / WINDOWING_NAMESPACE_WRAPPER
        if not namespace_path.is_file():
            raise RuntimeError(
                "Generated windowing namespace wrapper was not found: {0}".format(
                    namespace_path
                )
            )
        namespace_source = namespace_path.read_text(encoding="utf-8")
        enum_converter = (
            "static void "
            "Enum_PythonToCpp_fluent_windowing_BackdropEffect"
        )
        enum_count = namespace_source.count(enum_converter)
        if enum_count != 1:
            raise RuntimeError(
                "Expected one generated BackdropEffect converter, found {0}".format(
                    enum_count
                )
            )
        verified_contracts.append("BackdropEffect")

    print(
        "Verified generated {0} contracts in {1}".format(
            " and ".join(verified_contracts),
            generated_dir,
        )
    )


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--generated-dir", type=Path, required=True)
    parser.add_argument("--check-backdrop-converter", action="store_true")
    return parser.parse_args()


if __name__ == "__main__":
    try:
        arguments = parse_args()
        verify_contracts(
            arguments.generated_dir.resolve(),
            arguments.check_backdrop_converter,
        )
    except Exception as error:
        sys.stderr.write("Generated binding contract check failed: {0}\n".format(error))
        sys.exit(1)
