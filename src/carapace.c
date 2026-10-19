/* Carapace, the SnapOS store. The first page has a featured program, the
 * popular ones, the categories and a row of programs for each one; the search
 * looks through every program in Nix at once (an index made from the pinned
 * nixpkgs). Installing runs `snapos add` through pkexec, which adds the
 * program to /etc/snapos and builds the next system, so anything installed
 * can be removed or undone like every other change. While it works, Snappy
 * plays one of his little scenes. */
#include <gtk/gtk.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef DATADIR
#define DATADIR "share/carapace"
#endif

typedef struct {
    char *attr, *name, *icon, *cat, *summary, *version;
} App;

static App *apps;                 /* the catalogue: first page and categories */
static int napps;
static App *index_;               /* every program in Nix, from index.tsv */
static int nindex;
static GHashTable *installed;

static const char *CATS[] = { "Internet", "Games", "Graphics", "Audio & Video", "Office", "Development", "Utilities", "Education", NULL };
static const char *CAT_CLASS[] = { "c-internet", "c-games", "c-graphics", "c-media", "c-office", "c-dev", "c-utils", "c-edu" };
static const char *FEATURED[] = { "steam", "blender", "obs-studio", "discord", "krita", "vlc", "godot_4", "spotify", NULL };

static struct {
    GtkWidget *win, *stack, *search, *side;
    GtkWidget *explore, *cat_title, *cat_grid, *results, *results_title;
    GtkWidget *hero, *hero_icon, *hero_name, *hero_summary;
    GtkWidget *page_icon, *page_name, *page_summary, *page_attr, *page_button, *page_cmd, *page_more;
    App *shown, *hero_app;
    int hero_i;
    guint debounce;
} ui;

static const char *datadir(void) {
    const char *d = g_getenv("CARAPACE_DATA");
    return d && *d ? d : DATADIR;
}

static const char *nixdir(void) {
    const char *d = g_getenv("SNAPOS_NIX_DIR");
    if (d && *d) return d;
    if (access("/etc/snapos/configuration.nix", F_OK) == 0) return "/etc/snapos";
    return "/etc/nixos";
}

/* ---- the catalogue, the index and what is installed ---------------------- */

static App *read_tsv(const char *file, int fields, int *count) {
    char *path = g_build_filename(datadir(), file, NULL);
    char *text = NULL;
    App *out = NULL;
    *count = 0;
    if (g_file_get_contents(path, &text, NULL, NULL)) {
        char **lines = g_strsplit(text, "\n", -1);
        int n = g_strv_length(lines);
        out = g_new0(App, n + 1);
        for (int i = 0; i < n; i++) {
            if (!lines[i][0] || lines[i][0] == '#') continue;
            char **f = g_strsplit(lines[i], "\t", fields);
            if ((int)g_strv_length(f) >= fields - 1) {
                App *a = &out[(*count)++];
                if (fields == 5) {        /* apps.tsv: attr, name, icon, category, summary */
                    a->attr = g_strdup(f[0]); a->name = g_strdup(f[1]); a->icon = g_strdup(f[2]);
                    a->cat = g_strdup(f[3]); a->summary = g_strdup(f[4] ? f[4] : "");
                    a->version = g_strdup("");
                } else {                  /* index.tsv: attr, version, description */
                    a->attr = g_strdup(f[0]); a->version = g_strdup(f[1]);
                    a->summary = g_strdup(f[2] ? f[2] : "");
                    a->name = a->attr; a->icon = a->attr; a->cat = "";
                }
            }
            g_strfreev(f);
        }
        g_strfreev(lines);
        g_free(text);
    }
    g_free(path);
    return out;
}

/* The programs between the snapos:packages markers of configuration.nix. */
static void load_installed(void) {
    if (installed) g_hash_table_remove_all(installed);
    else installed = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    char *path = g_build_filename(nixdir(), "configuration.nix", NULL);
    char *cfg = NULL;
    if (g_file_get_contents(path, &cfg, NULL, NULL)) {
        char *b = strstr(cfg, "# snapos:packages:begin");
        char *e = b ? strstr(b, "# snapos:packages:end") : NULL;
        char *p = b ? strchr(b, '[') : NULL;
        if (p && e && p < e) {
            for (p++; p < e && *p != ']'; ) {
                if (isalnum((unsigned char)*p) || *p == '_') {
                    char *s = p;
                    while (p < e && (isalnum((unsigned char)*p) || strchr("._+-", *p))) p++;
                    g_hash_table_add(installed, g_strndup(s, p - s));
                } else if (*p == '#') {
                    while (p < e && *p != '\n') p++;
                } else p++;
            }
        }
        g_free(cfg);
    }
    g_free(path);
}

static gboolean is_installed(const char *attr) { return installed && g_hash_table_contains(installed, attr); }

/* parts of SnapOS itself in the program list: not shown as removable programs */
static gboolean is_part_of_snapos(const char *attr) {
    static const char *parts[] = { "snapguard", "snapos-tools", "snapweb", "dpkg", "pciutils", NULL };
    for (int i = 0; parts[i]; i++) if (!strcmp(parts[i], attr)) return TRUE;
    return FALSE;
}

static App *find_app(const char *attr) {
    for (int i = 0; i < napps; i++) if (!strcmp(apps[i].attr, attr)) return &apps[i];
    return NULL;
}

