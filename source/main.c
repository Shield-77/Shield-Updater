/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Shield-77.
 */
#include <3ds.h>
#include <3ds/services/mcuhwc.h>
#include <curl/curl.h>
#include <jansson.h>
#include <mbedtls/sha256.h>
#include "music.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef SHIELD_UPDATER_CIA_VERSION
#error SHIELD_UPDATER_CIA_VERSION must be supplied by the Makefile
#endif

// Each entry has a fixed repository, asset name and title ID. More Shield
// applications can be added when their release links are known.
typedef struct {
    const char *name;
    const char *description;
    const char *api_url;
    const char *asset_name;
    bool asset_prefix_match;
    const char *url_prefix;
    const char *file_name;
    u64 title_id;
} AppEntry;

static const AppEntry APPS[] = {
    {
        "Shield DJ Lite",
        "MP3/OGG, pitch, basse et reverb.",
        "https://api.github.com/repos/Shield-77/Shield-DJ-Lite/releases/latest",
        "ShieldDJLite.cia",
        false,
        "https://github.com/Shield-77/Shield-DJ-Lite/releases/download/",
        "ShieldDJLite",
        UINT64_C(0x000400000A210100),
    },
    {
        "Shield DJ Standard",
        "Pochettes, vinyle et effets.",
        "https://api.github.com/repos/Shield-77/Shield-DJ-standard-/releases/latest",
        "ShieldDJStandard.cia",
        false,
        "https://github.com/Shield-77/Shield-DJ-standard-/releases/download/",
        "ShieldDJStandard",
        UINT64_C(0x000400000A210300),
    },
    {
        "Shield MP4 Player",
        "Videos MP4 jusqu'a 144p.",
        "https://api.github.com/repos/Shield-77/Shield-MP4-Player/releases/latest",
        "MP4Player-Old3DS-",
        true,
        "https://github.com/Shield-77/Shield-MP4-Player/releases/download/",
        "ShieldMP4Player",
        UINT64_C(0x000400000BF75100),
    },
    {
        "Shield Photo",
        "Photos JPG/PNG, zoom et 3D.",
        "https://api.github.com/repos/Shield-77/Shield-Lecteur-d-images-2d-3d/releases/latest",
        "ShieldPhoto-",
        true,
        "https://github.com/Shield-77/Shield-Lecteur-d-images-2d-3d/releases/download/",
        "ShieldPhoto",
        UINT64_C(0x000400000A210200),
    },
    {
        "Shield Online Art [EXPERIMENTAL]",
        "Galerie en ligne 2D. Usage experimental.",
        "https://api.github.com/repos/Shield-77/Shield-Online-Art/releases/latest",
        "ShieldOnlineArt.cia",
        false,
        "https://github.com/Shield-77/Shield-Online-Art/releases/download/",
        "ShieldOnlineArt",
        UINT64_C(0x000400000A210500),
    },
    {
        "Shield MP4 Converter",
        "Convertit les videos en 144p.",
        "https://api.github.com/repos/Shield-77/Shield-Optimised-MP4-Converter-/releases/latest",
        "ShieldOptimisedConverter.cia",
        false,
        "https://github.com/Shield-77/Shield-Optimised-MP4-Converter-/releases/download/",
        "ShieldOptimisedConverter",
        UINT64_C(0x000400000BF75200),
    },
    {
        "Shield MP3 Downloader",
        "Telecharge et convertit en MP3.",
        "https://api.github.com/repos/Shield-77/Shield-MP3-downloader/releases/latest",
        "MP3Downloader_dev.cia",
        false,
        "https://github.com/Shield-77/Shield-MP3-downloader/releases/download/",
        "ShieldMP3Downloader",
        UINT64_C(0x000400000BF74E00),
    },
    {
        "Shield Radio",
        "Six radios francaises en Wi-Fi.",
        "https://api.github.com/repos/Shield-77/Shield-Radio-3DS/releases/latest",
        "shield-radio.cia",
        false,
        "https://github.com/Shield-77/Shield-Radio-3DS/releases/download/",
        "ShieldRadio",
        UINT64_C(0x000400000F643100),
    },
    {
        "Shield Theme Music",
        "Extrait et ecoute les musiques OGG des themes.",
        "https://api.github.com/repos/Shield-77/Shield-Theme-Music-Extractor/releases/latest",
        "ShieldThemeMusicExtractor.cia",
        false,
        "https://github.com/Shield-77/Shield-Theme-Music-Extractor/releases/download/",
        "ShieldThemeMusicExtractor",
        UINT64_C(0x000400000BF75300),
    },
    {
        "Shield Checker",
        "Verifie Luma3DS et GodMode9 sans modifier la console.",
        "https://api.github.com/repos/Shield-77/Shield-Checker/releases/latest",
        "ShieldChecker.cia",
        false,
        "https://github.com/Shield-77/Shield-Checker/releases/download/",
        "ShieldChecker",
        UINT64_C(0x000400000A210600),
    },
    {
        "Shield Theme Maker",
        "Cree des themes directement sur 3DS.",
        "https://api.github.com/repos/Shield-77/Shield-Theme-Maker/releases/latest",
        "shield_theme_maker.cia",
        false,
        "https://github.com/Shield-77/Shield-Theme-Maker/releases/download/",
        "ShieldThemeMaker",
        UINT64_C(0x000400000F77A100),
    },
    {
        "Shield Updater",
        "Mises a jour des apps Shield.",
        "https://api.github.com/repos/Shield-77/Shield-Updater/releases/latest",
        "ShieldUpdater.cia",
        false,
        "https://github.com/Shield-77/Shield-Updater/releases/download/",
        "ShieldUpdater",
        UINT64_C(0x000400000A210400),
    },
};

#define APP_COUNT (sizeof(APPS) / sizeof(APPS[0]))
#define SELF_INDEX (APP_COUNT - 1)
#define VISIBLE_APPS 5
#define ICON_BYTES (48 * 48 * 3)
#define API_LIMIT (512 * 1024)
#define MAX_CIA_SIZE (32 * 1024 * 1024)
#define CIA_IO_BUFFER_SIZE (32 * 1024)
#define DOWNLOAD_ATTEMPTS 2
#define CA_PATH "romfs:/ca-bundle.pem"
#define INSTALL_LOG_PATH "sdmc:/3ds/ShieldUpdater/install.log"
#define LANGUAGE_CONFIG_PATH "sdmc:/3ds/ShieldUpdater/ui-language.dat"
#define FIRST_RUN_GUIDE_PATH "sdmc:/3ds/ShieldUpdater/guide-seen.dat"
#define USER_OPTIONS_PATH "sdmc:/3ds/ShieldUpdater/options-v2.dat"
#define VERSION_CACHE_PATH "sdmc:/3ds/ShieldUpdater/version-cache-v7.txt"
#define CIA_HEADER_BYTES (32 * 1024)

typedef struct {
    char tag[32];
    char url[512];
    char digest[65];
    size_t size;
    bool valid;
    bool checked;
    bool downloaded;
    bool local_error;
    bool installed;
    u16 installed_cia_version;
    u16 cia_version;
} ReleaseInfo;

typedef struct {
    char release_digest[65];
    u16 release_version;
    char installed_digest[65];
    u16 installed_version;
} VersionCacheEntry;

typedef struct {
    char *data;
    size_t used;
    size_t capacity;
} MemorySink;

typedef struct {
    FILE *file;
    size_t written;
    size_t limit;
    bool failed;
} FileSink;

static char message[128] = "";
static ReleaseInfo releases[APP_COUNT];
static VersionCacheEntry version_cache[APP_COUNT];
static size_t selected;
static size_t list_offset;
static bool install_confirm;
static bool reinstall_prompt;
static bool boot_loading;
static bool bottom_console_initialized;
static PrintConsole *bottom_console;
static u16 *bottom_text_buffer;
static size_t boot_scan_index;
#define release_info (releases[selected])
static char download_path[128];
static char cia_path[128];
static char cia_fs_path[128];
static bool service_am;
static bool service_soc;
static bool service_romfs;
static CURL *network_curl;
static u32 *socket_buffer;
static Result soc_error;
static u8 *top_background;
static u8 *boot_bottom;
static u8 *updater_icon;
static u8 *font_atlas;
static u8 *app_icons;
static char *ca_bundle;
static size_t ca_bundle_size;
static u64 last_progress_update;
static size_t transfer_progress_total;
static bool busy;
static char busy_phase[24];
static unsigned busy_percent;
static bool busy_has_percent;
static u64 busy_started;
static bool transfer_cancelled;
static bool common_network_failure;
static bool network_local_failure;
static bool non_network_error_hint;
static bool self_updated;
static bool options_active;
static bool setup_active;
static bool download_only = true;
static bool self_update_prompt;
static bool option_save_failed;
static bool ui_french;
static bool language_confirmation_needed;
static bool language_prompt_active;
static bool guide_active;
static unsigned guide_page;
static bool startup_checks_done;
static bool service_mcu_hwc;

typedef struct {
    const char *french;
    const char *english;
} UiTranslation;

