/**
 * @file resistance_calculation.cpp
 * @brief Resistance parsing, equivalent-resistance calculation, mask safety checks,
 *        profile handling, and UI helper rendering.
 *
 * RP2040 OPTIMISATIONS OVER ORIGINAL
 * ====================================
 *
 *  1. Conductance cache  (s_cond[][])
 *     parseResistanceOhms() is slow: it constructs a String, calls trim/toUpperCase,
 *     and runs strtod on every call.  rebuildConductanceCache() runs it once per
 *     resistor slot at startup (or on config change) and stores the result as a float.
 *     Every hot-path call then reads a single array element instead of parsing a string.
 *
 *  2. float instead of double in the inner loop
 *     The RP2040 Cortex-M0+ has NO hardware FPU.  All floating-point is emulated via
 *     ROM helpers; the 32-bit (float) helpers are ~5× faster than the 64-bit (double)
 *     ones and produce half the memory traffic.  Resistor tolerances need ≤ 7 significant
 *     figures – well within float precision.  Final public API results are widened to
 *     double for compatibility.
 *
 *  3. __builtin_ctz / __builtin_popcount
 *     GCC lowers these to the optimal M0+ instruction sequence (using the Bootrom
 *     CLZ-based helpers).  __builtin_ctz replaces a 16-iteration bit-scan loop;
 *     __builtin_popcount replaces the shift-and-count loop in countActiveBits16().
 *
 *  4. Gosper's-hack enumeration in findNearestMaskForTarget()
 *     The original visited all 65,535 non-zero 16-bit masks.  For each it called
 *     checkMaskSafety() → calculateEquivalentOhms() → parseResistanceOhms() per active
 *     bit – over 1 million String operations in the worst case.
 *
 *     Gosper's hack generates only masks that have exactly k bits set, for k = 1…maxK.
 *     With safetyMaxActiveBits = 4 this is C(16,1)+…+C(16,4) = 2,516 iterations.
 *     Even with safetyExpertMode (all bits allowed), the conductance-cache inner loop
 *     replaces every string parse with a single float array read.
 *
 * INTEGRATION NOTE
 * -----------------
 *  This merged firmware declares rebuildConductanceCache() and
 *  invalidateConductanceCache() in app.h.  The cache is rebuilt after startup
 *  configuration loading and invalidated after runtime resistor-table edits.
 *  It is also rebuilt lazily on first use if required.
 */

#include "app.h"
#include <math.h>

// ============================================================
// CONDUCTANCE CACHE  (NEW)
// ============================================================

// Pre-computed conductance (1/R) in Siemens for each (channel, bit) pair.
// A negative sentinel (-1.0f) marks an invalid or unpopulated resistor slot.
// Memory cost: CHANNEL_COUNT(8) × BIT_COUNT(16) × 4 bytes = 512 bytes.
static float s_cond[CHANNEL_COUNT][BIT_COUNT];
static bool  s_condValid = false;

/**
 * @brief Build (or rebuild) the conductance cache from nominal_resistance strings.
 *
 * Call once after setup() has populated RuntimeResistorInfo data, and again
 * whenever that data changes at runtime.  Declared extern in app.h.
 */
void rebuildConductanceCache() {
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
        for (uint8_t bit = 0; bit < BIT_COUNT; bit++) {
            RuntimeResistorInfo* info = getRuntimeResistorInfoForBit(ch, bit);
            double rOhm = 0.0;
            if (info != nullptr && parseResistanceOhms(info->nominal_resistance, rOhm)) {
                s_cond[ch][bit] = float(1.0 / rOhm);
            } else {
                s_cond[ch][bit] = -1.0f;   // sentinel: invalid / unpopulated
            }
        }
    }
    s_condValid = true;
}

/**
 * @brief Invalidate the conductance cache after any runtime resistor-table edit.
 *
 * The next call to calculateEquivalentOhms(), checkMaskSafety(),
 * resistanceCssClass(), or findNearestMaskForTarget() will rebuild the cache
 * lazily.  This is safer than relying on every caller to rebuild immediately.
 */