static App *find_indexed(const char *attr) {
    for (int i = 0; i < nindex; i++) if (!strcmp(index_[i].attr, attr)) return &index_[i];
    return NULL;
}

/* ---- small pieces --------------------------------------------------------- */

/* The desktop's icon theme first; Papirus (SnapOS's icons) when it does not
 * have the program, as on Plasma, whose GTK theme is Breeze. */
static void set_icon(GtkImage *im, const char *name, int px) {
    static GtkIconTheme *papirus;
    GtkIconTheme *t = gtk_icon_theme_get_default();
    if (name && gtk_icon_theme_has_icon(t, name)) {
        gtk_image_set_from_icon_name(im, name, GTK_ICON_SIZE_DIALOG);
        gtk_image_set_pixel_size(im, px);
        return;
    }
    if (!papirus) { papirus = gtk_icon_theme_new(); gtk_icon_theme_set_custom_theme(papirus, "Papirus-Dark"); }
    GdkPixbuf *pb = name ? gtk_icon_theme_load_icon(papirus, name, px, GTK_ICON_LOOKUP_FORCE_SIZE, NULL) : NULL;
    if (pb) { gtk_image_set_from_pixbuf(im, pb); g_object_unref(pb); return; }
    gtk_image_set_from_icon_name(im, "application-x-executable", GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(im, px);
}

static GtkWidget *icon(const char *name, int px) {
    GtkWidget *im = gtk_image_new();
    set_icon(GTK_IMAGE(im), name, px);
    return im;
}

static GtkWidget *label(const char *text, const char *cls, gboolean wrap) {
    GtkWidget *l = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    if (wrap) { gtk_label_set_line_wrap(GTK_LABEL(l), TRUE); gtk_label_set_max_width_chars(GTK_LABEL(l), 46); }
    else gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
    if (cls) gtk_style_context_add_class(gtk_widget_get_style_context(l), cls);
    return l;
}

static void add_class(GtkWidget *w, const char *cls) { gtk_style_context_add_class(gtk_widget_get_style_context(w), cls); }

static void clear(GtkWidget *box) {
    GList *kids = gtk_container_get_children(GTK_CONTAINER(box));
    for (GList *k = kids; k; k = k->next) gtk_widget_destroy(k->data);
    g_list_free(kids);
}

static void show_app(App *a);
static void show_category(const char *cat);
static void on_card(GtkButton *b, gpointer data) { (void)b; show_app(data); }

/* a card: icon, name, what it does, and "Installed" when it is */
static GtkWidget *short_label(const char *text, const char *cls) {
    GtkWidget *l = label(text, cls, FALSE);
    gtk_label_set_max_width_chars(GTK_LABEL(l), 18);
    return l;
}

static GtkWidget *card(App *a, int width) {
    GtkWidget *btn = gtk_button_new();
    add_class(btn, "card");
    if (width) gtk_widget_set_size_request(btn, width, -1);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(box), icon(a->icon, 48), FALSE, FALSE, 0);
    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_valign(text, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(text), short_label(a->name, "card-name"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(text), short_label(a->summary, "dim"), FALSE, FALSE, 0);
    if (is_installed(a->attr)) gtk_box_pack_start(GTK_BOX(text), label("Installed", "ok", FALSE), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), text, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(btn), box);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_card), a);
    return btn;
}

static GtkWidget *section_title(const char *text, const char *more_cat) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(row), label(text, "section", FALSE), FALSE, FALSE, 0);
    if (more_cat) {
        GtkWidget *more = gtk_button_new_with_label("See all");
        add_class(more, "flat");
        g_signal_connect_swapped(more, "clicked", G_CALLBACK(show_category), (gpointer)more_cat);
        gtk_box_pack_end(GTK_BOX(row), more, FALSE, FALSE, 0);
    }
    return row;
}

/* ---- the first page -------------------------------------------------------- */

static void set_hero(App *a) {
    ui.hero_app = a;
    set_icon(GTK_IMAGE(ui.hero_icon), a->icon, 112);
    gtk_label_set_text(GTK_LABEL(ui.hero_name), a->name);
    gtk_label_set_text(GTK_LABEL(ui.hero_summary), a->summary);
}

static gboolean next_hero(gpointer data) {
    (void)data;
    for (int tries = 0; tries < 10; tries++) {
        ui.hero_i = FEATURED[ui.hero_i + 1] ? ui.hero_i + 1 : 0;
        App *a = find_app(FEATURED[ui.hero_i]);
        if (a) { set_hero(a); break; }
    }
    return G_SOURCE_CONTINUE;
}

static void on_hero(GtkButton *b, gpointer data) { (void)b; (void)data; if (ui.hero_app) show_app(ui.hero_app); }

static GtkWidget *build_hero(void) {
    GtkWidget *btn = gtk_button_new();
    add_class(btn, "hero");
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 28);
    ui.hero_icon = gtk_image_new();
    gtk_box_pack_start(GTK_BOX(box), ui.hero_icon, FALSE, FALSE, 0);
    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_valign(text, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(text), label("FEATURED", "hero-tag", FALSE), FALSE, FALSE, 0);
    ui.hero_name = label("", "hero-name", FALSE);
    ui.hero_summary = label("", "hero-summary", TRUE);
    gtk_box_pack_start(GTK_BOX(text), ui.hero_name, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(text), ui.hero_summary, FALSE, FALSE, 0);
    GtkWidget *go = label("See it  →", "hero-go", FALSE);
    gtk_box_pack_start(GTK_BOX(text), go, FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(box), text, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(btn), box);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_hero), NULL);
    return btn;
}

