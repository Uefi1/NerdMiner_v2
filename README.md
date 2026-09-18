# NerdMiner v2 — Pool Password Field Fix

## What was wrong

1. **WebUI field ID was broken**  
   In `wManager.cpp` the parameter was created with:
   ```cpp
   WiFiManagerParameter password_text_box("Poolpassword - Optional", ...);
   ```
   Spaces and `-` in the **ID** make an invalid HTML `id`/`name`. The field could fail to render or save correctly in the config portal.

2. **Buffer overflow**  
   `Settings.PoolPassword` is 80 bytes, but `mWorker.wPass` was only 20.  
   `strcpy(mWorker.wPass, Settings.PoolPassword)` overflowed into `.bss` if the password was longer than 19 characters (e.g. `d=0.00015` is fine, but longer options were not).

3. **Missing null-termination after `strncpy`**  
   If the value filled the buffer exactly, `strncpy` does not write `'\0'`.

## What this patch does

| File | Change |
|------|--------|
| `src/wManager.cpp` | Fix ID to `"Poolpassword"`, clearer label (mentions difficulty example), null-terminate after every `strncpy` of password/wallet |
| `src/stratum.h` | `wPass[20]` → `wPass[80]` |
| `src/mining.cpp` | `strcpy` → `snprintf` (always bounded + terminated) |

Default pool password remains **`x`** (from `DEFAULT_POOLPASS` in `storage.h`).

## How to apply

### Option A — patch against upstream BitMaker-hub/NerdMiner_v2

```bash
cd NerdMiner_v2   # your clone / fork
git apply nerdminer_v2_pool_password_fix.patch
# or:
patch -p1 < nerdminer_v2_pool_password_fix.patch
```

### Option B — replace files

Copy the three source files from this archive into your tree:

- `src/stratum.h`
- `src/mining.cpp`
- `src/wManager.cpp`

Then build with PlatformIO as usual.

## After flash

1. Connect to **NerdMinerAP** / password **MineYourCoins**
2. Open **192.168.4.1**
3. You should see the field **«Pool password (optional, e.g. d=0.0001 for difficulty)»**
4. Leave **`x`** for normal pools, or set e.g. **`d=0.0001`** / **`d=0.00015`** for pools that take difficulty in the password

## Notes

- Based on current `main` of https://github.com/BitMaker-hub/NerdMiner_v2
- Compatible with your fork https://github.com/Uefi1/NerdMiner_v2
- Fixes the same class of bugs discussed in upstream issues #152, #219, #564, #811