void invalidateConductanceCache() {
    s_condValid = false;
}

// ============================================================
// RESISTANCE PARSING & FORMATTING  (UNCHANGED)
// ============================================================

/**
 * @brief Parse Resistance Ohms.
 * @param text  Input string (e.g. "4.7K", "100R", "1M").
 * @param ohms  Output resistance in ohms.
 * @return true on success.
 */
bool parseResistanceOhms(const char* text, double& ohms) {
    if (text == nullptr) return false;

    String s(text);
    s.trim();
    s.toUpperCase();
    if (s.length() == 0)                     return false;
    if (s == "TODO" || s == "NC" || s == "N/A") return false;

    char*  endPtr = nullptr;
    double value  = strtod(s.c_str(), &endPtr);
    if (endPtr == s.c_str()) return false;

    while (*endPtr == ' ') endPtr++;

    double multiplier = 1.0;
    if      (*endPtr == 'R' || *endPtr == '\0') multiplier = 1.0;
    else if (*endPtr == 'K')                    multiplier = 1000.0;
    else if (*endPtr == 'M')                    multiplier = 1000000.0;
    else                                        return false;

    ohms = value * multiplier;
    return (ohms > 0.0);
}

/**
 * @brief Format Resistance Ohms.
 * @param ohms  Value in ohms.
 * @return Human-readable string such as "4.70 kOhm".
 */
String formatResistanceOhms(double ohms) {
    // Keep calculated output resistance display at 3 digits after the decimal
    // point in every unit range. This gives more useful feedback in the
    // browser control table without changing internal calculation precision.
    if (ohms >= 1000000.0) return String(ohms / 1000000.0, 3) + " MOhm";
    if (ohms >=    1000.0) return String(ohms /    1000.0, 3) + " kOhm";
    return String(ohms, 3) + " Ohm";
}

// ============================================================
// FAST RESISTANCE CALCULATION CORE  (NEW – private)
// ============================================================

/**
 * @brief Sum conductances from the cache and return the parallel equivalent resistance.
 *
 * Iterates only the bits that are set in `mask` using __builtin_ctz (O(popcount)
 * instead of O(BIT_COUNT)) and reads 1/R from the pre-built float cache.
 * Uses float arithmetic throughout; result is widened to double for the caller.
 *
 * @param channelIndex  Zero-based channel index.
 * @param mask          16-bit switch mask.
 * @param outOhms       Equivalent resistance (INFINITY when mask == 0).
 * @return false if channelIndex is out of range or any active bit has no valid resistor.
 */
static bool calcEquivFast(uint8_t channelIndex, uint16_t mask, double& outOhms) {
    if (channelIndex >= CHANNEL_COUNT) return false;
    if (mask == 0x0000) { outOhms = INFINITY; return true; }
    if (!s_condValid) rebuildConductanceCache();   // lazy init fallback

    float    gSum = 0.0f;
    uint32_t m    = mask;

    while (m) {
        uint8_t bit = uint8_t(__builtin_ctz(m));   // lowest set bit; compiler-optimised index; GCC selects the best RP2040 sequence
        float   g   = s_cond[channelIndex][bit];
        if (g < 0.0f) return false;                 // invalid / unpopulated resistor
        gSum += g;
        m &= m - 1u;                                // clear lowest set bit (Brian Kernighan's trick)
    }

    outOhms = double(1.0f / gSum);
    return true;
}

// ============================================================
// PUBLIC CALCULATION API  (OPTIMISED – now delegates to calcEquivFast)
// ============================================================

/**
 * @brief Calculate Output Resistance Text.
 * @param channelIndex  Zero-based channel index.
 * @param mask          16-bit resistance switch mask.
 * @return Human-readable resistance string, "OPEN", or "CONFIG ERROR".
 */