static const UiTranslation UI_TRANSLATIONS[] = {
    {"Shield DJ Lite", "Shield DJ Lite"},
    {"MP3/OGG, pitch, basse et reverb.", "MP3/OGG, pitch, bass and reverb."},
    {"Pochettes, vinyle et effets.", "Album art, vinyl and effects."},
    {"Videos MP4 jusqu'a 144p.", "MP4 videos up to 144p."},
    {"Photos JPG/PNG, zoom et 3D.", "JPG/PNG photos, zoom and 3D."},
    {"Galerie en ligne 2D. Usage experimental.", "Online 2D gallery. Experimental."},
    {"Convertit les videos en 144p.", "Converts videos to 144p."},
    {"Telecharge et convertit en MP3.", "Downloads and converts MP3 audio."},
    {"Six radios francaises en Wi-Fi.", "Six French radio stations over Wi-Fi."},
    {"Extrait et ecoute les musiques OGG des themes.", "Extracts and plays theme OGG music."},
    {"Verifie Luma3DS et GodMode9 sans modifier la console.", "Checks Luma3DS and GodMode9 without changing the system."},
    {"Mises a jour des apps Shield.", "Updates Shield apps."},
    {"Reseau : memoire SOC insuffisante.", "Network: not enough SOC memory."},
    {"Demarrage reseau", "Starting network"},
    {"Initialisation HTTPS impossible.", "HTTPS initialization failed."},
    {"INSTALLATION A CONFIRMER", "CONFIRM INSTALLATION"},
    {"REINSTALLATION A CONFIRMER", "CONFIRM REINSTALLATION"},
    {"VERIFICATION EN COURS", "CHECKING"},
    {"STATUT INDISPONIBLE", "STATUS UNAVAILABLE"},
    {"NON INSTALLE", "NOT INSTALLED"},
    {"VERSION A VERIFIER", "VERSION UNKNOWN"},
    {"RELEASE MODIFIEE", "RELEASE CHANGED"},
    {"A JOUR", "UP TO DATE"},
    {"MISE A JOUR DISPONIBLE", "UPDATE AVAILABLE"},
    {"VEUILLEZ PATIENTER", "PLEASE WAIT"},
    {"INSTALLER ?", "INSTALL?"},
    {"REINSTALLER ?", "REINSTALL?"},
    {"A : OUI   B : NON", "A: YES   B: NO"},
    {"Reseau indisponible. X : reessayer.", "Network unavailable. X: retry."},
    {"Memoire insuffisante.", "Not enough memory."},
    {"Erreur libcurl.", "Network library error."},
    {"Lecture de la release GitHub...", "Reading GitHub release..."},
    {"GitHub HTTP 403. X : reessayer plus tard.", "GitHub HTTP 403. X: retry later."},
    {"GitHub HTTP 429. X : reessayer plus tard.", "GitHub HTTP 429. X: retry later."},
    {"Metadonnees GitHub invalides.", "Invalid GitHub metadata."},
    {"CIA ou empreinte absente.", "CIA or checksum missing."},
    {"Version CIA reconnue.", "CIA version recognized."},
    {"Impossible d'ecrire sur la SD.", "Could not write to the SD card."},
    {"Telechargement annule. Aucune installation lancee.", "Download cancelled. Nothing was installed."},
    {"Erreur d'ecriture sur la SD.", "SD card write error."},
    {"Reseau interrompu. Nouvelle tentative securisee...", "Network interrupted. Retrying safely..."},
    {"Reseau instable. Rien n'a ete installe. X : reessayer.", "Unstable network. Nothing installed. X: retry."},
    {"Telechargement incomplet. Rien n'a ete installe.", "Incomplete download. Nothing installed."},
    {"Verification SHA-256...", "Checking SHA-256..."},
    {"Verification annulee.", "Verification cancelled."},
    {"Empreinte SHA-256 incorrecte.", "SHA-256 checksum mismatch."},
    {"Impossible de finaliser le CIA.", "Could not finalize the CIA."},
    {"Lecture CIA impossible.", "Could not read the CIA."},
    {"CIA invalide pour la console.", "CIA is not valid for this system."},
    {"Identifiant CIA inattendu !", "Unexpected CIA title ID!"},
    {"CIA telecharge introuvable.", "Downloaded CIA not found."},
    {"Memoire insuffisante pour installer.", "Not enough memory to install."},
    {"Finalisation du CIA...", "Finalizing the CIA..."},
    {"Installation terminee !", "Installation complete!"},
    {"Installation annulee.", "Installation cancelled."},
    {"App absente. A pour installer.", "App not installed. Press A to install."},
    {"Version inconnue. A pour verifier.", "Version unknown. Press A to check."},
    {"Mise a jour dispo. A pour installer.", "Update available. Press A to install."},
    {"Release modifiee. A pour reinstaller.", "Release changed. Press A to reinstall."},
    {"Application a jour.", "App is up to date."},
    {"Service d'installation absent.", "Install service unavailable."},
    {"Version installee plus recente que GitHub.", "Installed version is newer than GitHub."},
    {"Updater local plus recent que GitHub.", "Local Updater is newer than GitHub."},
    {"MAJ terminee. Quitter avec START, puis relancer dans HOME.", "Update complete. Press START to quit, then relaunch from HOME."},
    {"Pas de reseau. X : verifier a nouveau", "No network. X: check again"},
    {"Shield Updater est deja a jour.", "Shield Updater is already up to date."},
    {"Nouvelle version : installation...", "New version: installing..."},
    {"Statut indisponible. X : reverifier.", "Status unavailable. X: check again."},
    {"Fond d'ecran indisponible.", "Background unavailable."},
    {"Certificat CA introuvable.", "CA certificate not found."},
    {"Service AM indisponible.", "AM service unavailable."},
    {"Scan termine. A pour installer.", "Scan complete. Press A to install."},
    {"Scan termine. X : reverifier.", "Scan complete. X: check again."},
    {"Reinstallation annulee.", "Reinstallation cancelled."},
    {"Recherche de la release...", "Checking for the latest release..."},
    {"Page de release", "Release page"},
    {"Lecture des fichiers de release...", "Reading release assets..."},
    {"Fichiers GitHub", "GitHub assets"},
    {"Version CIA", "CIA version"},
    {"Telechargement", "Downloading"},
    {"Verification", "Verifying"},
    {"Connexion GitHub", "Connecting to GitHub"},
    {"Installation", "Installing"},
    {"Finalisation", "Finalizing"},
    {"A : chercher la derniere release", "A: check for the latest release"},
    {"Installee : CIA %u", "Installed: CIA %u"},
    {"Installee : non detectee", "Installed: not detected"},
    {"GitHub : %.24s (CIA %u)", "GitHub: %.24s (CIA %u)"},
    {"GitHub : a verifier", "GitHub: not checked"},
    {"Contacter Shield77 si cela persiste.", "Contact Shield77 if this continues."},
    {"HAUT/BAS : choisir l'application", "UP/DOWN: choose an app"},
    {"A : installer / reinstaller", "A: install / repair"},
    {"X : reverifier", "X: check again"},
    {"B : annuler", "B: cancel"},
    {"START : quitter", "START: quit"},
    {"      MISE A JOUR TERMINEE !", "      UPDATE COMPLETE!"},
    {"Pour utiliser la nouvelle version :", "To use the new version:"},
    {"1. Appuie sur START pour quitter.", "1. Press START to quit."},
    {"2. Relance Shield Updater depuis", "2. Relaunch Shield Updater from"},
    {"   le menu HOME.", "   the HOME Menu."},
    {"L'application ne redemarre pas seule.", "The app will not restart itself."},
    {"START : quitter maintenant", "START: quit now"},
    {"START : quitter puis relancer", "START: quit and relaunch"},
    {"Connexion annulee.", "Connection cancelled."},
    {"CIA %u valide. Installation...", "CIA %u verified. Installing..."},
    {"AM demarrage : 0x%08lX", "AM start: 0x%08lX"},
    {"AM finalisation : 0x%08lX", "AM finish: 0x%08lX"},
    {"Installation : %s (voir install.log)", "Installation: %s (see install.log)"},
    {"Reinstaller %s ? A : oui  B : non", "Repair %s? A: yes  B: no"},
    {"Installer %s ? A : oui  B : non", "Install %s? A: yes  B: no"},
    {"      MISE A JOUR TERMINEE !", "      UPDATE COMPLETE!"},
    {"B : NON, PASSER EN ENGLISH", "B: NO, SWITCH TO ENGLISH"},
    {"SEULS FRANCAIS ET ENGLISH SONT DISPONIBLES.", "ONLY FRENCH AND ENGLISH ARE AVAILABLE."},
    {"CONFIRMER FRANCAIS ? A : OUI", "CONFIRM ENGLISH? A: YES"},
    {"B : NON, CHOISIR ENGLISH", "B: NO, SWITCH TO FRENCH"},
    {"START : QUITTER", "START: QUIT"},
    {"Reseau : memoire SOC insuffisante.", "Network: not enough SOC memory."},
    {"Initialisation HTTPS impossible.", "Could not initialize HTTPS."},
    {"Certificat CA incompatible.", "CA certificate is incompatible."},
    {"SOC 0x%08lX ; X : reessayer", "SOC 0x%08lX; X: retry"},
    {"SOC 0x%08lX ; X : retry", "SOC 0x%08lX; X: retry"},
    {"Statut indisponible. X : reverifier.", "Status unavailable. X: check again."},
    {"App absente. A pour installer.", "App not installed. Press A to install."},
    {"Version inconnue. A pour verifier.", "Version unknown. Press A to check."},
    {"Mise a jour dispo. A pour installer.", "Update available. Press A to install."},
    {"Application a jour. A : reinstaller.", "App is up to date. A: repair."},
    {"Installation terminee !", "Installation complete!"},
    {"Scan termine. A pour installer.", "Scan complete. Press A to install."},
    {"Scan termine. X : reverifier.", "Scan complete. X: check again."},
    {"Nouvelle version : installation...", "New version: installing..."},
    {"MAJ terminee. Quitter avec START, puis relancer dans HOME.", "Update complete. Press START to quit, then relaunch from HOME."},
    {"Pas de reseau. X : verifier a nouveau", "No network. X: check again"},
    {"Shield Updater est deja a jour.", "Shield Updater is up to date."},
    {"Version CIA reconnue.", "CIA version recognized."},
    {"Version CIA non reconnue.", "CIA version not recognized."},
    {"Reseau indisponible. X : reessayer.", "Network unavailable. X: retry."},
    {"Reseau interrompu. Nouvelle tentative securisee...", "Network interrupted. Retrying safely..."},
    {"Reseau instable. Rien n'a ete installe. X : reessayer.", "Unstable network. Nothing installed. X: retry."},
    {"Identifiant CIA inattendu !", "Unexpected CIA title ID!"},
    {"Connexion annulee.", "Connection cancelled."},
    {"Installation annulee.", "Installation cancelled."},
    {"Reinstallation annulee.", "Repair cancelled."},
    {"CIA %u valide. Installation...", "CIA %u verified. Installing..."},
    {"AM demarrage : 0x%08lX", "AM start: 0x%08lX"},
    {"AM finalisation : 0x%08lX", "AM finish: 0x%08lX"},
    {"Installation : %s (voir install.log)", "Installation: %s (see install.log)"},
    {"Installer %s ? A : oui  B : non", "Install %s? A: yes  B: no"},
    {"Reinstaller %s ? A : oui  B : non", "Repair %s? A: yes  B: no"},
    {"Langue changee : Francais.", "Language changed: French."},
    {"Langue changee : English.", "Language changed: English."},
    {"Langue changee mais non sauvegardee.", "Language changed but could not be saved."},
    {"Y : changer la langue (FR/EN)", "Y: change language (EN/FR)"},
    {"A : continuer", "A: continue"},
    {"A : j'ai compris", "A: I understand"},
    {"B : page precedente", "B: previous page"},
    {"TELECHARGEMENT A CONFIRMER", "CONFIRM DOWNLOAD"},
    {"App absente. A pour telecharger.", "App not installed. A: download CIA."},
    {"MAJ dispo. A pour telecharger.", "Update available. A: download CIA."},
    {"Release modifiee. A : telecharger.", "Release changed. A: download CIA."},
    {"Application a jour. A : telecharger.", "Up to date. A: download CIA."},
    {"Telechargement annule.", "Download cancelled."},
    {"Verification CIA indisponible.", "CIA verification unavailable."},
    {"Options non sauvegardees sur la SD.", "Could not save options to SD."},
    {"Scan termine. A pour choisir.", "Scan done. Press A to choose."},
    {"Dossier Shield_Download impossible a creer.", "Cannot create Shield_Download folder."},
};

static const char *ui_text(const char *text) {
    if (!text || ui_french) return text;
    for (size_t i = 0; i < sizeof(UI_TRANSLATIONS) / sizeof(UI_TRANSLATIONS[0]); ++i)
        if (strcmp(text, UI_TRANSLATIONS[i].french) == 0)
            return UI_TRANSLATIONS[i].english;
    return text;
}

static void load_ui_language(void) {
    FILE *file = fopen(LANGUAGE_CONFIG_PATH, "rb");
    if (file) {
        int saved = fgetc(file);
        fclose(file);
        if (saved == 'F' || saved == 'E') {
            ui_french = saved == 'F';
            return;
        }
    }

    // Read only the language enum. No region, model, identifiers, or account data.
    ui_french = false;
    if (R_SUCCEEDED(cfguInit())) {
        u8 system_language = CFG_LANGUAGE_EN;
        if (R_SUCCEEDED(CFGU_GetSystemLanguage(&system_language)))
            ui_french = system_language == CFG_LANGUAGE_FR;
        cfguExit();
    }
    language_confirmation_needed = true;
}

static bool save_ui_language(void) {
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/ShieldUpdater", 0777);
    FILE *file = fopen(LANGUAGE_CONFIG_PATH, "wb");
    if (!file) return false;
    bool okay = fputc(ui_french ? 'F' : 'E', file) != EOF;
    if (fclose(file) != 0) okay = false;
    return okay;
}

static bool first_run_guide_seen(void) {
    FILE *file = fopen(FIRST_RUN_GUIDE_PATH, "rb");
    if (!file) return false;
    int seen = fgetc(file);
    fclose(file);
    return seen == '1';
}

static bool save_first_run_guide_seen(void) {
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/ShieldUpdater", 0777);
    FILE *file = fopen(FIRST_RUN_GUIDE_PATH, "wb");
    if (!file) return false;
    bool okay = fputc('1', file) != EOF;
    if (fclose(file) != 0) okay = false;
    return okay;
}

static bool load_user_options(void) {
    FILE *file = fopen(USER_OPTIONS_PATH, "rb");
    if (!file) return false;
    int version = fgetc(file);
    int only = fgetc(file);
    fclose(file);
    if (version != '2' || (only != '0' && only != '1')) return false;
    download_only = only == '1';
    return true;
}

static bool save_user_options(void) {
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/ShieldUpdater", 0777);
    FILE *file = fopen(USER_OPTIONS_PATH, "wb");
    if (!file) return false;
    bool okay = fputc('2', file) != EOF &&
        fputc(download_only ? '1' : '0', file) != EOF;
    if (fclose(file) != 0) okay = false;
    return okay;
}

// Use only the optional notification LED. Keep installation success/failure
// feedback independent from the power LED and skip it if MCUHWC is unavailable.
static void notify_install_result(bool succeeded) {
    if (!service_mcu_hwc) {
        if (R_FAILED(mcuHwcInit())) return;
        service_mcu_hwc = true;
    }

    InfoLedPattern pattern;
    memset(&pattern, 0, sizeof(pattern));
    pattern.delay = 1;       // 1/16 s per sample; the complete pattern lasts 2 s.
    pattern.smoothing = 1;
    pattern.loopDelay = 0xFF; // Play the pattern once.
    for (unsigned i = 0; i < 32; ++i) {
        bool lit = ((i / 4) & 1) == 0;
        if (!lit) continue;
        if (succeeded) pattern.greenPattern[i] = 0xFF;
        else {
            pattern.redPattern[i] = 0xFF;
            pattern.greenPattern[i] = 0x60;
        }
    }
    (void)MCUHWC_SetInfoLedPattern(&pattern);
}

static void refresh_ui(void);
static bool release_changed_same_version(void);

// HOME and lid-sleep are available while idle, but are deferred during an
// operation that must finish or be explicitly cancelled (network transfer,
// verification, or CIA installation). This does not intercept the POWER key.
static void set_busy_state(bool active) {
    busy = active;
    aptSetHomeAllowed(!active);
}

static void begin_busy(const char *phase, bool has_percent) {
    set_busy_state(true);
    busy_percent = 0;
    busy_has_percent = has_percent;
    busy_started = osGetTime();
    last_progress_update = 0;
    snprintf(busy_phase, sizeof(busy_phase), "%s", ui_text(phase));
    refresh_ui();
}

