# Source public de Shield Updater 1.1.24

Le code original de Shield Updater est distribué sous GNU GPL v3 uniquement (`GPL-3.0-only`). Les en-têtes minimp3, la police et les musiques conservent leurs licences propres, indiquées dans `LICENSES/` et `assets/LICENSE-AtkinsonHyperlegible.txt`.

La GPL autorise la copie, la modification et la redistribution du code, sous ses conditions. Elle ne donne pas le droit de prétendre qu'un fork est publié ou approuvé par Shield-77. Le nom SHIELD, les logos et l'identité du projet ne sont pas accordés pour présenter un fork comme une release officielle. Un fork doit indiquer clairement ses modifications et son origine. Cette mention n'interdit pas les références factuelles au projet d'origine.

L'illustration de Callie par kamrikeen est absente de cette copie publique. `tools/prepare_boot_art.py` crée à sa place un fond inférieur à partir du visuel SHIELD supérieur. Le CIA 1.1.24 déjà publié contient l'illustration créditée ; compiler cette copie publique produit donc un CIA dont le visuel de démarrage inférieur diffère.

Les exécutables `makerom` et `bannertool` ne sont pas redistribués ici. Les installer séparément et indiquer leur chemin avec `MAKEROM=...` et `BANNERTOOL=...` lors de la compilation. Les bibliothèques 3DS et devkitPro sont également requis. Le fichier `romfs/ca-bundle.pem` est un jeu de certificats publics utilisé pour HTTPS, pas une clé privée.