String calculateOutputResistanceText(uint8_t channelIndex, uint16_t mask) {
    if (channelIndex >= CHANNEL_COUNT) return "CONFIG ERROR";
    if (mask == 0x0000)               return "OPEN";

    double ohms = 0.0;
    if (!calcEquivFast(channelIndex, mask, ohms)) return "CONFIG ERROR";
    return formatResistanceOhms(ohms);
}

/**
 * @brief Calculate the equivalent resistance of all active branches in one channel mask.
 * @param channelIndex  Zero-based channel index.
 * @param mask          16-bit resistance switch mask.
 * @param outOhms       Output: equivalent resistance in ohms (INFINITY if mask == 0).
 * @return true on success.
 */
bool calculateEquivalentOhms(uint8_t channelIndex, uint16_t mask, double& outOhms) {
    return calcEquivFast(channelIndex, mask, outOhms);
}

/**
 * @brief Count active bits in a 16-bit mask.
 *        Uses __builtin_popcount so GCC can select an efficient RP2040 implementation.
 *        Replaces the original byte-by-byte shift-and-count loop.
 * @param mask  16-bit resistance switch mask.
 * @return Number of set bits (0–16).
 */
uint8_t countActiveBits16(uint16_t mask) {
    return uint8_t(__builtin_popcount(mask));
}

/**
 * @brief Validate a mask against the configured minimum resistance and maximum
 *        active-bit limits.  Now uses the fast conductance-cache path.
 * @param channelIndex  Zero-based channel index.
 * @param mask          16-bit resistance switch mask.
 * @param reason        Output buffer for a human-readable diagnostic message.
 * @param reasonLen     Size of reason buffer in bytes.
 * @return true if the mask passes all safety checks.
 */
bool checkMaskSafety(uint8_t channelIndex, uint16_t mask, char* reason, size_t reasonLen) {
    if (mask == 0x0000) return true;

    // __builtin_popcount lets GCC choose an efficient RP2040 implementation
    uint8_t activeBits = uint8_t(__builtin_popcount(mask));

    if (!safetyExpertMode && activeBits > safetyMaxActiveBits) {
        snprintf(reason, reasonLen,
                 "Rejected: %u active bits exceeds safety limit %u",
                 unsigned(activeBits), unsigned(safetyMaxActiveBits));
        return false;
    }

    double ohms = 0.0;
    if (!calcEquivFast(channelIndex, mask, ohms)) {
        snprintf(reason, reasonLen, "Rejected: resistance calculation failed");
        return false;
    }

    if (!safetyExpertMode && isfinite(ohms) && ohms < safetyMinOhm) {
        snprintf(reason, reasonLen,
                 "Rejected: calculated resistance %.3f Ohm below safety limit %.3f Ohm",
                 ohms, safetyMinOhm);
        return false;
    }

    if (!safetyExpertMode && safetyMaxOhm > 0.0 && isfinite(ohms) && ohms > (safetyMaxOhm * 1.000001)) {
        snprintf(reason, reasonLen,
                 "Rejected: calculated resistance %.3f Ohm above safety limit %.3f Ohm",
                 ohms, safetyMaxOhm);
        return false;
    }

    return true;
}

/**
 * @brief Return a CSS class name that reflects the safety level of the resistance.
 *        Now uses the fast conductance-cache path.
 * @param channelIndex  Zero-based channel index.
 * @param mask          16-bit resistance switch mask.
 * @return CSS class string.
 */
String resistanceCssClass(uint8_t channelIndex, uint16_t mask) {
    if (mask == 0x0000) return "res-open";

    double ohms = 0.0;
    if (!calcEquivFast(channelIndex, mask, ohms) || !isfinite(ohms)) return "res-error";

    if (ohms <     500.0) return "res-danger";
    if (ohms <   10000.0) return "res-warn";
    if (ohms <= 1000000.0) return "res-ok";
    return "res-high";
}

// ============================================================
// PATH HELPERS  (UNCHANGED)
// ============================================================

