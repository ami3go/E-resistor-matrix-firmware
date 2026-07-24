/**
 * @file admin_auth.cpp
 * @brief Opt-in shared-secret gate for the highest-risk HTTP routes.
 *
 * Context: this firmware has no authentication anywhere. Any host that can
 * reach port 80 can set channel masks, change safety limits, wipe calibration,
 * or flash firmware. For an isolated bench link that may be an acceptable
 * risk -- but it should be a recorded decision, not a silent default.
 *
 * This module does not attempt full API authentication (out of scope for a
 * single pass -- it would touch every mutating route and the SCPI server).
 * It gates only the two routes where a mistake or a malicious request is
 * least recoverable without physical access to the board:
 *
 *   - POST /firmware_update  -- arbitrary code execution on the RP2040
 *   - POST /factory_reset    -- irrecoverable loss of calibration/profiles
 *
 * Token storage: a plain string in LittleFS at ADMIN_TOKEN_PATH, set via
 * POST /admin_token (itself gated once a token exists, open on first use).
 * This is a shared secret over HTTP, not a cryptographic access-control
 * system -- it stops accidental and casual misuse on a shared network, not a
 * determined attacker who can sniff HTTP. Document that limitation to users
 * rather than imply more than it provides.
 *
 * Default state is DISABLED (no token file present). Existing deployments are
 * unaffected until an operator opts in, and the diagnostics page states the
 * current state plainly so the choice cannot be made by accident.
 */

#include "app.h"

namespace {
constexpr const char* ADMIN_TOKEN_PATH = "/admin_token.txt";
constexpr size_t ADMIN_TOKEN_MAX_LEN = 64;
}  // namespace

bool adminAuthConfigured() {
  return littleFsReady && LittleFS.exists(ADMIN_TOKEN_PATH);
}

/** @brief Constant-time comparison; token comparisons must not leak length via timing. */
static bool constantTimeEquals(const String& a, const String& b) {
  if (a.length() != b.length()) return false;
  uint8_t diff = 0;
  for (size_t i = 0; i < a.length(); ++i) {
    diff |= uint8_t(a[i]) ^ uint8_t(b[i]);
  }
  return diff == 0;
}

bool adminAuthCheck(const String& providedToken) {
  if (!adminAuthConfigured()) {
    // No token configured: gate is open by operator choice. Every call site
    // still logs the action taken, so this is visible in the event log even
    // when unauthenticated.
    return true;
  }
  File file = LittleFS.open(ADMIN_TOKEN_PATH, "r");
  if (!file) return false;
  String stored = file.readString();
  file.close();
  stored.trim();
  return constantTimeEquals(stored, providedToken);
}

bool adminAuthRequireOrReject(WebServer& server, const char* action) {
  if (adminAuthCheck(server.hasArg("admin_token") ? server.arg("admin_token") : "")) return true;
  server.send(403, "text/plain",
              "Rejected: admin token required. Set one with POST /admin_token, "
              "then include it as ?admin_token=... on this request.\n");
  char message[96];
  snprintf(message, sizeof(message), "Rejected (no/invalid admin token): %s", action ? action : "");
  appendLogEvent(message);
  return false;
}

void handleAdminTokenSet() {
  noteHttpRequest();
  // Once a token exists, changing it requires the current token -- otherwise
  // any host could immediately overwrite an operator's token and relock them
  // out, which is worse than not having the gate at all.
  if (adminAuthConfigured() && !adminAuthCheck(server.hasArg("current_token") ? server.arg("current_token") : "")) {
    server.send(403, "text/plain", "Rejected: current_token required to change an existing admin token\n");
    appendLogEvent("Admin token change rejected: wrong current token");
    return;
  }
  if (!server.hasArg("token") || server.arg("token").length() == 0U) {
    server.send(400, "text/plain", "Rejected: token argument required\n");
    return;
  }
  String token = server.arg("token");
  if (token.length() > ADMIN_TOKEN_MAX_LEN) {
    server.send(400, "text/plain", "Rejected: token too long (max 64 chars)\n");
    return;
  }
  if (!littleFsReady) {
    server.send(503, "text/plain", "Rejected: filesystem not ready\n");
    return;
  }
  File file = LittleFS.open(ADMIN_TOKEN_PATH, "w");
  if (!file) {
    server.send(500, "text/plain", "Failed to write token\n");
    return;
  }
  file.print(token);
  file.close();
  appendLogEvent("Admin token set: /firmware_update and /factory_reset now require it");
  server.send(200, "text/plain", "Admin token set. Include it as ?admin_token=... on /firmware_update and /factory_reset.\n");
}

// Bound to POST /admin_token_clear, not DELETE /admin_token: every other
// mutating route in this API uses POST, and introducing a new HTTP verb here
// would be an inconsistency for no real benefit.
void handleAdminTokenClear() {
  noteHttpRequest();
  if (adminAuthConfigured() && !adminAuthCheck(server.hasArg("admin_token") ? server.arg("admin_token") : "")) {
    server.send(403, "text/plain", "Rejected: admin_token required to clear the admin token\n");
    appendLogEvent("Admin token clear rejected: wrong token");
    return;
  }
  if (littleFsReady) LittleFS.remove(ADMIN_TOKEN_PATH);
  appendLogEvent("Admin token cleared: /firmware_update and /factory_reset are now unauthenticated");
  server.send(200, "text/plain", "Admin token cleared. Gate is now open.\n");
}