/* a row that scrolls sideways: every program of one category */
static GtkWidget *app_row(const char *cat, App *skip) {
    GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    add_class(sc, "row");
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    for (int i = 0; i < napps; i++)
        if (!strcmp(apps[i].cat, cat) && &apps[i] != skip) gtk_box_pack_start(GTK_BOX(box), card(&apps[i], 270), FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(sc), box);
    gtk_widget_set_size_request(sc, -1, 96);
    return sc;
}

static void on_tile(GtkButton *b, gpointer data) { (void)b; show_category(data); }

static void build_explore(void) {
    GtkWidget *page = ui.explore;
    clear(page);
    gtk_box_pack_start(GTK_BOX(page), ui.hero = build_hero(), FALSE, FALSE, 0);
    ui.hero_i = -1;
    next_hero(NULL);

    gtk_box_pack_start(GTK_BOX(page), section_title("Popular", NULL), FALSE, FALSE, 6);
    GtkWidget *grid = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(grid), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(grid), 1);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(grid), 4);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(grid), 10);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(grid), 10);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(grid), TRUE);
    static const char *popular[] = { "firefox", "discord", "vlc", "obs-studio", "gimp", "steam", "libreoffice", "spotify", "prismlauncher", NULL };
    for (int i = 0; popular[i]; i++) { App *a = find_app(popular[i]); if (a) gtk_flow_box_insert(GTK_FLOW_BOX(grid), card(a, 0), -1); }
    gtk_box_pack_start(GTK_BOX(page), grid, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(page), section_title("Categories", NULL), FALSE, FALSE, 6);
    GtkWidget *tiles = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(tiles), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(tiles), 2);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(tiles), 8);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(tiles), 10);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(tiles), 10);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(tiles), TRUE);
    for (int c = 0; CATS[c]; c++) {
        GtkWidget *t = gtk_button_new_with_label(CATS[c]);
        add_class(t, "tile"); add_class(t, CAT_CLASS[c]);
        g_signal_connect(t, "clicked", G_CALLBACK(on_tile), (gpointer)CATS[c]);
        gtk_flow_box_insert(GTK_FLOW_BOX(tiles), t, -1);
    }
    gtk_box_pack_start(GTK_BOX(page), tiles, FALSE, FALSE, 0);

    for (int c = 0; CATS[c]; c++) {
        gtk_box_pack_start(GTK_BOX(page), section_title(CATS[c], CATS[c]), FALSE, FALSE, 6);
        gtk_box_pack_start(GTK_BOX(page), app_row(CATS[c], NULL), FALSE, FALSE, 0);
    }
    gtk_widget_show_all(page);
}

static void show_explore(void) {
    build_explore();
    gtk_stack_set_visible_child_name(GTK_STACK(ui.stack), "explore");
}

/* ---- a category, or what is installed --------------------------------------- */

static void show_category(const char *cat) {
    clear(ui.cat_grid);
    gboolean inst = !strcmp(cat, "@installed");
    gtk_label_set_text(GTK_LABEL(ui.cat_title), inst ? "Installed" : cat);
    int n = 0;
    for (int i = 0; i < napps; i++) {
        if (inst ? !is_installed(apps[i].attr) : strcmp(apps[i].cat, cat)) continue;
        gtk_flow_box_insert(GTK_FLOW_BOX(ui.cat_grid), card(&apps[i], 0), -1); n++;
    }
    if (inst && installed) {               /* installed, and not in the catalogue */
        GHashTableIter it; gpointer k;
        g_hash_table_iter_init(&it, installed);
        while (g_hash_table_iter_next(&it, &k, NULL)) {
            if (find_app(k) || is_part_of_snapos(k)) continue;
            App *idx = find_indexed(k);
            App *a = g_new0(App, 1);
            a->attr = g_strdup(k); a->name = g_strdup(k); a->icon = g_strdup(k); a->cat = "";
            a->summary = g_strdup(idx ? idx->summary : ""); a->version = g_strdup(idx ? idx->version : "");
            gtk_flow_box_insert(GTK_FLOW_BOX(ui.cat_grid), card(a, 0), -1); n++;
        }
    }
    if (!n) gtk_flow_box_insert(GTK_FLOW_BOX(ui.cat_grid), label(inst ? "Nothing installed from Carapace yet." : "Nothing here yet.", "dim", FALSE), -1);
    gtk_widget_show_all(ui.cat_grid);
    gtk_stack_set_visible_child_name(GTK_STACK(ui.stack), "category");
}

static void on_side(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    (void)box; (void)data;
    if (!row) return;
    const char *cat = g_object_get_data(G_OBJECT(row), "cat");
    if (cat && *cat) show_category(cat);
    else show_explore();
}

/* ---- the app page ------------------------------------------------------------ */

static void install(App *a, gboolean add);

static void on_page_button(GtkButton *b, gpointer data) {
    (void)b; (void)data;
    if (ui.shown) install(ui.shown, !is_installed(ui.shown->attr));
}