/** @brief Return the LittleFS path to the safety configuration CSV. */
String safetyConfigPath() { return "/safety.csv"; }

/**
 * @brief Return the LittleFS path to the calibration metadata CSV for a channel.
 * @param channelIndex  Zero-based channel index.
 */
String calibrationMetaPath(uint8_t channelIndex) {
    return String("/meta_ch") + String(channelIndex + 1) + ".csv";
}

// ============================================================
// EVENT LOG  (UNCHANGED)
// ============================================================

/**
 * @brief Append a timestamped entry to the ring-buffer event log (max 32 entries).
 * @param text  Message string.
 */
void appendLogEvent(const char* text) {
    if (text == nullptr) return;
    uint8_t index = eventLogHead;
    snprintf(eventLog[index], sizeof(eventLog[index]),
             "%lu ms,%s", (unsigned long)millis(), text);
    eventLogHead = (eventLogHead + 1U) % 32U;
    if (eventLogCount < 32U) eventLogCount++;
}

// ============================================================
// NAME / PATH SANITISATION  (UNCHANGED)
// ============================================================

/**
 * @brief Sanitize a profile name for use as a filesystem file name.
 *        Keeps alphanumeric characters, underscores, and hyphens; converts
 *        spaces and dots to underscores; truncates at 24 characters.
 * @param name  Input name.
 * @return Safe file-name string.
 */
String sanitizeName(String name) {
    name.trim();
    String out;
    out.reserve(24);
    for (uint16_t i = 0; i < name.length() && out.length() < 24; i++) {
        char c = name[i];
        if      (isalnum(c) || c == '_' || c == '-') out += c;
        else if (c == ' '   || c == '.')              out += '_';
    }
    if (out.length() == 0) out = "profile";
    return out;
}

/**
 * @brief Construct the LittleFS path for a named profile CSV.
 * @param name  Profile name (will be sanitised).
 */
String profilePathFromName(String name) {
    return String("/profile_") + sanitizeName(name) + ".csv";
}

// ============================================================
// SAFETY CONFIG – LittleFS  (UNCHANGED)
// ============================================================

/**
 * @brief Persist safety configuration to LittleFS (/safety.csv).
 * @return true on success.
 */
bool saveSafetyConfigToLittleFS() {
    if (!littleFsReady) return false;
    File f = LittleFS.open(safetyConfigPath(), "w");
    if (!f) return false;
    f.print("min_ohm,");        f.println(String(safetyMinOhm, 6));
    f.print("max_ohm,");        f.println(String(safetyMaxOhm, 6));
    f.print("max_active_bits,"); f.println(String(safetyMaxActiveBits));
    f.print("expert_mode,");    f.println(safetyExpertMode ? "1" : "0");
    f.close();
    return true;
}

/**
 * @brief Load safety configuration from LittleFS (/safety.csv).
 * @return true on success.
 */
bool loadSafetyConfigFromLittleFS() {
    if (!littleFsReady || !LittleFS.exists(safetyConfigPath())) return false;

    File f = LittleFS.open(safetyConfigPath(), "r");
    if (!f) return false;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        int comma = line.indexOf(',');
        if (comma < 0) continue;

        String key   = line.substring(0, comma);
        String value = line.substring(comma + 1);
        key.trim(); value.trim(); key.toLowerCase();

        if      (key == "min_ohm")          safetyMinOhm = value.toFloat();
        else if (key == "max_ohm")          {
            double v = value.toFloat();
            if (v > 0.0) safetyMaxOhm = v;
        }
        else if (key == "max_active_bits")  {
            int v = value.toInt();
            if (v >= 1 && v <= 16) safetyMaxActiveBits = uint8_t(v);
        }
        else if (key == "expert_mode")
            safetyExpertMode = (value == "1" || value == "true" || value == "on");
    }
    f.close();
    return true;
}

