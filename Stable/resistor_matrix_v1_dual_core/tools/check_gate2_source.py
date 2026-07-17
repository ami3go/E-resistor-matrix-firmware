#!/usr/bin/env python3
from __future__ import annotations
import json
import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
files = [p for p in root.glob('*') if p.suffix in {'.cpp','.h','.ino'}]
text = '\n'.join(p.read_text(errors='replace') for p in files)
checks = []

def add(name: str, passed: bool, detail: str = '') -> None:
    checks.append({'name': name, 'passed': bool(passed), 'detail': detail})

add('firmware version 0.5.0', 'FIRMWARE_VERSION = "0.5.0"' in text)
add('numeric runtime table', 'float channelResistorOhms[CHANNEL_COUNT][BIT_COUNT]' in text)
add('legacy runtime struct removed', 'RuntimeResistorInfo' not in text)
add('legacy runtime table removed', 'channelResistorTable' not in text)
add('single shared default table', 'DEFAULT_RESISTOR_OHMS[BIT_COUNT]' in text and 'CHANNEL_DEFAULT_TABLES' not in text)
add('direct bit indexing', 'getRuntimeResistanceOhms' in text and 'for (uint8_t i = 0; i < BIT_COUNT; i++)' not in (root/'runtime_resistor_config.cpp').read_text())
add('target search has no logf', 'logf(' not in (root/'resistance_calculation.cpp').read_text())
add('target search stats', all(token in text for token in ['candidatesVisited','elapsedUs','timedOut','cancelled']))
add('target dry-run SCPI', 'TARGET:CALC?' in (root/'scpi_server.cpp').read_text())
add('serial diagnostic SCPI', 'SYST:DIAG:SERIAL?' in (root/'scpi_server.cpp').read_text())
add('strict resistance suffix parser', "if (*endPtr != '\\0') return false;" in (root/'resistance_calculation.cpp').read_text())
add('Core1 Serial still absent', not any('Serial.' in p.read_text(errors='replace') for p in [root/'shift_registers.cpp', root/'core_command.cpp', root/'core1_event_queue.cpp']))
add('estimated model RAM saving >= 2 KiB', (25*8*16 + 4*8*16) - (4*8*16 + 4*8*16) >= 2048,
    f'estimated_saved_bytes={(25*8*16 + 4*8*16) - (4*8*16 + 4*8*16)}')

oracle_path = root/'validation'/'gate2_oracle_results.json'
try:
    oracle = json.loads(oracle_path.read_text())
    add('oracle vectors pass', bool(oracle.get('pass')), f"masks={oracle.get('mask_vector_count')} targets={oracle.get('target_case_count')}")
except Exception as exc:
    add('oracle vectors pass', False, str(exc))

result = {'gate':'G2','checks':checks,'passed':sum(c['passed'] for c in checks),'failed':sum(not c['passed'] for c in checks)}
out = root/'validation'/'gate2_source_check.json'
out.write_text(json.dumps(result, indent=2, sort_keys=True))
for c in checks:
    print(('PASS' if c['passed'] else 'FAIL'), '-', c['name'], c['detail'])
print(f"Summary: {result['passed']} passed, {result['failed']} failed")
sys.exit(0 if result['failed']==0 else 1)
