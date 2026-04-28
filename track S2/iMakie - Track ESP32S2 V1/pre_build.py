Import("env")
import datetime
import re

now = datetime.datetime.now()
fw_ver   = "0.0.1"
ver_comp = fw_ver.replace(".", "")
build_id = now.strftime("%Y%m%d.%H%M") + f".{ver_comp}"

# Leer config.h para extraer HW_STATUS
hw_status_str = ""
config_path = "src/config.h"
hw_components = [
    "Motor", "RS485", "Display", "ADC", "Fader",
    "TouchFader", "NeoPixels", "Touch", "Encoder", "Buttons"
]

try:
    with open(config_path, "r") as f:
        content = f.read()
        for comp in hw_components:
            match = re.search(rf"//\s*HW_STATUS:\s*{comp}=(\d)", content)
            if match:
                hw_status_str += match.group(1)
            else:
                hw_status_str += "0"  # default: no implementado
except:
    hw_status_str = "0" * len(hw_components)

env.Append(CCFLAGS=[
    f'-DFW_VERSION=\\"{fw_ver}\\"',
    f'-DFW_BUILD_ID=\\"{build_id}\\"',
    f'-DHW_STATUS=\\"{hw_status_str}\\"',
])
print(f"[version] {build_id}")
print(f"[hw_status] {hw_status_str}")
