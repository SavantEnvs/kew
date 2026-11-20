/* Link-only stand-ins for three functions in modules the theme/library-cache harnesses do not
 * otherwise need (playlist.c and settings.c pull in the TUI, audio and every decoder). None is
 * reached from load_theme_from_file or read_tree_from_binary: empty_playlist is called by
 * appstate.c's playlist teardown, is_music_file by count_music_files_in_directory, and
 * get_system_data_dir by ensure_default_themes. */
#include <stdbool.h>
#include <stddef.h>
typedef struct PlayList PlayList;
void empty_playlist(PlayList *list) { (void)list; }
int is_music_file(const char *filename) { (void)filename; return 0; }
const char *get_system_data_dir(void) { return "/nonexistent"; }
