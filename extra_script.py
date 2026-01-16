from SCons.Script import Import
Import("env")
import os

print(">>> extra_script.py loaded for PIOENV:", env.get("PIOENV"))

def before_test(target, source, env):
    exe_path = target[0].get_abspath()
    build_dir = os.path.dirname(exe_path)
    profile_path = os.path.join(build_dir, "coverage_%p.profraw")

    print("=== EXTRA_SCRIPT before_test() LOADED ===")
    print("Executable:", exe_path)
    print("LLVM_PROFILE_FILE set to:", profile_path)

    # IMPORTANT: set in the *real* OS environment so the test runner sees it
    os.environ["LLVM_PROFILE_FILE"] = profile_path

# Attach to actual test executable link target
env.AddPostAction("$BUILD_DIR/program.exe", before_test)
