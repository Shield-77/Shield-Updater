# Musiques et décodeurs — CIA Shield Updater 1.1.24

Les quatre fichiers ci-dessous sont intégrés au **CIA officiel** (version interne 28). Ils ne sont pas distribués dans la copie source publique. Cette liste distingue les crédits connus des autorisations encore à confirmer ; la GPL du code ne couvre pas automatiquement ces œuvres.

| Fichier | Crédit et source | Conditions indiquées par la source |
| --- | --- | --- |
| `01-cat-caffe.mp3` | **Cat Caffe**, TAD, [Lofi Compilation](https://opengameart.org/content/lofi-compilation) | CC0 1.0 indiqué dans les notes du projet ; page et fichier exact à revérifier. |
| `02-chillloopable.mp3` | **Chill (Loopable)**, Pro Sensory / Alex McCulloch, [page source](https://opengameart.org/content/chill-loopable) | CC0 1.0 indiqué dans les notes du projet ; le crédit Alex McCulloch y est demandé. Page et fichier exact à revérifier. |
| `03-chill-lofi-loop.ogg` | **Chill lofi inspired (loop edit)** par qubodup, d’après un morceau d’omfgdude, [page source](https://opengameart.org/content/chill-lofi-inspired-loop-edit) | La page indique CC0 1.0 et propose le crédit « Chill lofi inspired by omfgdude, made loopable by qubodup ». |
| `05-blue-boi.mp3` | **Blue boi**, [mise en ligne par FazePepinito23](https://www.newgrounds.com/audio/listen/1068377), signalée comme remix d’une œuvre tierce et taguée Lakey Inspired | La page indique attribution et partage dans les mêmes conditions pour cette mise en ligne. Elle demande aussi de vérifier les droits de l’œuvre tierce. **L’autorisation de l’auteur de l’œuvre d’origine n’est pas documentée ici.** |

La page Newgrounds de *Blue boi* ne prouve pas que la personne qui l’a mise en ligne détenait tous les droits du morceau d’origine. Il serait inexact de présenter une licence de cette page comme une autorisation certaine de Lakey Inspired. Le fichier actuellement distribué dans le CIA n’est pas modifié par cette mise à jour documentaire ; cette autorisation reste à clarifier avant une future redistribution.

**Warm Horizon** (`04-warm-horizon.mp3`) de Purrple Cat était présent dans une ancienne préparation, puis a été retiré avant CIA 27 ; il **n’est pas dans CIA 28**. [Source](https://purrplecat.itch.io/mellow-skies), CC BY 4.0 indiqué dans les notes du projet.

## Décodeurs

`minimp3.h` et `minimp3_ex.h` sont par lieff, sous CC0 1.0 : [source](https://github.com/lieff/minimp3), [texte inclus](minimp3-CC0.txt). La lecture OGG utilise Tremor et libogg fournis par les portlibs 3DS de devkitPro ; leurs notices relèvent des paquets installés séparément.