// ============================================================
// NEAREST-MASK SEARCH  (OPTIMISED – Gosper's hack + conductance cache)
// ============================================================

/**
 * @brief Search for the switch mask that produces the closest equivalent
 *        resistance to targetOhm, respecting current safety limits.
 *
 * ORIGINAL (slow)
 * ---------------
 *  Iterated all 65,535 non-zero masks.  For every mask it called
 *  checkMaskSafety() → calculateEquivalentOhms() → parseResistanceOhms()
 *  per active bit – over 1 million String operations in the worst case.
 *  On the RP2040 M0+ this could take many seconds.
 *
 * OPTIMISED
 * ---------
 *  a) Gosper's hack  – enumerate only masks with exactly k set bits,
 *     for k = 1 … maxK.  safetyMaxActiveBits = 4 → 2,516 masks visited
 *     instead of 65,535 (~26× fewer iterations before even touching math).
 *
 *  b) Conductance cache  – the inner loop reads 1/R from s_cond[][] (one
 *     float load) instead of calling parseResistanceOhms() (String + strtod).
 *
 *  c) float arithmetic and logf()  – faster than double on M0+.
 *
 *  d) Inlined safety check  – the bit-count limit is implicit in the k-loop
 *     (no masks with too many bits are ever generated); only the min-ohm
 *     limit needs a runtime comparison (gSum <= maxGSafe).
 *
 *  e) Early exit  – stops immediately if an exact match is found (score == 0).
 *
 *  Scoring: |log(R_candidate / R_target)| = |log(G_target / G_candidate)|
 *           = |logf(gTarget) – logf(gSum)|, identical to the original metric.
 *
 * @param channelIndex     Zero-based channel index.
 * @param targetOhm        Desired resistance in ohms.
 * @param bestMask         Output: 16-bit mask of the best match.
 * @param bestOhm          Output: equivalent resistance of the best match.
 * @param bestErrorPercent Output: signed % error vs targetOhm.
 * @return true if at least one safe, valid mask was found.
 */
bool findNearestMaskForTarget(uint8_t channelIndex, double targetOhm,
                               uint16_t& bestMask, double& bestOhm,
                               double& bestErrorPercent) {
    if (channelIndex >= CHANNEL_COUNT || targetOhm <= 0.0) return false;
    if (!s_condValid) rebuildConductanceCache();

    // Constants computed once for the entire search
    const float gTarget  = float(1.0 / targetOhm);
    const float logGTgt  = logf(gTarget);

    // Safety conductance window:
    //   R_eq >= safetyMinOhm  ↔  gSum <= 1/safetyMinOhm
    //   R_eq <= safetyMaxOhm  ↔  gSum >= 1/safetyMaxOhm
    const float maxGSafe = (safetyExpertMode || safetyMinOhm <= 0.0)
                               ? 1.0e30f
                               : float(1.0 / safetyMinOhm);
    const float minGSafe = (safetyExpertMode || safetyMaxOhm <= 0.0)
                               ? 0.0f
                               : float(1.0 / (safetyMaxOhm * 1.000001));

    // Outer loop upper bound: respect safetyMaxActiveBits (unless expert mode).
    // Clamp defensively so corrupted settings cannot generate invalid bit patterns.
    uint8_t maxK = safetyExpertMode ? uint8_t(BIT_COUNT) : safetyMaxActiveBits;
    if (maxK > BIT_COUNT) maxK = BIT_COUNT;
    if (maxK == 0) return false;

    float bestScore = 1.0e30f;
    bool  found     = false;

    for (uint8_t k = 1; k <= maxK; k++) {

        // Gosper's hack: start with the k lowest bits set (smallest k-bit number)
        uint32_t mask = (1u << k) - 1u;

        while (mask <= 0xFFFFu) {

            // ── Compute conductance sum for this mask ─────────────────────────
            float    gSum  = 0.0f;
            bool     valid = true;
            uint32_t m     = mask;

            while (m) {
                uint8_t bit = uint8_t(__builtin_ctz(m));   // lowest set bit; compiler-optimised
                float   g   = s_cond[channelIndex][bit];
                if (g < 0.0f) { valid = false; break; }    // unpopulated slot
                gSum += g;
                m &= m - 1u;                                // clear lowest bit
            }

            // ── Safety check + log-scale scoring ─────────────────────────────
            if (valid && gSum > 0.0f && gSum >= minGSafe && gSum <= maxGSafe) {
                float score = fabsf(logf(gSum) - logGTgt);
                if (score < bestScore) {
                    bestScore = score;
                    bestMask  = uint16_t(mask);
                    bestOhm   = double(1.0f / gSum);
                    found     = true;
                    if (bestScore == 0.0f) goto search_done;   // exact – can't do better
                }
            }

            // ── Gosper's hack: advance to the next mask with exactly k bits set ─
            {
                uint32_t c  = mask & (0u - mask);   // isolate lowest set bit
                uint32_t r  = mask + c;
                mask = (((r ^ mask) >> 2u) / c) | r;
                // When all k bits have shifted past bit 15, mask > 0xFFFF → loop ends.
            }
        }
    }

search_done:
    if (!found) return false;
    bestErrorPercent = (bestOhm - targetOhm) / targetOhm * 100.0;
    return true;
}