static void select_paths(void) {
    snprintf(download_path, sizeof(download_path), "sdmc:/3ds/ShieldUpdater/%s.cia.part", APPS[selected].file_name);
    if (download_only) {
        snprintf(cia_path, sizeof(cia_path), "sdmc:/Shield_Download/%s.cia", APPS[selected].file_name);
        snprintf(cia_fs_path, sizeof(cia_fs_path), "/Shield_Download/%s.cia", APPS[selected].file_name);
    } else {
        snprintf(cia_path, sizeof(cia_path), "sdmc:/3ds/ShieldUpdater/%s.cia", APPS[selected].file_name);
        snprintf(cia_fs_path, sizeof(cia_fs_path), "/3ds/ShieldUpdater/%s.cia", APPS[selected].file_name);
    }
}

static void set_message(const char *value) {
    snprintf(message, sizeof(message), "%s", ui_text(value));
    non_network_error_hint = false;
}

static void set_local_error(const char *value) {
    set_message(value);
    non_network_error_hint = true;
    if (selected < APP_COUNT) release_info.local_error = true;
}

// Kept outside install_cia's stack frame so a failed installation can be
// diagnosed from the SD card without changing the normal UI.
static void install_trace(const char *stage, Result result, u64 offset, u32 detail) {
    FILE *log = fopen(INSTALL_LOG_PATH, "a");
    if (!log) return;
    fprintf(log, "%llu %s %s result=0x%08lX offset=%llu detail=%lu\n",
        (unsigned long long)osGetTime(), APPS[selected].file_name, stage,
        (unsigned long)(u32)result, (unsigned long long)offset, (unsigned long)detail);
    fclose(log);
}

static bool init_network(void) {
    if (service_soc) return true;
    if (!socket_buffer) socket_buffer = memalign(0x1000, 0x100000);
    if (!socket_buffer) {
        network_local_failure = true;
        set_local_error("Reseau : memoire SOC insuffisante.");
        return false;
    }
    begin_busy("Demarrage reseau", false);
    soc_error = socInit(socket_buffer, 0x100000);
    if (soc_error != 0) {
        set_busy_state(false);
        common_network_failure = true;
        snprintf(message, sizeof(message), ui_french ? "SOC 0x%08lX ; X : reessayer" :
                 "SOC 0x%08lX; X: retry", (unsigned long)soc_error);
        return false;
    }
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        socExit();
        set_busy_state(false);
        common_network_failure = true;
        network_local_failure = true;
        set_local_error("Initialisation HTTPS impossible.");
        return false;
    }
    service_soc = true;
    common_network_failure = false;
    network_local_failure = false;
    set_busy_state(false);
    return true;
}

static bool installed_version(const AppEntry *app, u16 *version) {
    AM_TitleInfo info;
    u64 id = app->title_id;
    if (!service_am || R_FAILED(AM_GetTitleInfo(MEDIATYPE_SD, 1, &id, &info))) return false;
    *version = info.version;
    return true;
}

static void refresh_installed(size_t index) {
    u16 version = 0;
    releases[index].installed = installed_version(&APPS[index], &version);
    releases[index].installed_cia_version = releases[index].installed ? version : 0;
}

static u16 self_current_version(void) {
    u16 version = SHIELD_UPDATER_CIA_VERSION;
    if (releases[SELF_INDEX].installed &&
        releases[SELF_INDEX].installed_cia_version > version)
        version = releases[SELF_INDEX].installed_cia_version;
    return version;
}

static const char *selection_status(void) {
    if (install_confirm) return download_only ? "TELECHARGEMENT A CONFIRMER" :
        (reinstall_prompt ? "REINSTALLATION A CONFIRMER" : "INSTALLATION A CONFIRMER");
    if (!release_info.checked) return "VERIFICATION EN COURS";
    if (!release_info.valid) return "STATUT INDISPONIBLE";
    if (!release_info.installed) return "NON INSTALLE";
    if (!release_info.cia_version) return "VERSION A VERIFIER";
    if (release_changed_same_version()) return "RELEASE MODIFIEE";
    if (release_info.installed_cia_version >= release_info.cia_version) return "A JOUR";
    return "MISE A JOUR DISPONIBLE";
}

static void pixel(u8 *frame, int x, int y, u8 r, u8 g, u8 b) {
    if (x < 0 || y < 0 || x >= 400 || y >= 240) return;
    size_t at = ((size_t)x * 240 + (239 - y)) * 3;
    frame[at] = b;
    frame[at + 1] = g;
    frame[at + 2] = r;
}

static void top_text(u8 *frame, int x, int y, const char *text, u8 r, u8 g, u8 b) {
    if (!font_atlas) return;
    text = ui_text(text);
    for (int letter = 0; text[letter] && letter < 37; ++letter) {
        unsigned char ch = (unsigned char)text[letter];
        if (ch < 32 || ch > 126) ch = '?';
        const u8 *glyph = font_atlas + (ch - 32) * 160;
        for (int row = 0; row < 16; ++row)
            for (int col = 0; col < 10; ++col) {
                u8 alpha = glyph[row * 10 + col];
                int px = x + letter * 10 + col;
                int py = y + row;
                if (!alpha || px < 0 || px >= 400 || py < 0 || py >= 240) continue;
                size_t at = ((size_t)px * 240 + (239 - py)) * 3;
                frame[at] = (u8)((frame[at] * (255 - alpha) + b * alpha) / 255);
                frame[at + 1] = (u8)((frame[at + 1] * (255 - alpha) + g * alpha) / 255);
                frame[at + 2] = (u8)((frame[at + 2] * (255 - alpha) + r * alpha) / 255);
            }
    }
}

static void draw_top(void) {
    u8 *frame = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    if (!frame) return;
    if (top_background) memcpy(frame, top_background, 400 * 240 * 3);
    else memset(frame, 0, 400 * 240 * 3);
    if (boot_loading) {
        // Keep the Shield artwork visible through every GitHub version check.
        for (int y = 6; y < 62; ++y)
            for (int x = 6; x < 394; ++x)
                pixel(frame, x, y,
                      x == 6 || x == 393 || y == 6 || y == 61 ? 24 : 3,
                      x == 6 || x == 393 || y == 6 || y == 61 ? 211 : 17,
                      x == 6 || x == 393 || y == 6 || y == 61 ? 244 : 31);
        if (updater_icon) {
            for (int y = 0; y < 48; ++y)
                for (int x = 0; x < 48; ++x) {
                    const u8 *rgb = updater_icon + (y * 48 + x) * 3;
                    pixel(frame, 13 + x, 10 + y, rgb[0], rgb[1], rgb[2]);
                }
        }
        top_text(frame, 74, 11, "SHIELD UPDATER", 255, 255, 255);
        char scan_label[40];
        if (boot_scan_index)
            snprintf(scan_label, sizeof(scan_label), "VERIFICATION %u/%u",
                     (unsigned)boot_scan_index, (unsigned)APP_COUNT);
        else snprintf(scan_label, sizeof(scan_label), "DEMARRAGE");
        top_text(frame, 74, 32, scan_label, 35, 212, 241);
        int pulse = (int)((osGetTime() / 35) % 272);
        for (int x = 64; x < 336; ++x) {
            bool lit = (x - 64 - pulse + 272) % 272 < 32;
            pixel(frame, x, 224, lit ? 24 : 23,
                  lit ? 211 : 50, lit ? 244 : 70);
        }
        return;
    }
    if (language_prompt_active) {
        for (int y = 70; y < 174; ++y)
            for (int x = 30; x < 370; ++x)
                pixel(frame, x, y,
                      x == 30 || x == 369 || y == 70 || y == 173 ? 24 : 4,
                      x == 30 || x == 369 || y == 70 || y == 173 ? 211 : 23,
                      x == 30 || x == 369 || y == 70 || y == 173 ? 244 : 42);
        top_text(frame, 75, 82,
                 ui_french ? "LANGUE CHOISIE : FRANCAIS" : "SELECTED LANGUAGE: ENGLISH",
                 255, 255, 255);
        top_text(frame, 66, 101, ui_french ? "FRANCAIS ET ENGLISH UNIQUEMENT" : "FRENCH AND ENGLISH ONLY",
                 149, 229, 245);
        top_text(frame, 62, 120, ui_french ? "MON ANGLAIS N'EST PAS PARFAIT" : "MY ENGLISH ISNT PERFECT",
                 195, 223, 238);
        top_text(frame, 66, 139, ui_french ? "ERREURS DE TRADUCTION POSSIBLES" : "TRANSLATION ERRORS MAY REMAIN",
                 195, 223, 238);
        top_text(frame, 72, 158, ui_french ? "A: OUI / B: AUTRE LANGUE" : "A: YES / B: OTHER LANGUAGE",
                 255, 255, 255);
        return;
    }
    if (guide_active) {
        for (int y = 22; y < 224; ++y) {
            for (int x = 18; x < 382; ++x) {
                bool edge = x == 18 || x == 381 || y == 22 || y == 223;
                pixel(frame, x, y, edge ? 24 : 3,
                      edge ? 211 : 17, edge ? 244 : 32);
            }
        }
        static const char *titles_fr[4] = {
            "A QUOI SERT SHIELD UPDATER ?",
            "INSTALLER OU REPARER UNE APP",
            "COMPRENDRE UN STATUT INDISPONIBLE",
            "A PROPOS DU DEVELOPPEMENT",
        };
        static const char *titles_en[4] = {
            "WHAT DOES SHIELD UPDATER DO?",
            "INSTALL OR REPAIR AN APP",
            "ABOUT AN UNAVAILABLE STATUS",
            "ABOUT THIS PROJECT",
        };
        static const char *lines_fr[4][6] = {
            {"Verifie les versions des apps.",
              "MAJ d'Updater avec votre accord.",
             "Autres apps : accord requis.",
             "Le scan seul ne modifie rien.",
             "Choisis une app avec HAUT ou BAS.", ""},
             {"Choisis une app avec HAUT ou BAS.",
              "A ouvre une confirmation.",
             "Un second A confirme; B annule.",
              "CIA seul ou installation : ton choix.",
              "Le CIA est verifie avant action.", ""},
            {"Ce statut ne signifie pas un danger.",
             "Wi-Fi coupe ou connexion faible.",
             "GitHub ou lien de release indispo.",
             "Verifie Internet avant le demarrage.",
             "X relance la verification.", ""},
            {"Je suis un petit developpeur solo,",
             "plutot oriente hardware.",
             "Mon temps varie selon les periodes.",
             "Cette app est gratuite, faite sur",
             "mon temps libre. Mon anglais peut",
             "avoir des erreurs. Merci !"},
        };
        static const char *lines_en[4][6] = {
            {"Checks versions of Shield apps.",
              "Updater asks before updating.",
             "Other installs need your approval.",
             "Checks alone change nothing.",
             "Choose an app with UP or DOWN.", ""},
             {"Choose an app with UP or DOWN.",
              "A opens a confirmation.",
             "Press A again to confirm; B cancels.",
              "Download or install: your choice.",
              "The CIA is checked before action.", ""},
            {"This status does not mean danger.",
             "Wi-Fi may be off or unstable.",
             "GitHub or its link may be down.",
             "Check internet before app startup.",
             "X retries; nothing is forced.", ""},
            {"I am a small solo developer,",
             "mainly focused on hardware.",
             "My free time varies by period.",
             "I build it free in my spare time.",
             "My English may have mistakes.",
             "Thank you for your patience!"},
        };
        unsigned page = guide_page < 4 ? guide_page : 0;
        char page_label[16];
        snprintf(page_label, sizeof(page_label), "%u/4", page + 1);
        top_text(frame, 34, 34, ui_french ? titles_fr[page] : titles_en[page],
                 255, 255, 255);
        top_text(frame, 330, 34, page_label, 35, 212, 241);
        for (unsigned row = 0; row < 6; ++row) {
            const char *line = ui_french ? lines_fr[page][row] : lines_en[page][row];
            if (line[0]) top_text(frame, 31, 69 + (int)row * 21, line,
                                  row == 0 ? 149 : 205,
                                  row == 0 ? 229 : 225,
                                  row == 0 ? 245 : 238);
        }
        top_text(frame, 31, 202,
                 page == 3 ? ui_text("A : j'ai compris") : ui_text("A : continuer"),
                 35, 212, 241);
        return;
    }
    if (setup_active) {
        for (int y = 72; y < 170; ++y)
            for (int x = 24; x < 376; ++x)
                pixel(frame, x, y, 4, 23, 42);
        top_text(frame, 52, 84, ui_french ? "CHOISIS COMMENT GARDER LES CIA" :
                 "CHOOSE HOW TO HANDLE CIAS", 255, 255, 255);
        top_text(frame, 52, 111, ui_french ? "A : TELECHARGER SEULEMENT" :
                 "A: DOWNLOAD ONLY", 149, 229, 245);
        top_text(frame, 52, 137, ui_french ? "B : TELECHARGER ET INSTALLER" :
                 "B: DOWNLOAD AND INSTALL", 149, 229, 245);
        return;
    }
    // Compact information card; leave the main logo and the bottom artwork visible.
    for (int y = 7; y < 70; ++y) {
        for (int x = 7; x < 393; ++x) {
            size_t at = ((size_t)x * 240 + (239 - y)) * 3;
            frame[at] = (u8)(frame[at] / 5 + 15);
            frame[at + 1] = (u8)(frame[at + 1] / 5 + 8);
            frame[at + 2] = (u8)(frame[at + 2] / 5 + 2);
        }
    }
    for (int x = 7; x < 393; ++x) {
        pixel(frame, x, 7, 22, 195, 230);
        pixel(frame, x, 69, 22, 195, 230);
    }
    const u8 *icon = selected == SELF_INDEX ? updater_icon :
                     app_icons ? app_icons + selected * ICON_BYTES : NULL;
    if (icon) {
        for (int y = 0; y < 48; ++y)
            for (int x = 0; x < 48; ++x) {
                const u8 *rgb = icon + (y * 48 + x) * 3;
                pixel(frame, 15 + x, 14 + y, rgb[0], rgb[1], rgb[2]);
            }
    }
    top_text(frame, 73, 10, APPS[selected].name, 255, 255, 255);
    top_text(frame, 73, 28, selection_status(), 30, 215, 245);
    top_text(frame, 73, 47, APPS[selected].description, 195, 223, 238);
    if (busy) {
        for (int y = 95; y < 150; ++y)
            for (int x = 65; x < 335; ++x) {
                if (x == 65 || x == 334 || y == 95 || y == 149)
                    pixel(frame, x, y, 24, 211, 244);
                else pixel(frame, x, y, 4, 23, 42);
            }
        top_text(frame, 98, 101, "VEUILLEZ PATIENTER", 255, 255, 255);
        char progress[40];
        unsigned elapsed = (unsigned)((osGetTime() - busy_started) / 1000);
        if (busy_has_percent)
            snprintf(progress, sizeof(progress), "%s %u%% %us", busy_phase, busy_percent, elapsed);
        else
            snprintf(progress, sizeof(progress), "%s... %us", busy_phase, elapsed);
        top_text(frame, 94, 121, progress, 149, 229, 245);
        int pulse = (int)(((osGetTime() - busy_started) / 35) % 212);
        int fill = (int)(212 * busy_percent / 100);
        for (int x = 94; x < 306; ++x) {
            bool lit = busy_has_percent ? x < 94 + fill : (x - 94 - pulse + 212) % 212 < 24;
            pixel(frame, x, 142, lit ? 24 : 34, lit ? 211 : 55, lit ? 244 : 77);
        }
    }
    if (install_confirm) {
        for (int y = 87; y < 166; ++y)
            for (int x = 40; x < 360; ++x)
                pixel(frame, x, y,
                      x == 40 || x == 359 || y == 87 || y == 165 ? 24 : 4,
                      x == 40 || x == 359 || y == 87 || y == 165 ? 211 : 23,
                      x == 40 || x == 359 || y == 87 || y == 165 ? 244 : 42);
        top_text(frame, 111, 94, download_only ? "TELECHARGER ?" :
                 (reinstall_prompt ? "REINSTALLER ?" : "INSTALLER ?"), 255, 255, 255);
        top_text(frame, 55, 116, APPS[selected].name, 149, 229, 245);
        top_text(frame, 96, 140, "A : OUI   B : NON", 255, 255, 255);
    }
}

