"""
Pre-build script: patch the ST25R3916 RFAL library for ESP32 compatibility.

The Xtensa SDK defines  #define BR 4  (a CPU special register) in specreg.h,
which collides with  uint8_t BR;  in rfal_nfcDep.h.  We insert an #undef BR
at the top of that header so the struct field compiles cleanly.
"""
Import("env")
import os


def patch_rfal(env):
    libdeps = env.subst("$PROJECT_LIBDEPS_DIR")
    env_name = env.subst("$PIOENV")
    search_root = os.path.join(libdeps, env_name)

    if not os.path.isdir(search_root):
        return

    for root, _dirs, files in os.walk(search_root):
        if "rfal_nfcDep.h" in files:
            fpath = os.path.join(root, "rfal_nfcDep.h")
            with open(fpath, "r") as f:
                content = f.read()

            marker = "/* ESP32_BR_PATCHED */"
            if marker in content:
                return  # already patched

            # Insert #undef BR right after the first #define block / include guard
            patch = (
                "\n"
                f"{marker}\n"
                "#ifdef BR\n"
                "#undef BR\n"
                "#endif\n"
            )

            # Insert after the last #include in the file header
            last_include = content.rfind("#include")
            if last_include != -1:
                end_of_line = content.index("\n", last_include)
                content = content[: end_of_line + 1] + patch + content[end_of_line + 1 :]
            else:
                # Fallback: insert near the top
                content = patch + content

            with open(fpath, "w") as f:
                f.write(content)

            print(f"  [patch_rfal] Patched BR macro conflict in {fpath}")


patch_rfal(env)