// ============================================================
// PROFILE MANAGEMENT  (UNCHANGED)
// ============================================================

/**
 * @brief Build an HTML <option> list of saved profiles from LittleFS.
 * @return HTML string with one <option> per profile file.
 */
String profileListOptions() {
    String html;
    if (!littleFsReady) {
        html += "<option value=''>LittleFS not ready</option>";
        return html;
    }

    Dir      dir   = LittleFS.openDir("/");
    uint16_t count = 0;

    while (dir.next()) {
        String name = dir.fileName();
        if (!name.startsWith("/")) name = "/" + name;

        if (name.startsWith("/profile_") && name.endsWith(".csv")) {
            String label = name.substring(9, name.length() - 4);
            html += "<option value='"; html += label; html += "'>";
            html += label; html += "</option>";
            count++;
        }
    }

    if (count == 0) html += "<option value=''>No profiles saved</option>";
    return html;
}

/**
 * @brief Load channel masks from a named profile CSV.
 * @param profileName  Profile name.
 * @param masks        Output array of CHANNEL_COUNT masks (initialised from current state).
 * @return true on success.
 */
bool loadProfileMasks(String profileName, uint16_t masks[CHANNEL_COUNT]) {
    if (!littleFsReady) return false;
    String path = profilePathFromName(profileName);
    if (!LittleFS.exists(path)) return false;

    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) masks[ch] = channelMask[ch];

    File f = LittleFS.open(path, "r");
    if (!f) return false;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        int comma = line.indexOf(',');
        if (comma < 0) continue;

        String left  = line.substring(0, comma);
        String right = line.substring(comma + 1);
        left.trim(); right.trim(); left.toUpperCase();
        if (!left.startsWith("CH")) continue;

        int      ch   = left.substring(2).toInt();
        uint16_t mask = 0;
        if (ch >= 1 && ch <= 8 && parseHex16String(right, mask)) {
            masks[ch - 1] = mask;
        }
    }
    f.close();
    return true;
}

/**
 * @brief Save the current channel mask state as a named profile CSV.
 * @param profileName  Profile name.
 * @return true on success.
 */
bool saveCurrentProfile(String profileName) {
    if (!littleFsReady) return false;
    String path = profilePathFromName(profileName);
    File   f    = LittleFS.open(path, "w");
    if (!f) return false;

    f.print("name,"); f.println(sanitizeName(profileName));
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
        f.print("CH"); f.print(ch + 1); f.print(","); f.println(hex16(channelMask[ch]));
    }
    f.close();
    return true;
}

// ============================================================
// HTML HELPERS  (UNCHANGED)
// ============================================================

