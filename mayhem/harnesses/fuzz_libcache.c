/* kew's library cache (src/data/directorytree.c read_tree_from_binary): the binary file kew
 * writes to its cache dir and reloads at start-up — header, entry table, string table, parent
 * links by id. The loader takes a path, so the input goes to a scratch file.
 *
 * After loading, the harness runs what the player itself does with the tree built from the
 * cache (src/ops/library_ops.c, search_ops.c):
 *   - library_init: sort_library_by -> sort_file_system_tree with compare_entry_natural
 *     (SORT_BY_NAME) and compare_folders_by_age_files_alphabetically (SORT_BY_DATE);
 *   - the library view's directory counter: count_directories_in_directory;
 *   - update_library_if_changed_detected: copy_is_enqueued -> find_corresponding_entry;
 *   - search_ops.c: fuzzy_search_recursive -> search_match_entry over the cached names;
 *   - library_shutdown/save_library: write_tree_to_binary, and the reload on the next start. */
#include "data/directorytree.h"
#include "scratch.h"

static void on_match(FileSystemEntry *e, int score) { (void)e; (void)score; }

static void exercise(FileSystemEntry *root) {
        sort_file_system_tree(root, compare_entry_natural);
        sort_file_system_tree(root, compare_folders_by_age_files_alphabetically);
        (void)count_directories_in_directory(root);
        copy_is_enqueued(root, root);
        fuzzy_search_recursive(root, "a", false, on_match);
        fuzzy_search_recursive(root, "song", true, on_match);
}

int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n) {
        char path[1024], saved[1024];
        scratch_write("library.dat", d, n, path, sizeof path);
        int entries = 0;
        FileSystemEntry *root = read_tree_from_binary(path, "/music", &entries, true);
        unlink(path);
        if (!root) return 0;
        exercise(root);
        snprintf(saved, sizeof saved, "%s/saved.dat", scratch_dir());
        if (write_tree_to_binary(root, saved) == 0) {
                FileSystemEntry *again = read_tree_from_binary(saved, "/music", &entries, false);
                if (again) free_tree(again);
        }
        unlink(saved);
        free_tree(root);
        return 0;
}
