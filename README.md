# Shield Updater

**Téléchargements :** [ShieldUpdater.cia](https://github.com/Shield-77/Shield-Updater/releases/download/1.1.24/ShieldUpdater.cia) · [source public 1.1.24 sans Callie](https://github.com/Shield-77/Shield-Updater/releases/download/1.1.24/Shield-Updater-source-1.1.24.zip)


Source public de la release 1.1.24 (CIA interne 28). Le code original est sous
**GNU GPL v3 uniquement**. Les ressources tierces gardent leurs propres
licences ; voir [NOTICE.md](NOTICE.md) et `LICENSES/`.

L'illustration de Callie n'est pas distribuée dans ce source. Le fond inférieur
du boot est ici une découpe du visuel SHIELD supérieur, générée par
`tools/prepare_boot_art.py`. Le CIA officiel 1.1.24 publié précédemment a donc
un fond inférieur différent. La copie privée du projet n'est pas modifiée.

Shield Updater vérifie les releases des applications Shield. L'utilisateur
choisit de télécharger seulement le CIA dans `sdmc:/Shield_Download/`, ou de
le télécharger puis de l'installer depuis l'application. Une mise à jour de
Shield Updater est signalée et proposée, sans installation automatique.



<img width="1774" height="887" alt="1000243601" src="https://github.com/user-attachments/assets/6d3179aa-198c-4596-88da-e519fddcb192" />

**Les logos ont été réalisés avec canva, codex n'a pas touché à cette partie.**


## CIA 28 — nom officiel Shield Updater

Le nom affiché, le titre CIA et le fichier principal sont désormais **Shield
Updater** / `ShieldUpdater.cia`. L'application reconnaît exactement ce nom
dans sa propre release GitHub. Le Title ID `000400000A210400` reste identique
pour mettre à jour l'installation existante. La release de transition contient
aussi une copie identique sous l'ancien nom de fichier, uniquement pour les
versions déjà installées qui cherchent encore ce nom ; le nouveau code ne
l'utilise pas. Ce projet n'est pas affilié à Universal Updater.

## CIA 27 — release 1.1.23

Au premier lancement, le splash précède le choix de langue, le guide et le
questionnaire obligatoire. A choisit de télécharger les CIA sans les installer ;
B choisit de les installer dans l'application. START quitte sans enregistrer ce
choix et le questionnaire revient au prochain lancement. La vérification des
12 dépôts commence ensuite, avec le splash et un compteur. Une mise à jour
d'Updater est proposée sur l'écran inférieur avec son tag et sa version CIA ;
A accepte, B reporte. Elle ne se lance jamais automatiquement. SELECT ouvre
les options pour changer le mode choisi.

En mode « télécharger seulement », les CIA contrôlés sont enregistrés dans
`sdmc:/Shield_Download/`. Ce dossier est créé à chaque téléchargement si
nécessaire et sa suppression n'empêche pas le démarrage de l'application.
L'empreinte SHA-256, la version interne et l'identifiant du CIA restent
vérifiés avant de déclarer un téléchargement réussi. La logique de décodage
musical n'a pas été modifiée.
Chaque application utilise un seul nom de CIA dans `Shield_Download`. Si le
fichier présent a déjà la taille et l'empreinte attendues, l'Updater le
réutilise sans le télécharger une seconde fois. Si la release change, le
nouveau CIA vérifié remplace l'ancien ; une copie temporaire de l'ancien
permet de le restaurer si le remplacement échoue.
La piste `04-warm-horizon.mp3` a été retirée du RomFS et de la playlist du
candidat CIA 27 pour réduire le poids du téléchargement. Les quatre autres
pistes et le décodage audio restent inchangés.
La police supérieure Atkinson Hyperlegible conserve sa licence SIL Open Font
License 1.1 dans `assets/LICENSE-AtkinsonHyperlegible.txt` ; son rendu est
adouci avec les niveaux de gris déjà présents dans `romfs/font.bin`.

Le créateur a testé et approuvé cette version sur console. Elle est publiée
dans la [release 1.1.23](https://github.com/Shield-77/Shield-Updater/releases/tag/1.1.23)
avec le fichier exact `ShieldUniversalUpdater.cia`. Les versions antérieures
peuvent encore installer automatiquement cette transition unique ; le nouveau
comportement prend effet après l'installation de CIA 27.

## CIA internal version 23 - English and French

**English:** The interface selects French on a French-language system and English otherwise. After the startup checks, press Y to switch between the two languages. A four-page guide appears once on first use and explains the controls, installation and repair prompts, and possible reasons for an unavailable status. This status is informational and does not by itself mean the console is at risk. The optional notification LED gives a green flash after a successful install and an amber/red flash after a failed install, when the hardware service supports it.

**Francais :** L'interface choisit le francais si la langue du systeme est le francais, et l'anglais sinon. Apres les verifications de demarrage, Y permet de passer d'une langue a l'autre. Un guide de quatre pages apparait une fois et explique les commandes, l'installation, la reparation et les causes possibles d'un statut indisponible. Ce statut est informatif et ne signifie pas a lui seul que la console est en danger. Le voyant de notification optionnel clignote en vert apres une installation reussie et en orange/rouge apres un echec, si le service materiel le permet.

Application de Shield pour consulter les dernières releases GitHub et télécharger ou installer ses applications 3DS. Après le guide et le questionnaire initial, l'Updater vérifie sa propre release en premier, puis les onze autres applications. Le visuel Shield et l'image inférieure avec son crédit d'artiste restent affichés pendant ces vérifications. L'écran supérieur montre l'icône, une description et le statut de l'application sélectionnée. L'écran inférieur montre la liste et les commandes. L'interface se contrôle uniquement avec les boutons ; le tactile n'est pas utilisé.

| Application | Dépôt GitHub | Identifiant CIA attendu |
| --- | --- | --- |
| Shield DJ Lite | `Shield-77/Shield-DJ-Lite` | `000400000A210100` |
| Shield DJ Standard | `Shield-77/Shield-DJ-standard-` | `000400000A210300` |
| Shield MP4 Player | `Shield-77/Shield-MP4-Player` | `000400000BF75100` |
| Shield Photo | `Shield-77/Shield-Lecteur-d-images-2d-3d` | `000400000A210200` |
| Shield Online Art **[EXPERIMENTAL]** | `Shield-77/Shield-Online-Art` | `000400000A210500` (provisoire) |
| Shield MP4 Converter | `Shield-77/Shield-Optimised-MP4-Converter-` | `000400000BF75200` |
| Shield MP3 Downloader | `Shield-77/Shield-MP3-downloader` | `000400000BF74E00` |
| Shield Radio | `Shield-77/Shield-Radio-3DS` | `000400000F643100` |
| Shield Theme Music | `Shield-77/Shield-Theme-Music-Extractor` | `000400000BF75300` |
| Shield Checker | `Shield-77/Shield-Checker` | `000400000A210600` |
| Shield Updater | `Shield-77/Shield-Updater` | `000400000A210400` |

## Version CIA 18

Remplace la piste d'ambiance unique par cinq pistes MP3/OGG. Elles sont décodées en stéréo 22,05 kHz sur un fil audio dédié et s'enchaînent avec un fondu croisé de 0,5 seconde. La lecture MP3 se fait par blocs, sans charger un morceau entier en mémoire. Le DSP et chaque piste restent facultatifs : en cas d'échec audio, l'Updater continue de fonctionner. Les crédits des morceaux et des décodeurs sont dans `LICENSES/MUSIC-CREDITS.md`.

## Version CIA 20

Ajoute Shield Checker avec son icône. L'Updater consulte sa release GitHub, reconnaît le CIA exact `ShieldChecker.cia` et vérifie le Title ID `000400000A210600` avant toute installation. Le cache passe à `version-cache-v6.txt` pour éviter de confondre les anciens index avec cette nouvelle entrée.

## Version CIA 17

Ajoute Shield Online Art à la liste avec la mention **[EXPERIMENTAL]**. L'entrée vérifie la release `Shield-77/Shield-Online-Art`, attend l'asset exact `ShieldOnlineArt.cia` et contrôle le Title ID `000400000A210500`, confirmé sur le CIA publié v0.1.3. Le nouvel identifiant `version-cache-v5.txt` évite de réutiliser les anciens index du cache, qui changent avec cette entrée.

HAUT/BAS choisit instantanément une application parmi les résultats gardés en mémoire, sans nouvel accès réseau. A affiche « Installer [nom] ? » ou « Réinstaller [nom] ? » si la même version est déjà présente ; un second A confirme le téléchargement puis l'installation, B annule. Réinstaller sert aussi à réparer l'app. X relance manuellement la vérification de l'app sélectionnée. START quitte. B annule aussi un transfert en cours. HOME et la veille sont désactivés pendant toute l'exécution, notamment pendant les téléchargements et installations. Fermer la console ne la met donc pas en veille et peut vider la batterie.

Le statut lit la version interne du CIA publié avec une requête HTTPS partielle de 32 Ko, puis la compare à celle installée. Le résultat est gardé sur la SD sous son empreinte SHA-256, y compris si le même tag GitHub est réutilisé. Si la lecture partielle échoue, l'application peut toujours télécharger le CIA complet et vérifier sa version avant installation. Elle refuse un identifiant CIA inattendu ou une version plus ancienne que celle déjà installée. Le téléchargement complet vérifie la taille et le SHA-256 fournis par GitHub. Une fenêtre « VEUILLEZ PATIENTER » montre l'étape et sa progression, avec animation et temps écoulé quand le pourcentage est inconnu.

Shield Updater vérifie aussi sa propre release après le guide et le questionnaire initial. Quand un CIA plus récent est disponible, il prévient l'utilisateur et attend A pour le télécharger ou l'installer selon le mode choisi ; B reporte. Après une installation, il affiche « MAJ terminée » et demande de quitter avec START puis de relancer l'application. Le cache commun mémorise aussi la version des autres applications et l'empreinte de ce qui a été installé par l'Updater. Une nouvelle archive publiée sous le même numéro CIA peut ainsi être signalée comme « release modifiée » lorsqu'elle remplace une installation faite par l'Updater. En cas d'erreur locale ou d'installation, l'interface suggère de contacter Shield77 si le problème persiste. Les erreurs de Wi-Fi et de réseau sont signalées séparément.

La musique d'ambiance est une playlist intégrée au RomFS. Elle tourne sur un fil audio séparé pour continuer pendant le réseau et l'installation. Le mixage DSP reste à 1,00 ; un limiteur doux évite l'écrêtage des transitions. Si le DSP ou une piste est indisponible, l'Updater reste utilisable sans musique.

La version CIA 3 corrige l'écran inférieur du splash de vérification : l'image 24 bits est affichée avant l'initialisation de la console texte, qui bascule ensuite l'écran en 16 bits pour la liste. Cela évite de copier 230 400 octets dans un framebuffer de 153 600 octets. La version CIA 4 augmente la musique de fond.

## Version CIA 14

Bloque la mise en veille de la console pendant toute l'exécution de Shield Updater, y compris le scan des releases, le téléchargement, la vérification et l'installation d'un CIA. La veille normale est rétablie à la fermeture de l'application. La fermeture du clapet ne met donc pas la console en veille pendant que l'Updater fonctionne et consomme de la batterie.

Lors d'un téléchargement, une coupure réseau ou une connexion bloquée déclenche une seconde tentative automatique. Un transfert lent mais qui progresse n'a pas de limite de durée globale ; seule une vitesse inférieure à 1 octet/s pendant 20 secondes déclenche la détection de blocage. Le fichier temporaire incomplet est supprimé ; aucun CIA n'est installé avant la vérification de sa taille et de son SHA-256. Si le réseau échoue encore, l'Updater conserve l'application déjà installée et affiche qu'on peut réessayer avec X. Une erreur de stockage n'est pas relancée comme si c'était une erreur réseau.

## Version CIA 15

Le pourcentage du téléchargement utilise maintenant le total reçu de libcurl quand la page de secours GitHub ne fournit pas la taille à l'avance. Si le serveur ne donne aucun total, l'écran affiche les Ko reçus avec l'indicateur de chargement.

## Version CIA 16

La mise à jour de Shield Updater n'a plus de limite totale ni de seuil de débit minimal pendant le téléchargement du CIA : une liaison lente ou momentanément silencieuse n'est pas annulée par un chronomètre. Une erreur de connexion déclenche de nouvelles tentatives jusqu'à réussite ou annulation manuelle avec B ; un délai de connexion ne met donc pas fin à la mise à jour. Les téléchargements des autres applications gardent une limite de 180 s et deux tentatives. Tous les CIA restent contrôlés par taille, identifiant et SHA-256 avant installation.

## Release v1.1.5

Remplace le visuel du splash inférieur par le PNG fourni, qui conserve le crédit de l'artiste. Le fond 320 × 240 est produit à partir du fichier original 4:3 sans déformation.

## Release v1.1.7

Lit d'abord les pages publiques de release pour éviter de consommer la limite de l'API GitHub à chaque démarrage. Construit le lien CIA à partir du chemin exact fourni par GitHub, sans reconstruire le tag, puis vérifie le nom du dépôt, le nom de l'asset, sa taille et son SHA-256 avant installation. Une nouvelle vérification invalide correctement l'ancien résultat si le réseau échoue.

## Compilation

Installer devkitPro/devkitARM, libctru, les portlibs 3DS libcurl, jansson, mbedTLS, Tremor et libogg, ainsi que `makerom` et `bannertool`. Compiler avec `make cia`, en donnant au besoin `MAKEROM=/chemin/makerom` et `BANNERTOOL=/chemin/bannertool`. Les exécutables de ces outils ne sont pas fournis dans ce source.

Le RomFS est déclaré dans `app.rsf` et embarque les fonds `background.bgr` et `boot-bottom.bgr`, le logo `updater-icon.rgb` (également utilisé pour sa fiche), la police `font.bin`, les icônes des applications dans `app-icons.rgb`, le certificat `ca-bundle.pem` et quatre morceaux dans `romfs/music/`. Le CIA compilé est `build/ShieldUpdater.cia`, avec l'identifiant `000400000A210400`, distinct des autres applications installées. La version CIA 18 a ajouté la playlist MP3/OGG ; CIA 27 en retire la piste la plus lourde.

L'installation CIA utilise un tampon de 32 Ko sur le tas pour préserver la pile du thread principal. Les étapes d'ouverture, d'écriture et de finalisation AM sont consignées dans `sdmc:/3ds/ShieldUpdater/install.log` avec les codes d'erreur et offsets. Ce journal est créé seulement après une tentative d'installation. La release v1.1.8 utilise la version interne 13 ; la v1.1.9 porte le CIA à la version 14 ; la v1.1.10 à la version 15 ; la v1.1.11 à la version 16 ; la version CIA 17 ajoute Shield Online Art. L'ELF est conservé dans `build/ShieldUpdater.elf`.


Pour recréer le fond de boot inférieur public à partir de `romfs/background.bgr`, lancer `python tools/prepare_boot_art.py`.

## Crédit de l’illustration
Le fanart utilisé sur l’écran inférieur du splash du CIA officiel 1.1.24 est réalisé par **kamrikeen**. Il n'est pas présent dans ce dépôt source public.

Le créateur du projet a tenté de contacter l'artiste et souhaite conserver ce fanart avec son crédit en attendant une réponse. Il le retirera rapidement si l'artiste le demande. Aucune autorisation de redistribution n'est documentée ici.
