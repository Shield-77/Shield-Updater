# Droits et ressources de Shield Updater 1.1.24

Le code original de Shield Updater est distribué sous GPL-3.0-only (`LICENSE`). Les en-têtes minimp3 sous CC0, la police sous SIL OFL et les ressources audio du CIA ont leurs propres conditions : voir `LICENSES/` et `assets/LICENSE-AtkinsonHyperlegible.txt`. Les crédits et incertitudes d’autorisation des quatre morceaux sont détaillés dans `LICENSES/MUSIC-CREDITS.md` ; en particulier, les droits de l’œuvre tierce utilisée dans le remix *Blue boi* ne sont pas établis.

La GPL permet de copier, modifier et redistribuer le code sous ses conditions. Elle ne permet pas de présenter un fork comme une release officielle de Shield-77. Les références factuelles au projet d’origine restent possibles.

La copie source publique n’inclut ni le fanart de Callie par kamrikeen, ni les quatre morceaux de musique du CIA officiel. Le fond inférieur public est construit avec `tools/prepare_boot_art.py` à partir du visuel SHIELD. Le CIA officiel 1.1.24 précédemment publié contient un autre fond inférieur et les morceaux crédités dans `LICENSES/MUSIC-CREDITS.md`. Une compilation depuis le source public sera donc différente.

`makerom` et `bannertool` doivent être obtenus séparément. Le fichier `romfs/ca-bundle.pem` contient des certificats publics pour HTTPS, pas une clé privée.