static void draw_bottom(void) {
    if (boot_loading) {
        u8 *frame = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
        if (frame) {
            if (bottom_console_initialized) {
                u16 *pixels = (u16 *)frame;
                for (size_t i = 0; i < 320 * 240; ++i) {
                    const u8 *src = boot_bottom ? boot_bottom + i * 3 : NULL;
                    u8 b = src ? src[0] : 0;
                    u8 g = src ? src[1] : 0;
                    u8 r = src ? src[2] : 0;
                    pixels[i] = (u16)(((u16)(r >> 3) << 11) |
                                      ((u16)(g >> 2) << 5) | (b >> 3));
                }
            } else if (boot_bottom) memcpy(frame, boot_bottom, 320 * 240 * 3);
            else memset(frame, 0, 320 * 240 * 3);
        }
        return;
    }
    // Render a complete RGB565 text page before copying it to the display.
    // If the offscreen buffer could not be allocated, still clear the live
    // framebuffer completely so an old page cannot remain behind SELECT.
    u8 *frame = bottom_text_buffer ? (u8 *)bottom_text_buffer :
        gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
    if (!frame) return;
    memset(frame, 0, 320 * 240 * sizeof(u16));
    printf("\x1b[1;1H\x1b[36mSHIELD UPDATER\x1b[0m\n");
    if (language_prompt_active) {
        printf("--------------------------------\n\n");
        if (ui_french) {
            printf("Francais et English uniquement.\n");
            printf("Mon anglais n'est pas parfait; des\n") ;
            printf("erreurs de traduction sont possibles.\n\n");
            printf("A : oui, garder Francais\n");
            printf("B : non, passer en English\n");
            printf("START : quitter\n");
        } else {
            printf("Only French and English are available.\n");
            printf("My English is not perfect; translation\n");
            printf("mistakes may remain.\n\n");
            printf("A: yes, keep English\n");
            printf("B: no, switch to French\n");
            printf("START: quit\n");
        }
        return;
    }
    if (guide_active) {
        printf("----------------------------------------\n\n");
        printf(ui_french ? "GUIDE DE PREMIERE UTILISATION\n" : "FIRST-USE GUIDE\n");
        printf(ui_french ? "Page %u/4\n\n" : "Page %u/4\n\n", guide_page + 1);
        if (guide_page == 0) {
            printf(ui_french ? "Shield Updater verifie les versions\n"
                             : "Shield Updater checks app versions\n");
            printf(ui_french ? "des apps Shield et explique leur statut.\n"
                             : "and explains their status simply.\n");
            printf(ui_french ? "La MAJ d'Updater est proposee.\n"
                             : "Updater asks before updating.\n");
            printf(ui_french ? "Les autres apps demandent votre accord.\n"
                             : "Other installs need your approval.\n");
            printf(ui_french ? "Le scan seul ne modifie rien.\n"
                             : "Checks alone change nothing.\n");
        } else if (guide_page == 1) {
            printf(ui_french ? "HAUT/BAS : choisir une application.\n"
                             : "UP/DOWN: choose an app.\n");
            printf(ui_french ? "A ouvre la demande; A confirme ensuite.\n"
                             : "A: show the prompt. A again: confirm.\n");
            printf(ui_french ? "B annule. Reinstaller peut reparer.\n"
                             : "B cancels. Repair reinstalls the app.\n");
            printf(ui_french ? "SELECT : options et mode CIA seul.\n"
                             : "SELECT: options and download only.\n");
        } else if (guide_page == 2) {
            printf(ui_french ? "Ce statut ne veut pas dire danger.\n"
                             : "This status does not mean danger.\n");
            printf(ui_french ? "Wi-Fi coupe/faible, GitHub indispo.\n"
                             : "Wi-Fi may be off/weak; GitHub may be down.\n");
            printf(ui_french ? "Un lien de release peut etre invalide.\n"
                             : "A release link can also be invalid.\n");
            printf(ui_french ? "Verifie Internet avant lancement; X\n"
                             : "Check your connection before launch; X\n");
            printf(ui_french ? "relance le controle. Rien n'est force.\n"
                             : "retries a check. Nothing is forced.\n");
        } else {
            printf(ui_french ? "Je suis un petit dev solo, surtout\n"
                             : "I am a small solo developer, focused\n");
            printf(ui_french ? "oriente hardware. Mon temps libre varie\n"
                             : "mainly on hardware. My free time varies\n");
            printf(ui_french ? "selon les periodes. Je fais cette app\n"
                             : "by period. I build this app for free\n");
            printf(ui_french ? "gratuitement, sur mon temps libre. Mon\n"
                             : "in my spare time. My English may have\n");
            printf(ui_french ? "anglais peut contenir des erreurs. Merci !\n"
                             : "mistakes. Thanks for your patience!\n");
        }
        printf("\n%s\n", ui_text(guide_page == 3 ? "A : j'ai compris" : "A : continuer"));
        if (guide_page > 0) printf("%s\n", ui_text("B : page precedente"));
        printf("%s\n", ui_text("START : quitter"));
        return;
    }
    if (setup_active) {
        printf("--------------------------------\n\n");
        printf(ui_french ? "CHOIX DES CIA\n\n" : "CIA PREFERENCE\n\n");
        printf(ui_french ? "Que faire apres le telechargement ?\n\n"
                         : "What happens after download?\n\n");
        printf(ui_french ? "A : garder le CIA sur la SD\n    dans /Shield_Download/\n\n"
                         : "A: save the CIA on SD\n   in /Shield_Download/\n\n");
        printf(ui_french ? "B : installer le CIA ici\n\n"
                         : "B: install the CIA here\n\n");
        printf(ui_french ? "Vous pourrez changer avec SELECT.\n"
                         : "Change this later with SELECT.\n");
        printf("\nSTART : quitter\n");
        if (option_save_failed) printf(ui_french ? "ERREUR : options non sauvegardees.\n" :
                                        "ERROR: options were not saved.\n");
        return;
    }
    if (options_active) {
        printf("--------------------------------\n\n");
        printf(ui_french ? "OPTIONS - SELECT pour fermer\n\n" : "OPTIONS - SELECT to close\n\n");
        printf(ui_french ? "Telecharger sans installer : %s\n\n" : "Download without install: %s\n\n",
               download_only ? "ON" : "OFF");
        printf(ui_french ? "ON : CIA dans /Shield_Download/\n"
                         : "ON: CIA in /Shield_Download/\n");
        printf(ui_french ? "OFF : installation apres accord\n"
                         : "OFF: install after approval\n");
        printf(ui_french ? "A : changer  B : retour\n"
                         : "A: change  B: back\n");
        printf("START : quitter\n");
        if (option_save_failed) printf(ui_french ? "ERREUR : options non sauvegardees.\n" :
                                        "ERROR: options were not saved.\n");
        return;
    }
    if (self_update_prompt) {
        printf("--------------------------------\n\n");
        printf(ui_french ? "MAJ SHIELD UPDATER\n\n" : "SHIELD UPDATER UPDATE\n\n");
        printf(ui_french ? "Version actuelle : CIA %u\n" : "Current version: CIA %u\n", self_current_version());
        printf(ui_french ? "Nouvelle version : %.24s\n" : "New version: %.24s\n", release_info.tag);
        if (release_info.cia_version)
            printf("CIA %u\n\n", release_info.cia_version);
        else printf(ui_french ? "CIA a verifier\n\n" : "CIA version to verify\n\n");
        printf(download_only ? (ui_french ? "Telecharger le CIA sans installer ?\n\nA : oui  B : plus tard\n" :
                                      "Download CIA without installing?\n\nA: yes  B: later\n") :
                            (ui_french ? "Voulez-vous la mettre a jour ?\n\nA : oui  B : plus tard\n" :
                                         "Update now?\n\nA: yes  B: later\n"));
        printf("START : quitter\n");
        return;
    }
    if (self_updated) {
        printf("--------------------------------\n\n");
        printf("%s\n\n", ui_text("      MISE A JOUR TERMINEE !"));
        printf("%s\n\n", ui_text("Pour utiliser la nouvelle version :"));
        printf("%s\n\n", ui_text("1. Appuie sur START pour quitter."));
        printf("%s\n", ui_text("2. Relance Shield Updater depuis"));
        printf("%s\n\n", ui_text("   le menu HOME."));
        printf("%s\n\n", ui_text("L'application ne redemarre pas seule."));
        printf("%s\n", ui_text("START : quitter maintenant"));
        return;
    }
    printf("--------------------------------\n\n");
    for (size_t row = 0; row < VISIBLE_APPS; ++row) {
        size_t i = list_offset + row;
        if (i < APP_COUNT) printf("%s %s\n\n", i == selected ? ">" : " ", APPS[i].name);
        else printf("\n\n");
    }
    printf("--------------------------------\n");
    if (release_info.installed)
        printf(ui_french ? "Installee : CIA %u\n" : "Installed: CIA %u\n",
               release_info.installed_cia_version);
    else
        printf("%s\n", ui_text("Installee : non detectee"));
    if (release_info.valid)
        printf(ui_french ? "GitHub : %.24s (CIA %u)\n" : "GitHub: %.24s (CIA %u)\n",
               release_info.tag, release_info.cia_version);
    else
        printf("%s\n", ui_text("GitHub : a verifier"));
    printf("\n--------------------------------\n");
    printf("%.39s\n", message);
    if (non_network_error_hint) printf("%s\n", ui_text("Contacter Shield77 si cela persiste."));
    else printf("\n");
    if (self_updated) {
        printf("START : quitter puis relancer\n");
        return;
    }
    printf("%s\n", ui_text("HAUT/BAS : choisir l'application"));
    printf("%s\n", download_only ? (ui_french ? "A : telecharger le CIA" : "A: download CIA") : ui_text("A : installer / reinstaller"));
    printf("%s\n", ui_text("X : reverifier"));
    printf("%s\n", ui_text("B : annuler"));
    printf("%s\n", ui_text("START : quitter"));
    if (startup_checks_done)
        printf("%s\n", ui_text("Y : changer la langue (FR/EN)"));
    if (startup_checks_done) printf("SELECT : options\n");
}