static void show_app(App *a) {
    ui.shown = a;
    set_icon(GTK_IMAGE(ui.page_icon), a->icon, 112);
    gtk_label_set_text(GTK_LABEL(ui.page_name), a->name);
    gtk_label_set_text(GTK_LABEL(ui.page_summary), a->summary);
    App *idx = find_indexed(a->attr);
    const char *ver = a->version && *a->version ? a->version : idx ? idx->version : "";
    char *attr = *ver ? g_strdup_printf("Nix package: %s · version %s", a->attr, ver) : g_strdup_printf("Nix package: %s", a->attr);
    gtk_label_set_text(GTK_LABEL(ui.page_attr), attr);
    g_free(attr);
    gboolean have = is_installed(a->attr);
    gtk_button_set_label(GTK_BUTTON(ui.page_button), have ? "Remove" : "Install");
    GtkStyleContext *sc = gtk_widget_get_style_context(ui.page_button);
    if (have) { gtk_style_context_remove_class(sc, "suggested-action"); gtk_style_context_add_class(sc, "destructive-action"); }
    else { gtk_style_context_remove_class(sc, "destructive-action"); gtk_style_context_add_class(sc, "suggested-action"); }
    char *cmd = g_strdup_printf("$ snapos %s %s", have ? "remove" : "add", a->attr);
    gtk_label_set_text(GTK_LABEL(ui.page_cmd), cmd);
    g_free(cmd);
    /* more of the same kind */
    clear(ui.page_more);
    if (a->cat && *a->cat) {
        gtk_box_pack_start(GTK_BOX(ui.page_more), section_title("More like this", a->cat), FALSE, FALSE, 6);
        gtk_box_pack_start(GTK_BOX(ui.page_more), app_row(a->cat, a), FALSE, FALSE, 0);
    }
    gtk_widget_show_all(ui.page_more);
    gtk_stack_set_visible_child_name(GTK_STACK(ui.stack), "app");
}

/* ---- the search: through the whole index, at once ----------------------------- */

/* lower is better: the catalogue, then the name itself, then names that start
 * with it or have it, then the description; programs before desktop sets */
static int relevance(const App *a, const char *q) {
    const char *base = strrchr(a->attr, '.') ? strrchr(a->attr, '.') + 1 : a->attr;
    int nested = strchr(a->attr, '.') ? 1 : 0;
    if (find_app(a->attr)) return 0;
    if (!g_ascii_strcasecmp(base, q)) return 1 + nested;
    if (!g_ascii_strncasecmp(base, q, strlen(q))) return 3 + nested;
    if (strstr(base, q)) return 5 + nested;
    return 7 + nested;
}

static char *lower(const char *s) { return g_ascii_strdown(s, -1); }

typedef struct { App *a; int score; } Hit;
static gint by_score(gconstpointer x, gconstpointer y) {
    const Hit *a = x, *b = y;
    if (a->score != b->score) return a->score - b->score;
    return g_ascii_strcasecmp(a->a->attr, b->a->attr);
}

static void on_result(GtkButton *b, gpointer data) {
    (void)b;
    App *found = data;
    App *known = find_app(found->attr);
    show_app(known ? known : found);
}

static void add_result(App *a) {
    App *known = find_app(a->attr);
    GtkWidget *btn = gtk_button_new();
    add_class(btn, "card");
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(box), icon(known ? known->icon : a->attr, 40), FALSE, FALSE, 0);
    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(text), label(known ? known->name : a->attr, "card-name", FALSE), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(text), label(known ? known->summary : a->summary, "dim", FALSE), FALSE, FALSE, 0);
    char *pkg = g_strdup_printf("%s%s%s", a->attr, a->version && *a->version ? " · " : "", a->version ? a->version : "");
    gtk_box_pack_start(GTK_BOX(text), label(pkg, "mono", FALSE), FALSE, FALSE, 0);
    g_free(pkg);
    gtk_box_pack_start(GTK_BOX(box), text, TRUE, TRUE, 0);
    if (is_installed(a->attr)) gtk_box_pack_end(GTK_BOX(box), label("Installed", "ok", FALSE), FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(btn), box);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_result), a);
    gtk_box_pack_start(GTK_BOX(ui.results), btn, FALSE, FALSE, 0);
}

static gboolean run_search(gpointer data) {
    (void)data;
    ui.debounce = 0;
    const char *raw = gtk_entry_get_text(GTK_ENTRY(ui.search));
    if (!raw || strlen(raw) < 2) { show_explore(); return G_SOURCE_REMOVE; }
    char *q = lower(raw);
    g_strstrip(q);
    GArray *hits = g_array_new(FALSE, FALSE, sizeof(Hit));
    App *pool = nindex ? index_ : apps;
    int npool = nindex ? nindex : napps;
    for (int i = 0; i < npool; i++) {
        App *a = &pool[i];
        char *attr = lower(a->attr);
        gboolean match = strstr(attr, q) != NULL;
        if (!match) { char *d = lower(a->summary); match = strstr(d, q) != NULL; g_free(d); }
        App *k = find_app(a->attr);
        if (!match && k) { char *n = lower(k->name); match = strstr(n, q) != NULL; g_free(n); }
        g_free(attr);
        if (match) { Hit h = { a, relevance(a, q) }; g_array_append_val(hits, h); }
    }
    g_array_sort(hits, by_score);
    clear(ui.results);
    for (guint i = 0; i < hits->len && i < 200; i++) add_result(g_array_index(hits, Hit, i).a);
    char *t = hits->len ? g_strdup_printf("%u programs found", hits->len) : g_strdup("Nothing found. Try another word.");
    gtk_label_set_text(GTK_LABEL(ui.results_title), t);
    if (g_getenv("CARAPACE_TEST_SEARCH")) printf("CARAPACE-FOUND %u\n", hits->len), fflush(stdout);
    g_free(t);
    g_array_free(hits, TRUE);
    g_free(q);
    gtk_widget_show_all(ui.results);
    gtk_stack_set_visible_child_name(GTK_STACK(ui.stack), "results");
    return G_SOURCE_REMOVE;
}

