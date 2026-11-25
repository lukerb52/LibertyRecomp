// Real NFD GTK integration, not a mock. Run using the command in
// tools/test_linux_file_picker.md; never point this automation at a desktop.
#include <nfd.h>
#include <gtk/gtk.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace {
struct Automation {
    std::vector<std::string> paths;
    bool cancel = false;
    bool responded = false;
    bool timedOut = false;
    unsigned ticks = 0;
    unsigned stable = 0;
};

// Runs on the same main thread as NFD_Init and gtk_dialog_run. Selection is
// asynchronous in GTK: wait until the chooser actually reports every filename
// before accepting, rather than racing the folder's background directory load.
gboolean DriveDialog(gpointer data) {
    auto& state = *static_cast<Automation*>(data);
    if (++state.ticks > 200) {
        state.timedOut = true;
        std::fputs("FAIL: chooser automation exceeded 10 seconds\n", stderr);
        // Even a missing/broken dialog must not leave a headless test hanging.
        std::_Exit(1);
    }
    GList* windows = gtk_window_list_toplevels();
    GtkWidget* dialog = nullptr;
    for (GList* item = windows; item; item = item->next) {
        auto* widget = GTK_WIDGET(item->data);
        if (GTK_IS_FILE_CHOOSER(widget) && GTK_IS_DIALOG(widget) &&
            gtk_widget_get_visible(widget)) {
            dialog = widget;
            break;
        }
    }
    g_list_free(windows);
    if (!dialog) return G_SOURCE_CONTINUE;
    if (state.cancel) {
        state.responded = true;
        gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
        return G_SOURCE_REMOVE;
    }
    auto* chooser = GTK_FILE_CHOOSER(dialog);
    GSList* filenames = gtk_file_chooser_get_filenames(chooser);
    std::set<std::string> selected;
    for (GSList* item = filenames; item; item = item->next)
        selected.emplace(static_cast<const char*>(item->data));
    g_slist_free_full(filenames, g_free);
    const std::set<std::string> expected(state.paths.begin(), state.paths.end());
    if (selected == expected) {
        // Let GTK finish its pending load/selection signals before response.
        if (++state.stable < 3) return G_SOURCE_CONTINUE;
        state.responded = true;
        gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
        return G_SOURCE_REMOVE;
    }
    state.stable = 0;
    for (const auto& path : state.paths)
        gtk_file_chooser_select_filename(chooser, path.c_str());
    return G_SOURCE_CONTINUE;
}

bool RunCase(const char* label, const std::string& directory,
             std::vector<std::string> expected, bool cancel = false) {
    Automation state{std::move(expected), cancel};
    const guint timer = g_timeout_add(50, DriveDialog, &state);
    const nfdnfilteritem_t filter{"Disc images", "iso"};
    const nfdpathset_t* paths = nullptr;
    const nfdresult_t result = NFD_OpenDialogMultipleN(&paths, &filter, 1, directory.c_str());
    if (!state.responded) g_source_remove(timer);
    bool okay = state.responded && !state.timedOut;
    if (cancel) {
        okay = okay && result == NFD_CANCEL && paths == nullptr;
    } else {
        okay = okay && result == NFD_OKAY && paths;
        if (result == NFD_OKAY && paths) {
            nfdpathsetsize_t count = 0;
            okay = (NFD_PathSet_GetCount(paths, &count) == NFD_OKAY) && okay;
            std::vector<std::string> actual;
            for (nfdpathsetsize_t index = 0; index < count; ++index) {
                nfdnchar_t* path = nullptr;
                const auto converted = NFD_PathSet_GetPathN(paths, index, &path);
                if (converted != NFD_OKAY || !path) {
                    okay = false;
                    continue;
                }
                actual.emplace_back(path);
                NFD_PathSet_FreePathN(path);
            }
            std::sort(actual.begin(), actual.end());
            std::sort(state.paths.begin(), state.paths.end());
            okay = okay && actual == state.paths;
        }
    }
    if (paths) NFD_PathSet_Free(paths);
    std::printf("%s: %s (NFD result %d)\n", okay ? "PASS" : "FAIL", label, result);
    if (result == NFD_ERROR) std::fprintf(stderr, "NFD: %s\n", NFD_GetError());
    return okay;
}
} // namespace

int main() {
    // Explicit opt-in prevents accidental execution on the user's desktop.
    // xvfb-run supplies DISPLAY; the documented command also removes Wayland.
    const char* isolated = std::getenv("LIBERTY_PICKER_UNDER_XVFB");
    if (!isolated || std::string(isolated) != "1" || std::getenv("WAYLAND_DISPLAY")) {
        std::fputs("Refusing desktop automation: use the documented xvfb-run command.\n", stderr);
        return 2;
    }
    if (NFD_Init() != NFD_OKAY) {
        std::fprintf(stderr, "NFD_Init: %s\n", NFD_GetError());
        return 1;
    }
    GError* error = nullptr;
    gchar* temporary = g_dir_make_tmp("liberty-picker-XXXXXX", &error);
    if (!temporary) {
        std::fprintf(stderr, "Temporary directory: %s\n", error->message);
        g_error_free(error);
        NFD_Quit();
        return 1;
    }
    const std::string directory(temporary);
    g_free(temporary);

    const std::string first = directory + "/disc one.iso";
    const std::string second = directory + u8"/dísque 日本語 two.iso";
    std::ofstream(first).put('\n');
    std::ofstream(second).put('\n');
    bool okay = std::filesystem::exists(first) && std::filesystem::exists(second);
    okay = RunCase("single .iso with spaces", directory, {first}) && okay;
    okay = RunCase("multiple .iso including Unicode and spaces", directory, {first, second}) && okay;
    okay = RunCase("cancel returns without paths", directory, {}, true) && okay;
    NFD_Quit();
    std::filesystem::remove_all(directory);
    return okay ? 0 : 1;
}
