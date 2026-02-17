"""PlatformIO pre-build hook: runs 'npm run build' in public/ to regenerate webui_html.h."""
import subprocess, sys, os
Import("env")  # noqa: F821

public_dir = os.path.join(env.get("PROJECT_DIR", os.getcwd()), "public")
result = subprocess.run(["npm", "run", "build"], cwd=public_dir, env=os.environ)
if result.returncode != 0:
    sys.exit(1)