static void load_art(void) {
    FILE *f = fopen("romfs:/background.bgr", "rb");
    if (f) {
        top_background = malloc(400 * 240 * 3);
        if (top_background && fread(top_background, 1, 400 * 240 * 3, f) != 400 * 240 * 3) {
            free(top_background); top_background = NULL;
        }
        fclose(f);
    }
    f = fopen("romfs:/boot-bottom.bgr", "rb");
    if (f) {
        boot_bottom = malloc(320 * 240 * 3);
        if (boot_bottom && fread(boot_bottom, 1, 320 * 240 * 3, f) != 320 * 240 * 3) {
            free(boot_bottom); boot_bottom = NULL;
        }
        fclose(f);
    }
    f = fopen("romfs:/updater-icon.rgb", "rb");
    if (f) {
        updater_icon = malloc(48 * 48 * 3);
        if (updater_icon && fread(updater_icon, 1, 48 * 48 * 3, f) != 48 * 48 * 3) {
            free(updater_icon); updater_icon = NULL;
        }
        fclose(f);
    }
    f = fopen("romfs:/font.bin", "rb");
    if (f) {
        font_atlas = malloc(95 * 160);
        if (font_atlas && fread(font_atlas, 1, 95 * 160, f) != 95 * 160) {
            free(font_atlas); font_atlas = NULL;
        }
        fclose(f);
    }
    f = fopen("romfs:/app-icons.rgb", "rb");
    if (f) {
        app_icons = malloc((APP_COUNT - 1) * ICON_BYTES);
        if (app_icons && fread(app_icons, 1, (APP_COUNT - 1) * ICON_BYTES, f) != (APP_COUNT - 1) * ICON_BYTES) {
            free(app_icons); app_icons = NULL;
        }
        fclose(f);
    }
}

static void load_ca_bundle(void) {
    FILE *file = fopen(CA_PATH, "rb");
    if (!file) return;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return; }
    long length = ftell(file);
    if (length <= 0 || length > 512 * 1024 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return;
    }
    ca_bundle = malloc((size_t)length + 1);
    if (ca_bundle && fread(ca_bundle, 1, (size_t)length, file) == (size_t)length) {
        ca_bundle[(size_t)length] = 0;
        ca_bundle_size = (size_t)length;
    } else {
        free(ca_bundle);
        ca_bundle = NULL;
    }
    fclose(file);
}

static void refresh_ui(void) {
    for (int i = 0; i < 2; ++i) {
        draw_top();
        // consoleInit disables bottom double buffering; one complete redraw
        // is enough after it runs. The initial BGR8 splash still needs both.
        if (i == 0 || !bottom_console_initialized) {
            draw_bottom();
            if (!boot_loading && bottom_text_buffer) {
                u8 *frame = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
                if (frame) memcpy(frame, bottom_text_buffer, 320 * 240 * sizeof(u16));
            }
        }
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }
}

static bool confirm_ui_language(void) {
    if (!language_confirmation_needed) return true;
    language_prompt_active = true;
    refresh_ui();
    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();
        if (down & KEY_START) {
            language_prompt_active = false;
            return false;
        }
        if (down & KEY_A) {
            save_ui_language();
            language_confirmation_needed = false;
            language_prompt_active = false;
            refresh_ui();
            return true;
        }
        if (down & KEY_B) {
            ui_french = !ui_french;
            save_ui_language();
            language_confirmation_needed = false;
            language_prompt_active = false;
            refresh_ui();
            return true;
        }
        gspWaitForVBlank();
    }
    language_prompt_active = false;
    return false;
}

static bool show_first_use_guide(void) {
    guide_active = true;
    guide_page = 0;
    refresh_ui();
    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();
        if (down & KEY_START) {
            guide_active = false;
            return false;
        }
        if ((down & KEY_B) && guide_page > 0) {
            --guide_page;
            refresh_ui();
        } else if (down & KEY_A) {
            if (guide_page < 3) {
                ++guide_page;
                refresh_ui();
            } else {
                (void)save_first_run_guide_seen();
                guide_active = false;
                refresh_ui();
                return true;
            }
        }
        gspWaitForVBlank();
    }
    guide_active = false;
    return false;
}

static bool show_preferences_setup(void) {
    setup_active = true;
    refresh_ui();
    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();
        if (down & KEY_START) { setup_active = false; return false; }
        if (down & (KEY_A | KEY_B)) {
            download_only = (down & KEY_A) != 0;
            if (!save_user_options()) {
                option_save_failed = true;
                set_local_error("Options non sauvegardees sur la SD.");
            } else {
                option_save_failed = false;
                setup_active = false;
                select_paths();
                refresh_ui();
                return true;
            }
            refresh_ui();
        }
        gspWaitForVBlank();
    }
    setup_active = false;
    return false;
}

static void show_progress(const char *phase, size_t done, size_t total) {
    u64 now = osGetTime();
    if (now - last_progress_update < 250 && done < total) return;
    last_progress_update = now;
    char line[100];
    if (total)
        snprintf(line, sizeof(line), "%s : %u%%", ui_text(phase), (unsigned)((done * 100) / total));
    else
        snprintf(line, sizeof(line), "%s : %lu Ko", ui_text(phase), (unsigned long)(done / 1024));
    set_message(line);
    snprintf(busy_phase, sizeof(busy_phase), "%s", ui_text(phase));
    busy_percent = total ? (unsigned)((done * 100) / total) : 0;
    if (busy_percent > 100) busy_percent = 100;
    busy_has_percent = total != 0;
    busy = true;
    refresh_ui();
}

static int curl_progress(void *unused, curl_off_t download_total, curl_off_t downloaded,
                         curl_off_t upload_total, curl_off_t uploaded) {
    (void)unused;
    (void)upload_total;
    (void)uploaded;
    hidScanInput();
    if ((hidKeysDown() & KEY_B) || !aptMainLoop()) {
        transfer_cancelled = true;
        return 1;
    }
    u64 now = osGetTime();
    if (!release_info.size && download_total > 0 &&
        (u64)download_total <= MAX_CIA_SIZE)
        transfer_progress_total = (size_t)download_total;
    if (busy && now - last_progress_update >= 250) {
        size_t total = release_info.size ? release_info.size : transfer_progress_total;
        if (total && downloaded > 0) {
            busy_has_percent = true;
            unsigned percent = (unsigned)((u64)downloaded * 100 / total);
            busy_percent = percent > 100 ? 100 : percent;
        }
        last_progress_update = now;
        refresh_ui();
    }
    return 0;
}

static size_t memory_write(char *ptr, size_t size, size_t count, void *userdata) {
    MemorySink *sink = userdata;
    if (size && count > SIZE_MAX / size) return 0;
    size_t bytes = size * count;
    if (bytes > sink->capacity - sink->used) return 0;
    memcpy(sink->data + sink->used, ptr, bytes);
    sink->used += bytes;
    sink->data[sink->used] = 0;
    return bytes;
}

static size_t file_write(char *ptr, size_t size, size_t count, void *userdata) {
    FileSink *sink = userdata;
    hidScanInput();
    if ((hidKeysDown() & KEY_B) || !aptMainLoop()) { sink->failed = true; return 0; }
    if (size && count > SIZE_MAX / size) return 0;
    size_t bytes = size * count;
    if (bytes > sink->limit - sink->written) { sink->failed = true; return 0; }
    size_t written = fwrite(ptr, 1, bytes, sink->file);
    sink->written += written;
    if (written != bytes) sink->failed = true;
    size_t total = release_info.size ? release_info.size : transfer_progress_total;
    show_progress("Telechargement", sink->written, total);
    return written;
}

// Reuse one easy handle so libcurl can keep GitHub's HTTPS connections alive
// between the many small release metadata requests at startup.
static CURL *acquire_curl(void) {
    if (!network_curl) network_curl = curl_easy_init();
    else curl_easy_reset(network_curl);
    return network_curl;
}

static void release_curl(CURL *curl) {
    if (curl && curl == network_curl) curl_easy_reset(curl);
}

static bool retryable_download_error(CURLcode result, long status) {
    if (status >= 500 && status <= 599) return true;
    switch (result) {
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_RESOLVE_PROXY:
        case CURLE_COULDNT_CONNECT:
        case CURLE_OPERATION_TIMEDOUT:
        case CURLE_RECV_ERROR:
        case CURLE_SEND_ERROR:
        case CURLE_PARTIAL_FILE:
        case CURLE_GOT_NOTHING:
            return true;
        default:
            return false;
    }
}

static bool configure_curl(CURL *curl, const char *url) {
    if (!ca_bundle) { set_local_error("Certificat CA introuvable."); return false; }
#define SETOPT_OR_FAIL(option, value) \
    do { \
        if (curl_easy_setopt(curl, (option), (value)) != CURLE_OK) { \
            set_local_error("Initialisation HTTPS impossible."); \
            return false; \
        } \
    } while (0)
    if (!url || strncmp(url, "https://", 8) != 0) {
        set_local_error("Initialisation HTTPS impossible.");
        return false;
    }
    SETOPT_OR_FAIL(CURLOPT_URL, url);
    SETOPT_OR_FAIL(CURLOPT_USERAGENT, "ShieldUpdater/1.1");
    SETOPT_OR_FAIL(CURLOPT_FOLLOWLOCATION, 1L);
    SETOPT_OR_FAIL(CURLOPT_MAXREDIRS, 5L);
    SETOPT_OR_FAIL(CURLOPT_PROTOCOLS_STR, "https");
    SETOPT_OR_FAIL(CURLOPT_REDIR_PROTOCOLS_STR, "https");
    SETOPT_OR_FAIL(CURLOPT_SSL_VERIFYPEER, 1L);
    SETOPT_OR_FAIL(CURLOPT_SSL_VERIFYHOST, 2L);
    struct curl_blob ca = { .data = ca_bundle, .len = ca_bundle_size, .flags = CURL_BLOB_NOCOPY };
    if (curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &ca) != CURLE_OK) {
        set_local_error("Certificat CA incompatible.");
        return false;
    }
    SETOPT_OR_FAIL(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    SETOPT_OR_FAIL(CURLOPT_CONNECTTIMEOUT, 8L);
    SETOPT_OR_FAIL(CURLOPT_TIMEOUT, 30L);
    SETOPT_OR_FAIL(CURLOPT_NOSIGNAL, 1L);
    SETOPT_OR_FAIL(CURLOPT_NOPROGRESS, 0L);
    SETOPT_OR_FAIL(CURLOPT_XFERINFOFUNCTION, curl_progress);
#undef SETOPT_OR_FAIL
    return true;
}

static bool hex_digest(const char *text) {
    if (!text || strncmp(text, "sha256:", 7) != 0 || strlen(text + 7) != 64) return false;
    for (const char *p = text + 7; *p; ++p) if (!isxdigit((unsigned char)*p)) return false;
    return true;
}

static bool raw_digest(const char *text) {
    if (!text || strlen(text) != 64) return false;
    for (const char *p = text; *p; ++p)
        if (!isxdigit((unsigned char)*p)) return false;
    return true;
}

static void load_version_cache(void) {
    FILE *file = fopen(VERSION_CACHE_PATH, "rb");
    if (!file) return;
    unsigned index, version, installed_version;
    char digest[65], installed[65];
    while (fscanf(file, "%u %64s %u %64s %u", &index, digest, &version,
                  installed, &installed_version) == 5) {
        if (index >= APP_COUNT || !raw_digest(digest) || !version || version > 65535 ||
            installed_version > 65535 ||
            (strcmp(installed, "-") && !raw_digest(installed))) continue;
        VersionCacheEntry *entry = &version_cache[index];
        snprintf(entry->release_digest, sizeof(entry->release_digest), "%s", digest);
        entry->release_version = (u16)version;
        if (strcmp(installed, "-") && installed_version) {
            snprintf(entry->installed_digest, sizeof(entry->installed_digest), "%s", installed);
            entry->installed_version = (u16)installed_version;
        }
    }
    fclose(file);
}

static void save_version_cache(void) {
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/ShieldUpdater", 0777);
    FILE *file = fopen(VERSION_CACHE_PATH, "wb");
    if (!file) return;
    for (size_t i = 0; i < APP_COUNT; ++i) {
        VersionCacheEntry *entry = &version_cache[i];
        if (!raw_digest(entry->release_digest) || !entry->release_version) continue;
        fprintf(file, "%u %s %u %s %u\n", (unsigned)i, entry->release_digest,
                entry->release_version,
                raw_digest(entry->installed_digest) ? entry->installed_digest : "-",
                entry->installed_version);
    }
    fclose(file);
}

