/* Link-only stand-ins for the m3u harness: settings.c and library_ops.c pull in the TUI and the
 * audio engine. Neither is reached from read_m3u_file: get_system_data_dir is called by
 * theme.c's ensure_default_themes, mark_as_enqueued by playlist.c's make_playlist/enqueue code. */
typedef struct FileSystemEntry FileSystemEntry;
const char *get_system_data_dir(void) { return "/nonexistent"; }
int mark_as_enqueued(FileSystemEntry *root, char *path, int list_row_num) { (void)root; (void)path; (void)list_row_num; return -1; }