static void on_search(GtkEntry *e, gpointer data) {
    (void)e; (void)data;
    if (ui.debounce) g_source_remove(ui.debounce);
    ui.debounce = g_timeout_add(180, run_search, NULL);
}

/* ---- installing: pkexec snapos add, a progress window and one of Snappy's scenes */

typedef struct {
    GtkWidget *dlg, *title, *sub, *bar, *step[4], *last, *done;
    GDataInputStream *in;
    GSubprocess *proc;
    App *app;
    gboolean add;
    int stage;
    guint pulse;
} Job;

static const char *STEPS_ADD[4] = { "Checking the program", "Adding it to /etc/snapos", "Building the next system", "Ready: it opens from the menu" };
static const char *STEPS_REMOVE[4] = { "Checking", "Taking it out of /etc/snapos", "Building the next system", "Removed" };

static char *strip_ansi(const char *s) {
    GString *o = g_string_new(NULL);
    for (const char *p = s; *p; p++) {
        if (*p == 0x1b) { while (*p && !g_ascii_isalpha(*p)) p++; if (!*p) break; continue; }
        g_string_append_c(o, *p);
    }
    return g_string_free(o, FALSE);
}

static void set_stage(Job *j, int s) {
    if (s <= j->stage) return;
    j->stage = s;
    for (int i = 0; i < 4; i++) {
        const char *mark = i < s ? "✓" : i == s ? "●" : "○";
        char *t = g_strdup_printf("%s  %s", mark, (j->add ? STEPS_ADD : STEPS_REMOVE)[i]);
        gtk_label_set_text(GTK_LABEL(j->step[i]), t);
        g_free(t);
    }
}

static gboolean pulse(gpointer data) {
    Job *j = data;
    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(j->bar));
    return G_SOURCE_CONTINUE;
}

static void read_line(Job *j);

static void on_line(GObject *src, GAsyncResult *res, gpointer data) {
    Job *j = data;
    char *line = g_data_input_stream_read_line_finish_utf8(G_DATA_INPUT_STREAM(src), res, NULL, NULL);
    if (!line) return;
    char *clean = strip_ansi(line);
    g_strstrip(clean);
    if (*clean) {
        gtk_label_set_text(GTK_LABEL(j->last), clean);
        if (strstr(clean, "snapctl") || strstr(clean, "declared") || strstr(clean, "added") || strstr(clean, "removed")) set_stage(j, 2);
        if (strstr(clean, "building") || strstr(clean, "copying path") || strstr(clean, "rebuild")) set_stage(j, 2);
    }
    g_free(clean);
    g_free(line);
    read_line(j);
}

static void read_line(Job *j) {
    g_data_input_stream_read_line_async(j->in, G_PRIORITY_DEFAULT, NULL, on_line, j);
}

static void on_done(GObject *src, GAsyncResult *res, gpointer data) {
    Job *j = data;
    g_subprocess_wait_finish(G_SUBPROCESS(src), res, NULL);
    int ok = g_subprocess_get_if_exited(j->proc) && g_subprocess_get_exit_status(j->proc) == 0;
    int st = g_subprocess_get_if_exited(j->proc) ? g_subprocess_get_exit_status(j->proc) : -1;
    g_source_remove(j->pulse);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(j->bar), ok ? 1.0 : 0.0);
    if (ok) {
        set_stage(j, 4);
        char *t = g_strdup_printf("%s is %s", j->app->name, j->add ? "installed" : "removed");
        gtk_label_set_text(GTK_LABEL(j->title), t);
        g_free(t);
        gtk_label_set_text(GTK_LABEL(j->sub), j->add ? "Find it in the menu. Undo anytime with snapos rollback." : "It is gone from the next system.");
    } else {
        gtk_label_set_text(GTK_LABEL(j->title), st == 126 || st == 127 ? "Cancelled" : "That did not work");
        gtk_label_set_text(GTK_LABEL(j->sub), "Nothing changed: your system is as it was.");
    }
    gtk_widget_set_sensitive(j->done, TRUE);
    load_installed();
    if (ui.shown) show_app(ui.shown);
    if (g_getenv("CARAPACE_TEST_INSTALL")) printf("CARAPACE-%s %s\n", ok ? "OK" : "FAILED", j->app->attr), fflush(stdout);
}

static char *pick_scene(void) {
    const char *fixed = g_getenv("CARAPACE_SCENE");
    char *dir = g_build_filename(datadir(), "scenes", NULL);
    if (fixed && *fixed) { char *f = g_strdup_printf("%s/%s.gif", dir, fixed); g_free(dir); return f; }
    GPtrArray *all = g_ptr_array_new_with_free_func(g_free);
    GDir *d = g_dir_open(dir, 0, NULL);
    if (d) {
        const char *n;
        while ((n = g_dir_read_name(d))) if (g_str_has_suffix(n, ".gif")) g_ptr_array_add(all, g_build_filename(dir, n, NULL));
        g_dir_close(d);
    }
    char *pick = all->len ? g_strdup(g_ptr_array_index(all, g_random_int_range(0, all->len))) : NULL;
    g_ptr_array_free(all, TRUE);
    g_free(dir);
    return pick;
}