static void remember_release_version(size_t index, const char *digest, u16 version) {
    if (index >= APP_COUNT || !raw_digest(digest) || !version) return;
    VersionCacheEntry *entry = &version_cache[index];
    if (entry->release_version == version && !strcmp(entry->release_digest, digest)) return;
    snprintf(entry->release_digest, sizeof(entry->release_digest), "%s", digest);
    entry->release_version = version;
    save_version_cache();
}

static void remember_installed_release(size_t index, const char *digest, u16 version) {
    if (index >= APP_COUNT || !raw_digest(digest) || !version) return;
    VersionCacheEntry *entry = &version_cache[index];
    if (entry->installed_version == version && !strcmp(entry->installed_digest, digest)) return;
    snprintf(entry->installed_digest, sizeof(entry->installed_digest), "%s", digest);
    entry->installed_version = version;
    save_version_cache();
}

static bool release_changed_same_version(void) {
    const VersionCacheEntry *entry = &version_cache[selected];
    return release_info.valid && release_info.installed && release_info.cia_version &&
           release_info.installed_cia_version == release_info.cia_version &&
           entry->installed_version == release_info.installed_cia_version &&
           raw_digest(entry->installed_digest) &&
           strcmp(entry->installed_digest, release_info.digest) != 0;
}

static u32 read_le32(const u8 *p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static u32 read_be32(const u8 *p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

static u16 read_be16(const u8 *p) {
    return (u16)(((u16)p[0] << 8) | p[1]);
}

static bool cia_version_from_header(const u8 *data, size_t length,
                                    u64 expected_title_id, u16 *version) {
    if (length < 0x20) return false;
    u32 header = read_le32(data);
    u32 cert = read_le32(data + 8);
    u32 ticket = read_le32(data + 12);
    u32 tmd = read_le32(data + 16);
    if (header < 0x20 || header > CIA_HEADER_BYTES) return false;
    u64 tmd_at = (((u64)header + 63) & ~UINT64_C(63)) +
                 (((u64)cert + 63) & ~UINT64_C(63)) +
                 (((u64)ticket + 63) & ~UINT64_C(63));
    if (tmd_at > length || length - (size_t)tmd_at < 4) return false;
    u32 signature = read_be32(data + (size_t)tmd_at);
    size_t signed_prefix;
    if (signature == 0x10000 || signature == 0x10003) signed_prefix = 0x240;
    else if (signature == 0x10001 || signature == 0x10004) signed_prefix = 0x140;
    else if (signature == 0x10002 || signature == 0x10005) signed_prefix = 0x80;
    else return false;
    if (tmd < signed_prefix + 0xC4 || length - (size_t)tmd_at < signed_prefix + 0xA0)
        return false;
    const u8 *meta = data + (size_t)tmd_at + signed_prefix;
    u64 title_id = 0;
    for (int i = 0; i < 8; ++i) title_id = (title_id << 8) | meta[0x4C + i];
    if (title_id != expected_title_id) return false;
    *version = read_be16(meta + 0x9C);
    return *version != 0;
}

static bool fetch_remote_cia_version(const AppEntry *app, const char *url, u16 *version) {
    MemorySink sink = {0};
    sink.capacity = CIA_HEADER_BYTES;
    sink.data = malloc(CIA_HEADER_BYTES + 1);
    if (!sink.data) return false;
    CURL *curl = acquire_curl();
    if (!curl) { free(sink.data); return false; }
    if (!configure_curl(curl, url)) { release_curl(curl); free(sink.data); return false; }
    curl_easy_setopt(curl, CURLOPT_RANGE, "0-32767");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, memory_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 18L);
    transfer_cancelled = false;
    begin_busy("Version CIA", false);
    CURLcode result = curl_easy_perform(curl);
    set_busy_state(false);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    release_curl(curl);
    bool received = !transfer_cancelled && (status == 206 || status == 200) &&
                    (result == CURLE_OK || result == CURLE_WRITE_ERROR);
    bool valid = received &&
        cia_version_from_header((const u8 *)sink.data, sink.used, app->title_id, version);
    free(sink.data);
    return valid;
}

static bool fetch_memory_url(const char *url, MemorySink *sink, long *http_status,
                             char *effective_url, size_t effective_capacity) {
    CURL *curl = acquire_curl();
    if (!curl) return false;
    if (!configure_curl(curl, url)) { release_curl(curl); return false; }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, memory_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, sink);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 12L);
    transfer_cancelled = false;
    CURLcode result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, http_status);
    char *effective = NULL;
    if (effective_url && effective_capacity &&
        curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective) == CURLE_OK && effective)
        snprintf(effective_url, effective_capacity, "%s", effective);
    release_curl(curl);
    return result == CURLE_OK && *http_status == 200 && !transfer_cancelled;
}

static bool fetch_effective_url(const char *url, char *effective_url,
                                size_t effective_capacity) {
    CURL *curl = acquire_curl();
    if (!curl) return false;
    if (!configure_curl(curl, url)) { release_curl(curl); return false; }
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 12L);
    transfer_cancelled = false;
    CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    char *effective = NULL;
    bool copied = effective_capacity &&
        curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective) == CURLE_OK && effective;
    if (copied) snprintf(effective_url, effective_capacity, "%s", effective);
    release_curl(curl);
    return result == CURLE_OK && status == 200 && copied && !transfer_cancelled;
}

// The unauthenticated GitHub API is limited to 60 requests per source IP per
// hour. Its public release page and expanded asset list are served separately
// and include the exact asset digest. Use that page as a secure fallback when
// the API is rate-limited, preserving the same SHA-256 and CIA checks.
static bool fetch_release_page(const AppEntry *app, ReleaseInfo *next) {
    const char *repo_start = strstr(app->api_url, "api.github.com/repos/");
    const char *repo_end = repo_start ? strstr(repo_start, "/releases/latest") : NULL;
    if (!repo_start || !repo_end) return false;
    repo_start += strlen("api.github.com/repos/");
    size_t repo_length = (size_t)(repo_end - repo_start);
    if (!repo_length || repo_length >= 160) return false;

    char latest_url[256], effective_url[512], expanded_url[320];
    snprintf(latest_url, sizeof(latest_url), "https://github.com/%.*s/releases/latest",
             (int)repo_length, repo_start);
    set_message("Recherche de la release...");
    begin_busy("Page de release", false);
    bool fetched = fetch_effective_url(latest_url, effective_url, sizeof(effective_url));
    set_busy_state(false);
    if (!fetched) return false;

    const char *tag = strstr(effective_url, "/releases/tag/");
    if (!tag) return false;
    tag += strlen("/releases/tag/");
    const char *tag_end = tag;
    while (*tag_end && *tag_end != '/' && *tag_end != '?' && *tag_end != '#') ++tag_end;
    size_t tag_length = (size_t)(tag_end - tag);
    if (!tag_length || tag_length >= sizeof(next->tag)) return false;
    memcpy(next->tag, tag, tag_length);
    next->tag[tag_length] = 0;

    char repo[160];
    memcpy(repo, repo_start, repo_length);
    repo[repo_length] = 0;
    snprintf(expanded_url, sizeof(expanded_url), "https://github.com/%s/releases/expanded_assets/%s",
             repo, next->tag);

    MemorySink page = {0};
    page.capacity = API_LIMIT;
    page.data = calloc(1, page.capacity + 1);
    if (!page.data) return false;
    long status = 0;
    set_message("Lecture des fichiers de release...");
    begin_busy("Fichiers GitHub", false);
    fetched = fetch_memory_url(expanded_url, &page, &status, NULL, 0);
    set_busy_state(false);
    if (!fetched) { free(page.data); return false; }

    const char *scan = page.data;
    const char *asset_href = NULL;
    const char *asset_name = NULL;
    size_t asset_length = 0;
    while ((scan = strstr(scan, "<a href=\"")) != NULL) {
        const char *href = scan + strlen("<a href=\"");
        const char *quote = strchr(href, '"');
        if (!quote) break;
        const char *slash = quote;
        while (slash > href && slash[-1] != '/') --slash;
        size_t name_length = (size_t)(quote - slash);
        bool matches = name_length > 0;
        if (app->asset_prefix_match) {
            size_t prefix = strlen(app->asset_name);
            matches = matches && name_length >= prefix + 4 &&
                !strncmp(slash, app->asset_name, prefix) &&
                !strcmp(slash + name_length - 4, ".cia");
        } else {
            matches = matches && strlen(app->asset_name) == name_length &&
                !strncmp(slash, app->asset_name, name_length);
        }
        if (matches && (size_t)(quote - href) < 500 &&
            !strncmp(href, "/", 1) && strstr(href, "/releases/download/")) {
            asset_href = href;
            asset_name = slash;
            asset_length = name_length;
            break;
        }
        scan = quote + 1;
    }
    if (!asset_href || !asset_name || !asset_length || asset_length >= 180) {
        free(page.data); return false;
    }

    // Use the exact download path returned by GitHub. Reconstructing it from
    // the tag and filename can corrupt tags containing escaped characters and
    // needlessly assumes GitHub's URL spelling will never change.
    size_t href_length = 0;
    while (asset_href[href_length] && asset_href[href_length] != '"') ++href_length;
    static const char github_origin[] = "https://github.com";
    const char *expected_path = NULL;
    size_t expected_path_length = 0;
    if (!strncmp(app->url_prefix, github_origin, sizeof(github_origin) - 1)) {
        expected_path = app->url_prefix + sizeof(github_origin) - 1;
        const char *download_segment = strstr(expected_path, "/releases/download/");
        if (download_segment) expected_path_length = (size_t)(download_segment - expected_path);
        else expected_path = NULL;
    }
    if (!expected_path || !expected_path_length || href_length < expected_path_length ||
        strncmp(asset_href, expected_path, expected_path_length) != 0 ||
        href_length + strlen("https://github.com") >= sizeof(next->url)) {
        free(page.data); return false;
    }
    const char *download_segment = strstr(asset_href, "/releases/download/");
    if (!download_segment || (size_t)(download_segment - asset_href) != expected_path_length) {
        free(page.data); return false;
    }

    const char *row = asset_href;
    while (row > page.data && strncmp(row, "<li", 3) != 0) --row;
    const char *row_end = strstr(asset_href, "</li>");
    const char *digest = strstr(asset_href, "sha256:");
    if (!row_end || !digest || digest >= row_end || (size_t)(row_end - row) > 8192) {
        free(page.data); return false;
    }
    char raw_digest_value[72];
    memcpy(raw_digest_value, digest, 71);
    raw_digest_value[71] = 0;
    if (!hex_digest(raw_digest_value)) { free(page.data); return false; }

    snprintf(next->url, sizeof(next->url), "https://github.com%.*s",
             (int)href_length, asset_href);
    snprintf(next->digest, sizeof(next->digest), "%s", raw_digest_value + 7);
    free(page.data);

    // The exact size is read from the download response. Avoid a separate
    // request to the release asset CDN for each app in the startup scan.
    next->size = 0;
    next->valid = true;
    return true;
}

