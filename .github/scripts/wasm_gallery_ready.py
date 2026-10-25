"""Wait for the real Gallery startup transition before browser interaction."""


def wait_for_gallery_ready(page: object) -> None:
    # main() returning and a hidden accessibility proxy do not prove that the
    # native transition released input. The read-only smoke probe stops once ready.
    page.wait_for_function(
        """() => JSON.parse(document.documentElement.dataset.fluentQtInteractionState || '{}').ready === true""",
        timeout=120_000,
    )