static void on_close(GtkButton *b, gpointer data) { (void)b; gtk_widget_destroy(((Job *)data)->dlg); }

static void install(App *a, gboolean add) {
    Job *j = g_new0(Job, 1);
    j->app = a; j->add = add; j->stage = -1;
    j->dlg = gtk_dialog_new();
    gtk_window_set_transient_for(GTK_WINDOW(j->dlg), GTK_WINDOW(ui.win));
    gtk_window_set_modal(GTK_WINDOW(j->dlg), TRUE);
    gtk_window_set_title(GTK_WINDOW(j->dlg), add ? "Installing" : "Removing");
    /* the window fits the screen: on a small one its content scrolls */
    GdkRectangle work = { 0, 0, 1280, 800 };
    GdkMonitor *mon = gdk_display_get_monitor_at_window(gdk_display_get_default(), gtk_widget_get_window(ui.win));
    if (mon) gdk_monitor_get_workarea(mon, &work);
    gtk_window_set_default_size(GTK_WINDOW(j->dlg), MIN(680, work.width - 40), MIN(640, work.height - 60));
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(j->dlg));
    GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(content), sc, TRUE, TRUE, 0);
    GtkWidget *area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(area), 18);
    gtk_container_add(GTK_CONTAINER(sc), area);
    GtkWidget *head = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_box_pack_start(GTK_BOX(head), icon(a->icon, 56), FALSE, FALSE, 0);
    GtkWidget *tt = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    char *t = g_strdup_printf("%s %s", add ? "Installing" : "Removing", a->name);
    j->title = label(t, "title", FALSE);
    g_free(t);
    j->sub = label("This takes a few minutes. Your current system stays as it is until it finishes.", "dim", TRUE);
    gtk_box_pack_start(GTK_BOX(tt), j->title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tt), j->sub, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(head), tt, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(area), head, FALSE, FALSE, 0);
    j->bar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(area), j->bar, FALSE, FALSE, 4);
    for (int i = 0; i < 4; i++) { j->step[i] = label("", "mono", FALSE); gtk_box_pack_start(GTK_BOX(area), j->step[i], FALSE, FALSE, 0); }
    j->last = label("", "dim-mono", FALSE);
    gtk_box_pack_start(GTK_BOX(area), j->last, FALSE, FALSE, 0);
    char *scene = pick_scene();
    if (scene) {
        GdkPixbufAnimation *anim = gdk_pixbuf_animation_new_from_file(scene, NULL);
        if (anim) {
            GtkWidget *frame = gtk_event_box_new();
            add_class(frame, "scene");
            gtk_container_add(GTK_CONTAINER(frame), gtk_image_new_from_animation(anim));
            gtk_box_pack_start(GTK_BOX(area), frame, FALSE, FALSE, 6);
            g_object_unref(anim);
        }
        g_free(scene);
    }
    j->done = gtk_button_new_with_label("Close");
    gtk_widget_set_sensitive(j->done, FALSE);
    gtk_widget_set_halign(j->done, GTK_ALIGN_END);
    g_signal_connect(j->done, "clicked", G_CALLBACK(on_close), j);
    gtk_box_pack_start(GTK_BOX(area), j->done, FALSE, FALSE, 0);
    set_stage(j, 0);
    gtk_widget_show_all(j->dlg);

    /* pkexec needs the full path; CARAPACE_RUN replaces pkexec in tests */
    char *snapos = g_find_program_in_path("snapos");
    const char *runner = g_getenv("CARAPACE_RUN");
    GError *err = NULL;
    GSubprocessFlags fl = G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE;
    if (runner && *runner)
        j->proc = g_subprocess_new(fl, &err, "sh", "-c", runner, "carapace", add ? "add" : "remove", a->attr, NULL);
    else
        j->proc = g_subprocess_new(fl, &err, "pkexec", snapos ? snapos : "/run/current-system/sw/bin/snapos", add ? "add" : "remove", a->attr, NULL);
    g_free(snapos);
    if (!j->proc) {
        gtk_label_set_text(GTK_LABEL(j->title), "Could not start");
        gtk_label_set_text(GTK_LABEL(j->sub), err ? err->message : "");
        gtk_widget_set_sensitive(j->done, TRUE);
        g_clear_error(&err);
        return;
    }
    set_stage(j, 1);
    j->pulse = g_timeout_add(120, pulse, j);
    j->in = g_data_input_stream_new(g_subprocess_get_stdout_pipe(j->proc));
    read_line(j);
    g_subprocess_wait_async(j->proc, NULL, on_done, j);
}

/* ---- the window ---------------------------------------------------------------- */

