Import("env")
import subprocess, sys

def ota_upload(source, target, env):
    ip       = env.GetProjectOption("upload_port")
    password = "9821"
    firmware = str(source[0])
    
    cmd = [
        sys.executable,
        env.subst("$PROJECT_PACKAGES_DIR/tool-espotapy/espota.py"),
        "-i", ip,
        "-p", "3232",
        "--auth", password,
        "-f", firmware
    ]
    print(f"[OTA] Subiendo a {ip}...")
    result = subprocess.run(cmd)
    if result.returncode != 0:
        raise Exception("OTA upload failed")

env.Replace(UPLOADCMD=ota_upload)