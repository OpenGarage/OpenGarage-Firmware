from pathlib import Path
import importlib.util
p=Path(__file__).resolve().parents[1]/'OpenGarage/wss_iram_budget.py'
s=importlib.util.spec_from_file_location('budget',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
current=0x40107cdd
assert m.require_budget(current,16384)==(17144,16896)
assert m.require_budget(current+248,16384)==(16896,16896)
try:m.require_budget(current+256,16384)
except ValueError:pass
else:raise AssertionError('Oversized IRAM build accepted')
try:m.require_budget(0x40200000,16384)
except ValueError:pass
else:raise AssertionError('Unexpected layout accepted')
print('WSS IRAM gate: current, boundary, overflow and invalid layout: PASS')
