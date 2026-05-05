"""Pre-build script: generates web_page.h from portal.html."""

import os

Import("env")  # noqa: F821 (injected by PlatformIO)

HTML_SOURCE = os.path.join("lib", "core", "network", "portal.html")
HEADER_OUTPUT = os.path.join("lib", "core", "network", "web_page.h")

with open(HTML_SOURCE, "r") as f:
    html = f.read()

new_content = (
    "#pragma once\n\n"
    "namespace web_page {\n\n"
    'inline const char HTML[] PROGMEM = R"rawhtml(\n'
    + html
    + ')rawhtml";\n\n'
    "}\n"
)

# Only write if content changed — avoids invalidating PlatformIO build cache.
if os.path.exists(HEADER_OUTPUT):
    with open(HEADER_OUTPUT, "r") as f:
        if f.read() == new_content:
            print(f"{HEADER_OUTPUT} is up to date")
        else:
            with open(HEADER_OUTPUT, "w") as f:
                f.write(new_content)
            print(f"Generated {HEADER_OUTPUT} from {HTML_SOURCE}")
else:
    with open(HEADER_OUTPUT, "w") as f:
        f.write(new_content)
    print(f"Generated {HEADER_OUTPUT} from {HTML_SOURCE}")
