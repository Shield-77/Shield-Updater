# Shield Updater

**Codex was used to generate the code**

Application 3DS de Shield-77 pour consulter les releases de ses applications, télécharger leurs CIA et, si l’utilisateur le choisit, les installer. La [release 1.1.24](https://github.com/Shield-77/Shield-Updater/releases/tag/1.1.24) contient **ShieldUpdater.cia** (version CIA interne 28).

<img width="245" height="245" alt="1000245603" src="https://github.com/user-attachments/assets/ca59a1c4-93c5-440e-b442-bfd6c18c63f1" />


## Premier lancement et commandes

Le démarrage affiche le guide, puis demande de choisir entre **télécharger seulement** et **télécharger et installer**. Ce choix précède la vérification des releases. Une mise à jour de Shield Updater est annoncée avec sa version ; elle attend la confirmation de l’utilisateur. Il n’y a pas d’installation automatique.

- **Haut/Bas** : choisir une application.
- **A** : ouvrir la confirmation de téléchargement ou d’installation, selon le mode choisi.
- **B** : revenir ou annuler une opération en cours.
- **X** : relancer la vérification de l’application sélectionnée.
- **SELECT** : ouvrir les options, notamment le mode de téléchargement et la langue.
- **START** : quitter.

En mode « télécharger seulement », le CIA vérifié est placé dans `sdmc:/Shield_Download/`. Le dossier est créé au besoin ; le supprimer plus tard n’empêche pas Shield Updater de démarrer. Un téléchargement identique déjà présent est réutilisé ; un nouveau CIA vérifié remplace l’ancien fichier du même nom. L’application contrôle l’identité du titre, sa version, sa taille et son empreinte avant d’annoncer le succès. Elle ne doit jamais confondre les CIA des applications du catalogue.

Pendant l’exécution, la mise en veille est empêchée pour protéger les transferts et installations. Fermer le clapet ne met donc pas la console en veille ; gardez assez de batterie.

## Identité et sources

- Dépôt : [Shield-77/Shield-Updater](https://github.com/Shield-77/Shield-Updater)
- CIA officiel : [ShieldUpdater.cia](https://github.com/Shield-77/Shield-Updater/releases/download/1.1.24/ShieldUpdater.cia)
- Title ID : `000400000A210400`
- Code public : répertoires [`source/`](source/), [`include/`](include/) et [`tools/`](tools/), ainsi que le [ZIP source 1.1.24](Shield-Updater-source-1.1.24.zip)

La release conserve aussi l’ancien nom de fichier `ShieldUniversalUpdater.cia` pour la transition des anciennes versions. Le code actuel recherche `ShieldUpdater.cia`. Les deux fichiers de cette release correspondent au même CIA ; l’ancien nom ne désigne pas un autre projet.

Le code original est distribué sous **GPL-3.0-only** ([LICENSE](LICENSE)). Les ressources tierces gardent leurs propres conditions : [NOTICE.md](NOTICE.md), [crédits audio du CIA](LICENSES/MUSIC-CREDITS.md) et [licence de la police](assets/LICENSE-AtkinsonHyperlegible.txt). Un fork doit identifier son origine et ses modifications, sans se présenter comme une publication officielle de Shield-77.

**Différence entre source public et CIA officiel :** le fanart de Callie utilisé sur l’écran inférieur du CIA officiel n’est pas inclus dans ce dépôt. Il est crédité à **kamrikeen** ; aucune autorisation de redistribution n’est documentée ici. Le source public reconstruit cet écran avec le visuel SHIELD. Les pistes musicales du CIA officiel sont également absentes du source public pour alléger le téléchargement. Le lecteur reste dans le code ; sans pistes, il échoue proprement et l’application fonctionne sans musique. La copie privée et le CIA déjà publié ne sont pas modifiés par ce nettoyage.

## Compiler

Installer devkitPro/devkitARM, libctru, les portlibs 3DS libcurl, Jansson, mbedTLS, Tremor et libogg, puis obtenir séparément `makerom` et `bannertool`. Depuis ce dépôt :

```sh
make cia MAKEROM=/chemin/makerom BANNERTOOL=/chemin/bannertool
```

La sortie attendue est `build/ShieldUpdater.cia`. Une compilation depuis ce source public diffère visuellement et musicalement du CIA officiel 1.1.24. Aucun exécutable des outils de construction n’est distribué dans le source.



Je tenais aussi à dire que dès que possible certainement vendredi après midi, je tirerai tout mes repository et je rendrais leurs licences bien plus claires et avec de meilleures crédit car je sais que j'ai toujours beaucoup de mal avec les crédits et les licences, je suis toujours excité à réaliser mes projets que j'oublie certains détails, c'est pourquoi je prendrais le soin des ce soir (jeudi heure de paris) ou vendredi pour tout mettre en règle, si j'avais oublié de cité précédemment certains crédits, je m'en excuserais et je les mettrais en évidence.
