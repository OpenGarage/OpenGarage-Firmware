"""Pinned core 3.1.2 shared-IRAM feasibility gate; runtime guards still required."""
import re
import subprocess
from pathlib import Path

IRAM_BASE = 0x40100000
IRAM_END = IRAM_BASE + 48 * 1024

def capacity(text_end):
    if not IRAM_BASE <= text_end < IRAM_END:
        raise ValueError('Unexpected ESP8266 IRAM layout')
    # mmu_sec_heap(): (_text_end + 32) rounded down to 8 bytes.
    # UMM reserves two 8-byte blocks for its heap boundary bookkeeping.
    heap_start = (text_end + 32) & ~7
    return max(0, IRAM_END - heap_start - 16)

def require_budget(text_end, rx):
    available = capacity(text_end)
    required = rx + 512  # Same feasibility threshold as the runtime WSS guard.
    if available < required:
        raise ValueError(f'WSS IRAM budget exceeded: at most {available} B free, need {required} B')
    return available, required

def register(env):
    defines = env.ParseFlags(env.get('BUILD_FLAGS', [])).get('CPPDEFINES', [])
    values = {d[0]: d[1] for d in defines if isinstance(d, (tuple, list))}
    names = {d if isinstance(d, str) else d[0] for d in defines}
    if 'PIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM48_SECHEAP_SHARED' not in names:
        return
    rx = int(values.get('OG_WSS_RX', 16384))
    def check(source, target, env):
        nm = Path(env.PioPlatform().get_package_dir('toolchain-xtensa')) / 'bin/xtensa-lx106-elf-nm'
        listing = subprocess.check_output([str(nm), '-n', str(target[0])], text=True)
        match = re.search(r'^([0-9a-fA-F]+)\s+\w\s+_text_end$', listing, re.M)
        if not match:
            raise ValueError('Cannot check WSS IRAM budget: missing _text_end')
        available, required = require_budget(int(match[1], 16), rx)
        print(f'WSS IRAM gate: capacity {available} B; required {required} B; margin {available-required} B')
    elf = '$BUILD_DIR/${PROGNAME}.elf'
    env.Depends(elf, str(Path(env.subst("$PROJECT_DIR")) / "wss_iram_budget.py"))
    env.AddPostAction(elf, check)

if 'Import' in globals():
    Import('env')
    register(env)
