Import("env")
import subprocess, sys, os

def ota_upload(source, target, env):
    ip = "192.168.1.15"

    password    = "9821"
    firmware    = os.path.abspath(str(source[0]))
    espota      = "/Users/julianno/.platformio/packages/framework-arduinoespressif32/tools/espota.py"
    project_dir = env.subst("$PROJECT_DIR")

    print(f"[OTA] firmware    : {firmware}")
    print(f"[OTA] project_dir : {project_dir}")
    print(f"[OTA] Subiendo a  : {ip}...")

    cmd = [sys.executable, espota,
           "-i", ip,
           "-p", "3232",
           f"--auth={password}",
           "-f", firmware,
           "-d",
           "-t", "60"]

    result = subprocess.run(cmd, timeout=120, cwd=project_dir)
    if result.returncode != 0:
        raise Exception("OTA upload failed")

env.Replace(UPLOADCMD=ota_upload)