static const char *CSS =
    ".side { background: alpha(@theme_fg_color, 0.03); }"
    ".side row { padding: 9px 16px; border-radius: 8px; margin: 1px 8px; }"
    ".side .heading { font-size: 11px; opacity: 0.55; padding: 16px 16px 4px; }"
    ".card { padding: 12px; border-radius: 12px; }"
    ".card-name { font-weight: bold; font-size: 14px; }"
    ".section { font-weight: bold; font-size: 18px; margin-top: 10px; }"
    ".title { font-weight: bold; font-size: 20px; }"
    ".page-name { font-weight: bold; font-size: 32px; }"
    ".dim { opacity: 0.7; }"
    ".ok { color: #5cc86e; font-size: 12px; }"
    ".mono { font-family: monospace; font-size: 12px; opacity: 0.75; }"
    ".dim-mono { font-family: monospace; font-size: 11px; opacity: 0.55; }"
    ".cmd { font-family: monospace; padding: 10px 14px; border-radius: 8px; background: alpha(@theme_fg_color, 0.06); }"
    ".scene { background: #0c0808; border-radius: 10px; padding: 6px; }"
    ".hero { padding: 28px 36px; border-radius: 18px; border: none;"
    "        background-image: linear-gradient(110deg, #1b2838 0%, #2a3f5a 55%, #6a1a26 100%); color: #f4f0ee; }"
    ".hero:hover { background-image: linear-gradient(110deg, #22344a 0%, #34506f 55%, #7d2030 100%); }"
    ".hero-tag { font-size: 12px; font-weight: bold; letter-spacing: 2px; color: #ffb3a6; }"
    ".hero-name { font-size: 34px; font-weight: bold; color: #ffffff; }"
    ".hero-summary { font-size: 15px; color: #d6dde6; }"
    ".hero-go { font-weight: bold; color: #ffffff; }"
    ".tile { padding: 20px 14px; border-radius: 12px; border: none; font-weight: bold; color: #ffffff; }"
    ".c-internet { background-image: linear-gradient(135deg, #1e3a5f, #2c6aa0); }"
    ".c-games    { background-image: linear-gradient(135deg, #3b1e5f, #7a3cc0); }"
    ".c-graphics { background-image: linear-gradient(135deg, #5f3a12, #c47a1e); }"
    ".c-media    { background-image: linear-gradient(135deg, #5f1238, #c02a6a); }"
    ".c-office   { background-image: linear-gradient(135deg, #154a2a, #2f9a55); }"
    ".c-dev      { background-image: linear-gradient(135deg, #123f4a, #1f8aa0); }"
    ".c-utils    { background-image: linear-gradient(135deg, #3a3a42, #6a6a78); }"
    ".c-edu      { background-image: linear-gradient(135deg, #5f4a12, #c4a01e); }";

static GtkWidget *side_row(const char *text, const char *cat) {
    GtkWidget *row = gtk_list_box_row_new();
    gtk_container_add(GTK_CONTAINER(row), label(text, NULL, FALSE));
    g_object_set_data_full(G_OBJECT(row), "cat", g_strdup(cat), g_free);
    return row;
}

static GtkWidget *scrolled(GtkWidget *child) {
    GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(sc), child);
    return sc;
}

static GtkWidget *build_page(void) {
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_container_set_border_width(GTK_CONTAINER(page), 28);
    GtkWidget *back = gtk_button_new_with_label("← Back");
    gtk_widget_set_halign(back, GTK_ALIGN_START);
    add_class(back, "flat");
    g_signal_connect_swapped(back, "clicked", G_CALLBACK(show_explore), NULL);
    gtk_box_pack_start(GTK_BOX(page), back, FALSE, FALSE, 0);
    GtkWidget *head = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 24);
    ui.page_icon = gtk_image_new();
    gtk_box_pack_start(GTK_BOX(head), ui.page_icon, FALSE, FALSE, 0);
    GtkWidget *tt = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_valign(tt, GTK_ALIGN_CENTER);
    ui.page_name = label("", "page-name", FALSE);
    ui.page_summary = label("", "dim", TRUE);
    ui.page_attr = label("", "mono", FALSE);
    ui.page_button = gtk_button_new_with_label("Install");
    gtk_widget_set_halign(ui.page_button, GTK_ALIGN_START);
    gtk_widget_set_size_request(ui.page_button, 140, 38);
    g_signal_connect(ui.page_button, "clicked", G_CALLBACK(on_page_button), NULL);
    gtk_box_pack_start(GTK_BOX(tt), ui.page_name, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tt), ui.page_summary, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tt), ui.page_attr, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tt), ui.page_button, FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(head), tt, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(page), head, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), label("How it installs", "section", FALSE), FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(page), label("It becomes part of your system, like everything in SnapOS: Carapace adds it to "
                                            "/etc/snapos and builds the next system. Removing it, or going back with snapos "
                                            "rollback, always works.", "dim", TRUE), FALSE, FALSE, 0);
    ui.page_cmd = label("", "cmd", FALSE);
    gtk_box_pack_start(GTK_BOX(page), ui.page_cmd, FALSE, FALSE, 0);
    ui.page_more = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start(GTK_BOX(page), ui.page_more, FALSE, FALSE, 8);
    return scrolled(page);
}