static bool fetch_release(const AppEntry *app) {
    release_info.checked = true;
    release_info.local_error = false;
    // A failed recheck must not leave a previous release marked available.
    bool installed = release_info.installed;
    u16 installed_version = release_info.installed_cia_version;
    memset(&release_info, 0, sizeof(release_info));
    release_info.checked = true;
    release_info.installed = installed;
    release_info.installed_cia_version = installed_version;
    if (!service_soc) {
        if (soc_error != 0)
            snprintf(message, sizeof(message), ui_french ? "SOC 0x%08lX ; X : reessayer" :
                     "SOC 0x%08lX; X: retry", (unsigned long)soc_error);
        else set_message("Reseau indisponible. X : reessayer.");
        return false;
    }
    ReleaseInfo next = {0};

    // Prefer GitHub's public release page. The unauthenticated REST API has a
    // low per-IP quota, and the page exposes the same asset name and SHA-256.
    // This avoids consuming eight API requests every time the app starts.
    if (fetch_release_page(app, &next)) goto release_ready;

    MemorySink *sink = calloc(1, sizeof(*sink));
    if (!sink) { set_local_error("Memoire insuffisante."); return false; }
    sink->capacity = API_LIMIT;
    sink->data = calloc(1, API_LIMIT + 1);
    if (!sink->data) { free(sink); set_local_error("Memoire insuffisante."); return false; }
    CURL *curl = acquire_curl();
    if (!curl) { free(sink->data); free(sink); set_local_error("Erreur libcurl."); return false; }
    if (!configure_curl(curl, app->api_url)) {
        release_curl(curl);
        free(sink->data); free(sink);
        return false;
    }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, memory_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, sink);
    struct curl_slist *headers = curl_slist_append(NULL, "Accept: application/vnd.github+json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 12L);
    set_message("Lecture de la release GitHub...");
    transfer_cancelled = false;
    begin_busy("Connexion GitHub", false);
    CURLcode result = curl_easy_perform(curl);
    set_busy_state(false);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    release_curl(curl);
    curl_slist_free_all(headers);
    if (result != CURLE_OK || status != 200) {
        if (result != CURLE_OK) common_network_failure = true;
        free(sink->data); free(sink);
        if (transfer_cancelled) { set_message("Connexion annulee."); return false; }
        if (result != CURLE_OK) { set_message(curl_easy_strerror(result)); return false; }
        if (status == 403) set_message("GitHub HTTP 403. X : reessayer plus tard.");
        else if (status == 429) set_message("GitHub HTTP 429. X : reessayer plus tard.");
        else snprintf(message, sizeof(message), ui_french ? "GitHub HTTP %ld. X : reessayer" :
                      "GitHub HTTP %ld. X: retry", status);
        return false;
    }

    json_error_t error;
    json_t *root = json_loadb(sink->data, sink->used, 0, &error);
    free(sink->data); free(sink);
    if (!root) {
        set_local_error("Metadonnees GitHub invalides."); return false;
    }
    const char *tag = json_string_value(json_object_get(root, "tag_name"));
    json_t *assets = json_object_get(root, "assets");
    bool found = false;
    if (tag && strlen(tag) < sizeof(next.tag) && json_is_array(assets)) {
        size_t i; json_t *asset;
        json_array_foreach(assets, i, asset) {
            const char *name = json_string_value(json_object_get(asset, "name"));
            const char *url = json_string_value(json_object_get(asset, "browser_download_url"));
            const char *digest = json_string_value(json_object_get(asset, "digest"));
            json_int_t size = json_integer_value(json_object_get(asset, "size"));
            if (!name || !url || !hex_digest(digest)) continue;
            if (app->asset_prefix_match) {
                size_t prefix = strlen(app->asset_name), length = strlen(name);
                if (strncmp(name, app->asset_name, prefix) != 0 || length < 4 || strcmp(name + length - 4, ".cia") != 0) continue;
            } else if (strcmp(name, app->asset_name) != 0) continue;
            if (strncmp(url, app->url_prefix, strlen(app->url_prefix)) != 0 || strlen(url) >= sizeof(next.url)) continue;
            if (size <= 0 || size > MAX_CIA_SIZE) continue;
            snprintf(next.tag, sizeof(next.tag), "%s", tag);
            snprintf(next.url, sizeof(next.url), "%s", url);
            snprintf(next.digest, sizeof(next.digest), "%s", digest + 7);
            next.size = (size_t)size;
            next.valid = true;
            found = true;
            break;
        }
    }
    json_decref(root);
    if (!found) {
        set_local_error("CIA ou empreinte absente."); return false;
    }
release_ready:
    // A successful page or API response clears a stale network warning left
    // by an earlier failed attempt.
    common_network_failure = false;
    next.installed = release_info.installed;
    next.installed_cia_version = release_info.installed_cia_version;
    release_info = next;
    release_info.checked = true;
    const VersionCacheEntry *cached = &version_cache[selected];
    if (!strcmp(cached->release_digest, release_info.digest)) {
        release_info.cia_version = cached->release_version;
    } else if (release_info.installed || selected == SELF_INDEX) {
        u16 version = 0;
        if (fetch_remote_cia_version(app, release_info.url, &version)) {
            release_info.cia_version = version;
            remember_release_version(selected, release_info.digest, version);
        }
    }
    set_message(release_info.cia_version ? "Version CIA reconnue." :
                "Version inconnue. A pour verifier le CIA.");
    return true;
}

static bool verify_sha256(const char *path, const char *expected) {
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    bool good = mbedtls_sha256_starts_ret(&context, 0) == 0;
    u8 buffer[16384];
    size_t processed = 0;
    while (good) {
        hidScanInput();
        if ((hidKeysDown() & KEY_B) || !aptMainLoop()) {
            transfer_cancelled = true;
            good = false;
            break;
        }
        size_t bytes = fread(buffer, 1, sizeof(buffer), file);
        if (bytes && mbedtls_sha256_update_ret(&context, buffer, bytes) != 0) good = false;
        processed += bytes;
        show_progress("Verification", processed, release_info.size);
        if (bytes < sizeof(buffer)) {
            if (ferror(file)) good = false;
            break;
        }
    }
    fclose(file);
    u8 hash[32];
    if (good) good = mbedtls_sha256_finish_ret(&context, hash) == 0;
    mbedtls_sha256_free(&context);
    if (!good) return false;
    char actual[65];
    for (int i = 0; i < 32; ++i) snprintf(actual + 2 * i, 3, "%02x", hash[i]);
    for (int i = 0; i < 64; ++i)
        if (tolower((unsigned char)actual[i]) != tolower((unsigned char)expected[i])) return false;
    return true;
}

static bool download_release(void) {
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/ShieldUpdater", 0777);
    if (download_only && mkdir("sdmc:/Shield_Download", 0777) != 0 && errno != EEXIST) {
        set_local_error("Dossier Shield_Download impossible a creer.");
        return false;
    }
    char previous_path[sizeof(cia_path) + 16];
    snprintf(previous_path, sizeof(previous_path), "%s.previous", cia_path);
    struct stat previous_file;
    if (stat(previous_path, &previous_file) == 0) {
        struct stat current_file;
        if (stat(cia_path, &current_file) == 0) {
            if (remove(previous_path) != 0) {
                set_local_error("Ancienne copie CIA impossible a nettoyer.");
                return false;
            }
        } else if (rename(previous_path, cia_path) != 0) {
            set_local_error("Ancien CIA impossible a restaurer.");
            return false;
        }
    }
    if (download_only) {
        struct stat existing;
        if (stat(cia_path, &existing) == 0 && existing.st_size > 0 &&
            (u64)existing.st_size == release_info.size) {
            transfer_cancelled = false;
            begin_busy("Verification", true);
            bool same_cia = verify_sha256(cia_path, release_info.digest);
            set_busy_state(false);
            if (transfer_cancelled) {
                set_message("Verification annulee.");
                return false;
            }
            if (same_cia) return true;
        }
    }
    bool downloaded = false;
    size_t downloaded_size = 0;
    for (unsigned attempt = 0; attempt < DOWNLOAD_ATTEMPTS; ++attempt) {
        FILE *file = fopen(download_path, "wb");
        if (!file) { set_local_error("Impossible d'ecrire sur la SD."); return false; }
        FileSink sink = { .file = file,
            .limit = release_info.size ? release_info.size : MAX_CIA_SIZE };
        CURL *curl = acquire_curl();
        if (!curl) {
            fclose(file); remove(download_path);
            set_local_error("Erreur libcurl.");
            return false;
        }
        if (!configure_curl(curl, release_info.url)) {
            release_curl(curl);
            fclose(file); remove(download_path);
            return false;
        }
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, file_write);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
        // CIA downloads may legitimately take longer than a fixed wall-clock
        // limit on a slow connection. Keep the total timeout disabled for every
        // app; the user can still cancel with B and the connect timeout remains.
        CURLcode timeout_result = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
        if (timeout_result != CURLE_OK) {
            release_curl(curl);
            fclose(file); remove(download_path);
            set_local_error("Initialisation HTTPS impossible.");
            return false;
        }
        transfer_progress_total = release_info.size;
        transfer_cancelled = false;
        begin_busy("Telechargement", release_info.size != 0);
        CURLcode result = curl_easy_perform(curl);
        set_busy_state(false);
        long status = 0;
        curl_off_t response_size = -1;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &response_size);
        release_curl(curl);
        bool closed = fclose(file) == 0;
        bool size_matches = release_info.size ? sink.written == release_info.size :
            (response_size <= 0 || (u64)response_size == sink.written);
        downloaded = response_size <= MAX_CIA_SIZE && result == CURLE_OK && status == 200 &&
            !sink.failed && closed && sink.written && sink.written <= MAX_CIA_SIZE && size_matches;
        if (downloaded) {
            downloaded_size = sink.written;
            break;
        }

        bool retryable = !transfer_cancelled && !sink.failed && closed &&
            retryable_download_error(result, status);
        remove(download_path);
        if (transfer_cancelled) {
            set_message("Telechargement annule. Aucune installation lancee.");
            return false;
        }
        if (result == CURLE_WRITE_ERROR && sink.failed) {
            set_local_error("Erreur d'ecriture sur la SD.");
            return false;
        }
        if (retryable && attempt + 1 < DOWNLOAD_ATTEMPTS) {
            common_network_failure = true;
            set_message("Reseau interrompu. Nouvelle tentative securisee...");
            refresh_ui();
            svcSleepThread(750000000LL);
            continue;
        }
        if (retryable) {
            common_network_failure = true;
            set_message("Reseau instable. Rien n'a ete installe. X : reessayer.");
        } else if (status == 403) set_message("GitHub HTTP 403. X : reessayer plus tard.");
        else if (status == 429) set_message("GitHub HTTP 429. X : reessayer plus tard.");
        else set_message("Telechargement incomplet. Rien n'a ete installe.");
        return false;
    }
    if (!downloaded) return false;
    common_network_failure = false;
    if (!release_info.size) release_info.size = downloaded_size;
    set_message("Verification SHA-256...");
    transfer_cancelled = false;
    begin_busy("Verification", true);
    bool verified = verify_sha256(download_path, release_info.digest);
    set_busy_state(false);
    if (!verified) {
        remove(download_path);
        if (transfer_cancelled) set_message("Verification annulee.");
        else set_local_error("Empreinte SHA-256 incorrecte.");
        return false;
    }
    struct stat old_file;
    bool had_previous = stat(cia_path, &old_file) == 0;
    if (had_previous && rename(cia_path, previous_path) != 0) {
        remove(download_path);
        set_local_error("Ancien CIA impossible a conserver.");
        return false;
    }
    if (rename(download_path, cia_path) != 0) {
        if (had_previous) (void)rename(previous_path, cia_path);
        remove(download_path);
        set_local_error("Impossible de finaliser le CIA.");
        return false;
    }
    if (had_previous) (void)remove(previous_path);
    return true;
}

static bool inspect_cia(AM_TitleInfo *title) {
    Handle file = 0;
    Result r = FSUSER_OpenFileDirectly(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
        fsMakePath(PATH_ASCII, cia_fs_path), FS_OPEN_READ, 0);
    if (R_FAILED(r)) { set_local_error("Lecture CIA impossible."); return false; }
    r = AM_GetCiaFileInfo(MEDIATYPE_SD, title, file);
    FSFILE_Close(file);
    if (R_FAILED(r)) { set_local_error("CIA invalide pour la console."); return false; }
    if (title->titleID != APPS[selected].title_id) { set_local_error("Identifiant CIA inattendu !"); return false; }
    return true;
}

static bool install_cia(void) {
    install_trace("open_cia_begin", 0, 0, 0);
    FILE *file = fopen(cia_path, "rb");
    if (!file) {
        install_trace("open_cia_failed", (Result)errno, 0, 0);
        set_local_error("CIA telecharge introuvable.");
        notify_install_result(false);
        return false;
    }
    u8 *buffer = malloc(CIA_IO_BUFFER_SIZE);
    if (!buffer) {
        install_trace("buffer_alloc_failed", 0, 0, CIA_IO_BUFFER_SIZE);
        fclose(file);
        set_local_error("Memoire insuffisante pour installer.");
        notify_install_result(false);
        return false;
    }
    begin_busy("Installation", true);
    install_trace("am_start_begin", 0, 0, 0);
    Handle handle = 0;
    Result r = AM_StartCiaInstall(MEDIATYPE_SD, &handle);
    install_trace("am_start_result", r, 0, (u32)handle);
    if (R_FAILED(r)) {
        set_busy_state(false);
        free(buffer);
        fclose(file);
        snprintf(message, sizeof(message), ui_french ? "AM demarrage : 0x%08lX" :
                 "AM start: 0x%08lX", (unsigned long)(u32)r);
        non_network_error_hint = true;
        notify_install_result(false);
        return false;
    }
    u64 offset = 0;
    bool okay = true;
    bool cancelled = false;
    const char *failed_stage = NULL;
    while (okay) {
        hidScanInput();
        if ((hidKeysDown() & KEY_B) || !aptMainLoop()) {
            okay = false;
            cancelled = true;
            failed_stage = "cancelled";
            break;
        }
        size_t bytes = fread(buffer, 1, CIA_IO_BUFFER_SIZE, file);
        if (!bytes) {
            if (ferror(file)) {
                okay = false;
                failed_stage = "read_failed";
                install_trace(failed_stage, (Result)errno, offset, 0);
            }
            break;
        }
        u32 written = 0;
        if (offset == 0) install_trace("fs_write_begin", 0, offset, (u32)bytes);
        r = FSFILE_Write(handle, &written, offset, buffer, (u32)bytes, 0);
        if (R_FAILED(r) || written != bytes) {
            okay = false;
            failed_stage = R_FAILED(r) ? "fs_write_failed" : "fs_write_short";
            install_trace(failed_stage, r, offset, written);
            break;
        }
        u64 previous_offset = offset;
        offset += written;
        if (previous_offset == 0 || (previous_offset >> 20) != (offset >> 20))
            install_trace("fs_write_progress", r, offset, written);
        show_progress("Installation", (size_t)offset, release_info.size);
    }
    if (fclose(file) != 0) {
        okay = false;
        if (!failed_stage) failed_stage = "close_failed";
        install_trace("close_failed", (Result)errno, offset, 0);
    }
    free(buffer);
    if (offset != release_info.size && !cancelled) {
        okay = false;
        if (!failed_stage) failed_stage = "size_mismatch";
        install_trace("size_check", 0, offset, (u32)release_info.size);
    }
    if (okay) {
        install_trace("fs_write_complete", 0, offset, 0);
        set_message("Finalisation du CIA...");
        begin_busy("Finalisation", false);
        install_trace("am_finish_begin", 0, offset, 0);
        r = AM_FinishCiaInstall(handle);
        install_trace("am_finish_result", r, offset, 0);
        set_busy_state(false);
        if (R_FAILED(r)) {
            snprintf(message, sizeof(message), ui_french ? "AM finalisation : 0x%08lX" :
                     "AM finish: 0x%08lX", (unsigned long)(u32)r);
            non_network_error_hint = true;
            notify_install_result(false);
            return false;
        }
        set_message("Installation terminee !");
        notify_install_result(true);
        return true;
    }
    Result cancel_result = AM_CancelCIAInstall(handle);
    install_trace("am_cancel_result", cancel_result, offset, 0);
    set_busy_state(false);
    if (cancelled) set_message("Installation annulee.");
    else {
        snprintf(message, sizeof(message), ui_french ? "Installation : %s (voir install.log)" :
                 "Installation: %s (see install.log)",
            failed_stage ? failed_stage : "erreur inconnue");
        non_network_error_hint = true;
        notify_install_result(false);
    }
    return false;
}

