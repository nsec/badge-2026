"""Pre-build script: generates web_page.h from portal.html."""

import os

Import("env")  # noqa: F821 (injected by PlatformIO)

HTML_SOURCE = os.path.join("lib", "core", "network", "portal.html")
HEADER_OUTPUT = os.path.join("lib", "core", "network", "web_page.h")

with open(HTML_SOURCE, "r") as f:
    html = f.read()

with open(HEADER_OUTPUT, "w") as f:
    f.write("#pragma once\n\n")
    f.write("namespace web_page {\n\n")
    f.write('inline const char HTML[] PROGMEM = R"rawhtml(\n')
    f.write(html)
    f.write(')rawhtml";\n\n')
    f.write("}  // namespace web_page\n")

print(f"Generated {HEADER_OUTPUT} from {HTML_SOURCE}")
