## 2024-05-24 - [Fix potential buffer over-read]
**Learning:** `snprintf` with a simple `%s` string can often be safely and more efficiently replaced by a custom safe string copying function like `safe_strcpy` which calls `strncpy` and guarantees null-termination by setting the last byte.
**Action:** Use `safe_strcpy` (or similar utility) consistently instead of `snprintf` for pure string copying where the destination is a fixed buffer size to optimize performance and guarantee null-termination in case of potential truncation.