static bool verify_selected(void) {
    select_paths();
    release_info.checked = true;
    release_info.downloaded = false;
    if (!fetch_release(&APPS[selected])) return false;
    if (!release_info.installed) set_message(download_only ? "App absente. A pour telecharger." : "App absente. A pour installer.");
    else if (!release_info.cia_version) set_message("Version inconnue. A pour verifier.");
    else if (release_info.installed_cia_version < release_info.cia_version) set_message(download_only ? "MAJ dispo. A pour telecharger." : "Mise a jour dispo. A pour installer.");
    else if (release_changed_same_version()) set_message(download_only ? "Release modifiee. A : telecharger." : "Release modifiee. A pour reinstaller.");
    else set_message("Application a jour.");
    return true;
}

static void download_and_install(void) {
    if (!service_am) {
        set_local_error(download_only ? "Verification CIA indisponible." : "Service d'installation absent.");
        return;
    }
    if (!release_info.valid && (!init_network() || !verify_selected())) return;
    if (release_info.installed && release_info.cia_version &&
        release_info.installed_cia_version > release_info.cia_version) {
        set_message("Version installee plus recente que GitHub.");
        return;
    }
    if (selected == SELF_INDEX && release_info.cia_version &&
        self_current_version() > release_info.cia_version) {
        set_message("Updater local plus recent que GitHub.");
        return;
    }
    if (!release_info.downloaded) {
        if (!download_release()) return;
        AM_TitleInfo candidate;
        if (!inspect_cia(&candidate)) return;
        release_info.cia_version = candidate.version;
        release_info.downloaded = true;
        remember_release_version(selected, release_info.digest, candidate.version);
    }
    if (release_info.installed && release_info.installed_cia_version > release_info.cia_version) {
        set_message("Version installee plus recente que GitHub.");
        return;
    }
    if (selected == SELF_INDEX && self_current_version() > release_info.cia_version) {
        set_message("Updater local plus recent que GitHub.");
        return;
    }
    if (download_only) {
        release_info.downloaded = false;
        set_message(ui_french ? "CIA dans /Shield_Download/; non installe" :
                    "CIA in /Shield_Download/; not installed");
        return;
    }
    char text[100];
    snprintf(text, sizeof(text), ui_french ? "CIA %u valide. Installation..." :
             "CIA %u verified. Installing...", release_info.cia_version);
    set_message(text); refresh_ui();
    if (install_cia()) {
        release_info.downloaded = false;
        remember_installed_release(selected, release_info.digest, release_info.cia_version);
        refresh_installed(selected);
        if (selected == SELF_INDEX) {
            self_updated = true;
            set_message("MAJ terminee. Quitter avec START, puis relancer dans HOME.");
        }
    }
}

static bool self_update_available(void) {
    return releases[SELF_INDEX].valid && releases[SELF_INDEX].cia_version &&
        (self_current_version() < releases[SELF_INDEX].cia_version ||
         release_changed_same_version());
}

static void change_selection(size_t next) {
    selected = next;
    install_confirm = false;
    reinstall_prompt = false;
    if (selected < list_offset) list_offset = selected;
    if (selected >= list_offset + VISIBLE_APPS) list_offset = selected - VISIBLE_APPS + 1;
    select_paths();
    if (!release_info.valid) {
        if (release_info.local_error) set_local_error("Statut indisponible. X : reverifier.");
        else set_message("Statut indisponible. X : reverifier.");
    }
    else if (!release_info.installed) set_message(download_only ? "App absente. A pour telecharger." : "App absente. A pour installer.");
    else if (!release_info.cia_version) set_message("Version inconnue. A pour verifier.");
    else if (release_info.installed_cia_version < release_info.cia_version)
        set_message(download_only ? "MAJ dispo. A pour telecharger." : "Mise a jour dispo. A pour installer.");
    else if (release_changed_same_version()) set_message(download_only ? "Release modifiee. A : telecharger." : "Release modifiee. A pour reinstaller.");
    else set_message(download_only ? "Application a jour. A : telecharger." : "Application a jour. A : reinstaller.");
    refresh_ui();
}

int main(void) {
    gfxInitDefault();
    // Home is available at idle and temporarily deferred during active work.
    // Preserve the existing sleep policy for the entire lifetime of Updater.
    aptSetHomeAllowed(true);
    aptSetSleepAllowed(false);
    // The loading artwork is 24-bit BGR. consoleInit switches the bottom
    // framebuffer to 16-bit RGB565, so initialize it only after the splash.
    gfxSetScreenFormat(GFX_BOTTOM, GSP_BGR8_OES);
    boot_loading = true;
    service_romfs = R_SUCCEEDED(romfsInit());
    load_version_cache();
    load_ui_language();
    bool options_saved = load_user_options();
    if (service_romfs) load_art();
    refresh_ui();
    if (service_romfs) {
        music_start();
        load_ca_bundle();
    }
    if (!service_romfs) set_local_error("Fond d'ecran indisponible.");
    else if (!ca_bundle) set_local_error("Certificat CA introuvable.");
    selected = SELF_INDEX;
    list_offset = SELF_INDEX >= VISIBLE_APPS ? SELF_INDEX - VISIBLE_APPS + 1 : 0;
    select_paths();
    boot_loading = false;
    bottom_console = consoleInit(GFX_BOTTOM, NULL);
    bottom_console_initialized = true;
    bottom_text_buffer = malloc(320 * 240 * sizeof(u16));
    if (bottom_text_buffer) bottom_console->frameBuffer = bottom_text_buffer;
    set_busy_state(false);
    bool run_interface = confirm_ui_language();
    if (run_interface) {
        if (!first_run_guide_seen())
            run_interface = show_first_use_guide();
        if (run_interface && !options_saved)
            run_interface = show_preferences_setup();
        if (run_interface) {
            boot_scan_index = 1;
            boot_loading = true;
            selected = SELF_INDEX;
            list_offset = SELF_INDEX >= VISIBLE_APPS ? SELF_INDEX - VISIBLE_APPS + 1 : 0;
            select_paths();
            refresh_ui();
            service_am = R_SUCCEEDED(amInit());
            if (!service_am) set_local_error("Service AM indisponible.");
            if (service_romfs && ca_bundle) init_network();
            refresh_installed(SELF_INDEX);
            if (service_soc && ca_bundle && !common_network_failure)
                verify_selected();
            else release_info.checked = true;
            for (size_t i = 0; i < SELF_INDEX && aptMainLoop(); ++i) {
                boot_scan_index = i + 2;
                selected = i;
                refresh_installed(i);
                refresh_ui();
                if (service_soc && ca_bundle && !common_network_failure)
                    verify_selected();
                else releases[i].checked = true;
            }
            selected = SELF_INDEX;
            list_offset = SELF_INDEX >= VISIBLE_APPS ? SELF_INDEX - VISIBLE_APPS + 1 : 0;
            select_paths();
            boot_loading = false;
            startup_checks_done = true;
            if (self_update_available()) {
                self_update_prompt = true;
            } else {
                selected = 0;
                list_offset = 0;
                select_paths();
                set_message("Scan termine. A pour choisir.");
            }
        }
        refresh_ui();
    }

    while (run_interface && aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();
        if (down & KEY_START) break;
        if (self_updated) { gspWaitForVBlank(); continue; }
        if (self_update_prompt) {
            if (down & (KEY_A | KEY_B)) {
                self_update_prompt = false;
                if (down & KEY_A) download_and_install();
                else change_selection(0);
                refresh_ui();
            }
            gspWaitForVBlank();
            continue;
        }
        if (down & KEY_SELECT) {
            options_active = !options_active;
            install_confirm = false;
            refresh_ui();
            continue;
        }
        if (options_active) {
            if (down & KEY_B) options_active = false;
            if (down & KEY_A) {
                bool previous_mode = download_only;
                download_only = !download_only;
                option_save_failed = !save_user_options();
                if (option_save_failed) {
                    download_only = previous_mode;
                    set_local_error("Options non sauvegardees sur la SD.");
                } else {
                    for (size_t i = 0; i < APP_COUNT; ++i) releases[i].downloaded = false;
                    select_paths();
                }
            }
            refresh_ui();
            continue;
        }
        if ((down & KEY_Y) && startup_checks_done && !busy && !install_confirm) {
            ui_french = !ui_french;
            bool saved = save_ui_language();
            if (saved)
                set_message(ui_french ? "Langue changee : Francais." : "Langue changee : English.");
            else
                set_message("Langue changee mais non sauvegardee.");
            refresh_ui();
            (void)show_first_use_guide();
            refresh_ui();
            continue;
        }
        if (down & (KEY_DUP | KEY_DDOWN)) {
            size_t next = selected;
            if (down & KEY_DDOWN) next = (selected + 1) % APP_COUNT;
            else next = (selected + APP_COUNT - 1) % APP_COUNT;
            change_selection(next);
        }
        if (down & KEY_X) {
            install_confirm = false;
            reinstall_prompt = false;
            memset(&release_info, 0, sizeof(release_info));
            refresh_installed(selected);
            if (init_network()) {
                verify_selected();
                if (selected == SELF_INDEX && self_update_available()) self_update_prompt = true;
            }
            refresh_ui();
        }
        if (down & KEY_B && install_confirm) {
            install_confirm = false;
            set_message(download_only ? "Telechargement annule." :
                        (reinstall_prompt ? "Reinstallation annulee." : "Installation annulee."));
            reinstall_prompt = false;
            refresh_ui();
        }
        if (down & KEY_A) {
            if (install_confirm) {
                install_confirm = false;
                reinstall_prompt = false;
                download_and_install();
            } else {
                install_confirm = true;
                reinstall_prompt = !download_only && release_info.installed && release_info.cia_version &&
                    release_info.installed_cia_version == release_info.cia_version;
                char prompt[sizeof(message)];
                if (download_only)
                    snprintf(prompt, sizeof(prompt), ui_french ? "Telecharger %s ? A : oui B : non" :
                             "Download %s? A: yes B: no", APPS[selected].name);
                else if (ui_french)
                    snprintf(prompt, sizeof(prompt), "%s %s ? A : oui  B : non",
                             reinstall_prompt ? "Reinstaller" : "Installer", APPS[selected].name);
                else
                    snprintf(prompt, sizeof(prompt), "%s %s? A: yes  B: no",
                             reinstall_prompt ? "Repair" : "Install", APPS[selected].name);
                set_message(prompt);
            }
            refresh_ui();
        }
        gspWaitForVBlank();
    }
    if (service_soc) {
        if (network_curl) { curl_easy_cleanup(network_curl); network_curl = NULL; }
        curl_global_cleanup();
        socExit();
    }
    free(socket_buffer);
    if (service_am) amExit();
    if (service_mcu_hwc) mcuHwcExit();
    music_stop();
    if (service_romfs) romfsExit();
    free(font_atlas);
    free(app_icons);
    free(updater_icon);
    free(boot_bottom);
    if (bottom_console && bottom_text_buffer)
        bottom_console->frameBuffer = (u16 *)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
    free(bottom_text_buffer);
    free(ca_bundle);
    free(top_background);
    // Restore the normal system policy as the updater exits.
    aptSetSleepAllowed(true);
    aptSetHomeAllowed(true);
    gfxExit();
    return 0;
}