static gboolean test_start(gpointer data) {
    (void)data;
    const char *q = g_getenv("CARAPACE_TEST_SEARCH");
    if (q && *q) gtk_entry_set_text(GTK_ENTRY(ui.search), q);
    const char *cat = g_getenv("CARAPACE_TEST_CATEGORY");
    if (cat && *cat) show_category(cat);
    const char *page = g_getenv("CARAPACE_TEST_APP");
    if (page && *page) { App *a = find_app(page); if (a) show_app(a); }
    const char *attr = g_getenv("CARAPACE_TEST_INSTALL");
    if (attr && *attr) {
        App *a = find_app(attr);
        if (!a) { a = g_new0(App, 1); a->attr = g_strdup(attr); a->name = g_strdup(attr); a->icon = g_strdup(attr); a->cat = ""; a->summary = g_strdup(""); a->version = g_strdup(""); }
        show_app(a);
        install(a, !is_installed(a->attr));
    }
    return G_SOURCE_REMOVE;
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    apps = read_tsv("apps.tsv", 5, &napps);
    index_ = read_tsv("index.tsv", 3, &nindex);
    load_installed();

    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    ui.win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GdkRectangle work = { 0, 0, 1280, 800 };
    GdkMonitor *mon = gdk_display_get_primary_monitor(gdk_display_get_default());
    if (!mon) mon = gdk_display_get_monitor(gdk_display_get_default(), 0);
    if (mon) gdk_monitor_get_workarea(mon, &work);
    int ww = MIN(1180, work.width - 20), wh = MIN(760, work.height - 20);
    const char *size = g_getenv("CARAPACE_TEST_SIZE");
    if (size && *size) sscanf(size, "%dx%d", &ww, &wh);
    gtk_window_set_default_size(GTK_WINDOW(ui.win), ww, wh);
    gtk_window_set_icon_name(GTK_WINDOW(ui.win), "carapace");
    g_signal_connect(ui.win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *hb = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(hb), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(hb), "Carapace");
    ui.search = gtk_search_entry_new();
    char *ph = g_strdup_printf("Search %d programs", nindex ? nindex : napps);
    gtk_entry_set_placeholder_text(GTK_ENTRY(ui.search), ph);
    g_free(ph);
    gtk_widget_set_size_request(ui.search, 440, -1);
    g_signal_connect(ui.search, "changed", G_CALLBACK(on_search), NULL);
    gtk_header_bar_set_custom_title(GTK_HEADER_BAR(hb), ui.search);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(hb), icon("carapace", 26));
    gtk_window_set_titlebar(GTK_WINDOW(ui.win), hb);

    GtkWidget *body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    ui.side = gtk_list_box_new();
    add_class(ui.side, "side");
    gtk_widget_set_size_request(ui.side, 210, -1);
    gtk_list_box_insert(GTK_LIST_BOX(ui.side), side_row("Explore", ""), -1);
    gtk_list_box_insert(GTK_LIST_BOX(ui.side), side_row("Installed", "@installed"), -1);
    GtkWidget *h = gtk_list_box_row_new();
    gtk_container_add(GTK_CONTAINER(h), label("CATEGORIES", "heading", FALSE));
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(h), FALSE);
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(h), FALSE);
    gtk_list_box_insert(GTK_LIST_BOX(ui.side), h, -1);
    for (int i = 0; CATS[i]; i++) gtk_list_box_insert(GTK_LIST_BOX(ui.side), side_row(CATS[i], CATS[i]), -1);
    g_signal_connect(ui.side, "row-activated", G_CALLBACK(on_side), NULL);
    gtk_box_pack_start(GTK_BOX(body), ui.side, FALSE, FALSE, 0);

    ui.stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(ui.stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);

    ui.explore = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ui.explore), 24);
    gtk_stack_add_named(GTK_STACK(ui.stack), scrolled(ui.explore), "explore");

    GtkWidget *cat = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(cat), 24);
    ui.cat_title = label("", "title", FALSE);
    gtk_box_pack_start(GTK_BOX(cat), ui.cat_title, FALSE, FALSE, 0);
    ui.cat_grid = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(ui.cat_grid), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(ui.cat_grid), 1);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(ui.cat_grid), 4);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(ui.cat_grid), 10);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(ui.cat_grid), 10);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(ui.cat_grid), TRUE);
    gtk_widget_set_valign(ui.cat_grid, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(cat), ui.cat_grid, FALSE, FALSE, 0);
    gtk_stack_add_named(GTK_STACK(ui.stack), scrolled(cat), "category");

    GtkWidget *rbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(rbox), 24);
    ui.results_title = label("", "title", FALSE);
    gtk_box_pack_start(GTK_BOX(rbox), ui.results_title, FALSE, FALSE, 0);
    ui.results = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_pack_start(GTK_BOX(rbox), ui.results, FALSE, FALSE, 0);
    gtk_stack_add_named(GTK_STACK(ui.stack), scrolled(rbox), "results");

    gtk_stack_add_named(GTK_STACK(ui.stack), build_page(), "app");

    gtk_box_pack_start(GTK_BOX(body), ui.stack, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(ui.win), body);
    gtk_widget_show_all(ui.win);
    show_explore();
    gtk_list_box_select_row(GTK_LIST_BOX(ui.side), gtk_list_box_get_row_at_index(GTK_LIST_BOX(ui.side), 0));
    g_timeout_add_seconds(7, next_hero, NULL);
    if (g_getenv("CARAPACE_TEST_INSTALL") || g_getenv("CARAPACE_TEST_SEARCH") || g_getenv("CARAPACE_TEST_CATEGORY") || g_getenv("CARAPACE_TEST_APP"))
        g_timeout_add(800, test_start, NULL);
    gtk_main();
    return 0;
}