/**
 * @brief Append the profile-manager card (save / load / delete forms) to an HTML string.
 * @param html  HTML string that receives generated markup.
 */
void appendProfileManager(String& html) {
    html += "<div class='card'>";
    html += "<h2>Profiles / Presets</h2>";
    html += "<p class='small'>Save, load, and delete full 8-channel mask states. Profiles are stored as LittleFS CSV files.</p>";

    html += "<form method='POST' action='/profile_save'>";
    html += "<input name='name' placeholder='Profile name' value='default_profile'>";
    html += "<button type='submit'>Save current state</button>";
    html += "</form>";

    html += "<form method='GET' action='/profile_apply'>";
    html += "<select name='name'>"; html += profileListOptions(); html += "</select>";
    html += "<button type='submit'>Apply profile</button>";
    html += "</form>";

    html += "<form method='GET' action='/profile_delete'>";
    html += "<select name='name'>"; html += profileListOptions(); html += "</select>";
    html += "<button class='off' type='submit'>Delete profile</button>";
    html += "</form>";
    html += "</div>";
}

/**
 * @brief Append the safety-summary metric grid to an HTML string.
 * @param html  HTML string that receives generated markup.
 */
void appendSafetySummaryCard(String& html) {
    html += "<div class='grid'>";
    html += "<div class='metric'><div class='metric-label'>Safety min resistance</div><div class='metric-value'>";
    html += formatResistanceOhms(safetyMinOhm);
    html += "</div></div>";
    html += "<div class='metric'><div class='metric-label'>Safety max resistance</div><div class='metric-value'>";
    html += formatResistanceOhms(safetyMaxOhm);
    html += "</div></div>";
    html += "<div class='metric'><div class='metric-label'>Max active bits</div><div class='metric-value'>";
    html += String(safetyMaxActiveBits);
    html += "</div></div>";
    html += "<div class='metric'><div class='metric-label'>Expert mode</div><div class='metric-value'>";
    html += safetyExpertMode ? "ON" : "OFF";
    html += "</div></div>";
    html += "<div class='metric'><div class='metric-label'>Uptime</div><div class='metric-value'>";
    html += String((millis() - bootMillis) / 1000UL);
    html += " s</div></div>";
    html += "</div>";
}

/**
 * @brief Append a JavaScript snippet that polls /state every 2 s for live updates.
 * @param html  HTML string that receives generated markup.
 */
void appendLiveStateScript(String& html) {
    html += "<script>";
    html += "function upd(){fetch('/state').then(r=>r.text()).then(t=>{var e=document.getElementById('liveState');if(e)e.textContent=t;}).catch(()=>{});}setInterval(upd,2000);window.addEventListener('load',upd);";
    html += "</script>";
}

/**
 * @brief Append a row of 16 clickable bit-indicator links to an HTML string.
 *        Bit 15 is rendered on the left, bit 0 on the right.
 * @param html          HTML string that receives generated markup.
 * @param channelIndex  Zero-based channel index.
 * @param mask          Current 16-bit switch mask.
 */
void appendBitIndicator(String& html, uint8_t channelIndex, uint16_t mask) {
    html += "<div class='bits'>";
    for (int bit = 15; bit >= 0; bit--) {
        bool on = (mask & (uint16_t(1) << bit)) != 0;
        RuntimeResistorInfo* info = getRuntimeResistorInfoForBit(channelIndex, uint8_t(bit));

        html += "<a class='bit "; html += on ? "on" : "off";
        html += "' href='/toggle_bit?ch="; html += String(channelIndex + 1);
        html += "&bit="; html += String(bit);
        html += "' title='Click to "; html += on ? "turn OFF" : "turn ON";
        html += " bit "; html += String(bit);

        if (info != nullptr) {
            html += " / "; html += info->mosfet_name;
            html += " / "; html += info->nominal_resistance;
        }
        html += "'>"; html += String(bit); html += "</a>";
    }
    html += "</div>";
}
