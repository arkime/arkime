---
name: locale-sync
description: |
  Use this agent when the user needs to synchronize locale/translation JSON files.
  This includes ensuring all language files have the same keys at the same line numbers as en.json,
  adding missing keys with translations, or auditing locale files for consistency.
  Examples:
    - "Sync the locale files"
    - "Check if all language files match en.json"
    - "Add missing translation keys"
    - "Audit the locale files"
---

## Locale file structure
 - Locale files live in `common/vueapp/locales/`
 - `en.json` is the source of truth for keys and line numbers
 - Other locale files: de.json, es.json, et.json, fr.json, ja.json, ko.json, zh.json, x-pl.json
 - x-pl.json (Pig Latin) has one extra line in `__meta` for `"customFlag": "🐷"`, so all its keys are offset by +1 line compared to en.json
 - All other locale files must have identical keys at identical line numbers as en.json

## How to audit and fix

1. Compare each locale file against en.json line-by-line, matching keys at each line number
2. For x-pl.json, account for the +1 line offset after the `__meta` block
3. When a key is missing from a locale file:
   - Add a comma to the preceding line if needed
   - Insert the missing key at the correct line number
   - Provide an appropriate translation for that language (translate the en.json value)
   - For x-pl.json, convert to Pig Latin style
4. After fixing, verify all files have the correct line count and all keys match

## Quick check script

Run this from the locales directory to find mismatches:

```python
python3 -c "
with open('en.json') as f:
    en_lines = f.readlines()

def get_key(line):
    line = line.strip().rstrip(',')
    if '\"' in line and ':' in line:
        parts = line.split(':', 1)
        key = parts[0].strip()
        if key.startswith('\"'):
            return key
    return None

for lang in ['de', 'es', 'et', 'fr', 'ja', 'ko', 'zh']:
    with open(f'{lang}.json') as f:
        lang_lines = f.readlines()
    if len(en_lines) != len(lang_lines):
        print(f'{lang}.json: line count mismatch ({len(lang_lines)} vs {len(en_lines)})')
    else:
        for i in range(len(en_lines)):
            ek, lk = get_key(en_lines[i]), get_key(lang_lines[i])
            if ek and ek != lk:
                print(f'{lang}.json line {i+1}: expected {ek}, got {lk}')

# x-pl.json has +1 offset for customFlag in __meta
with open('x-pl.json') as f:
    xpl_lines = f.readlines()
for i in range(7, len(en_lines)):
    ek = get_key(en_lines[i])
    xk = get_key(xpl_lines[i+1]) if i+1 < len(xpl_lines) else None
    if ek and ek != xk:
        print(f'x-pl.json line {i+2}: expected {ek}, got {xk}')

print('Done')
"
```
