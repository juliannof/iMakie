Import("env")
import datetime

now = datetime.datetime.now()
fw_ver   = "0.0.1"
ver_comp = fw_ver.replace(".", "")
build_id = now.strftime("%Y%m%d.%H%M") + f".{ver_comp}"

env.Append(CCFLAGS=[
    f'-DFW_VERSION=\\"{fw_ver}\\"',
    f'-DFW_BUILD_ID=\\"{build_id}\\"',
])
print(f"[version] {build_id}")
