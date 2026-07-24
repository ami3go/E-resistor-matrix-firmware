/**
 * @file http_response_writer.cpp
 * @brief Fixed-size chunked HTTP response writer used by large Gate 5 pages and APIs.
 */
#include "http_api.h"

char HttpResponseWriter::buffer_[HttpResponseWriter::BUFFER_SIZE] = {0};

HttpResponseWriter::HttpResponseWriter()
    : used_(0), bytesWritten_(0), started_(false), ok_(true) {}

bool HttpResponseWriter::begin(int statusCode, const char* contentType, bool cacheable) {
  used_ = 0;
  bytesWritten_ = 0;
  ok_ = true;
  started_ = true;
  if (cacheable) {
    server.sendHeader("Cache-Control", "public, max-age=60");
  } else {
    sendNoCacheHeaders();
  }
  if (!server.chunkedResponseModeStart(statusCode, contentType)) {
    started_ = false;
    ok_ = false;
    return false;
  }
  ++httpStreamedResponseCount;
  return true;
}

bool HttpResponseWriter::flush() {
  if (!started_ || !ok_) return false;
  if (used_ == 0) return true;
  server.sendContent(buffer_, used_);
  bytesWritten_ += uint32_t(used_);
  used_ = 0;
  return true;
}

bool HttpResponseWriter::writeChar(char value) {
  if (!started_ || !ok_) return false;
  if (used_ >= BUFFER_SIZE) {
    if (!flush()) return false;
  }
  buffer_[used_++] = value;
  return true;
}

bool HttpResponseWriter::write(const char* text) {
  if (!text) return true;
  while (*text) {
    const size_t available = BUFFER_SIZE - used_;
    if (available == 0) {
      if (!flush()) return false;
      continue;
    }
    const size_t remaining = strlen(text);
    const size_t count = remaining < available ? remaining : available;
    memcpy(buffer_ + used_, text, count);
    used_ += count;
    text += count;
  }
  return true;
}

bool HttpResponseWriter::write(const String& text) {
  const size_t length = text.length();
  size_t offset = 0;
  while (offset < length) {
    const size_t available = BUFFER_SIZE - used_;
    if (available == 0) {
      if (!flush()) return false;
      continue;
    }
    const size_t remaining = length - offset;
    const size_t count = remaining < available ? remaining : available;
    memcpy(buffer_ + used_, text.c_str() + offset, count);
    used_ += count;
    offset += count;
  }
  return true;
}

bool HttpResponseWriter::printf(const char* format, ...) {
  if (!format) return true;
  char local[256];
  va_list args;
  va_start(args, format);
  const int length = vsnprintf(local, sizeof(local), format, args);
  va_end(args);
  if (length < 0) {
    ok_ = false;
    return false;
  }
  if (size_t(length) < sizeof(local)) return write(local);

  // Large formatted fields are intentionally rare; truncate instead of allocating.
  local[sizeof(local) - 2U] = '~';
  local[sizeof(local) - 1U] = '\0';
  return write(local);
}

void HttpResponseWriter::end() {
  if (!started_) return;
  flush();
  server.chunkedResponseFinalize();
  started_ = false;
}
