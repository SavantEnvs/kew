/* The decoders log through k_log()/set_error_message(), defined in src/utils/k_log.c and
 * src/common/common.c, which drag in the whole player state (appstate/model). They only
 * record diagnostics, so the harnesses use no-op versions. */
void k_log(const char *fmt, ...) { (void)fmt; }
void set_error_message(const char *m) { (void)m; }
