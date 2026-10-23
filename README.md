# WebAD

WebAD est un navigateur de bureau en C++17 qui utilise Qt 6 et Chromium via Qt
WebEngine. Son interface compacte à onglets conserve les pages de gestion à
portée de barre d’adresse et de menu.

## Fonctionnalités

- Navigation par onglets avec favicon du site, déplacements et fermeture des
  onglets.
- Ouverture normale des liens au clic, y compris les liens demandant un nouvel
  onglet.
- Menu contextuel adapté aux liens et au texte sélectionné : nouvel onglet,
  copie d’adresse, ajout aux favoris et recherche du texte.
- Fenêtre privée séparée (`Ctrl+Maj+P`) avec profil WebEngine hors-the-record,
  cookies, cache et stockage local temporaires ; l’historique et la liste des
  téléchargements privés ne sont pas enregistrés.
- HTTPS chiffre les échanges avec les sites qui le prennent en charge. Le mode
  privé n’est ni un VPN ni un chiffrement de toutes les connexions : le réseau
  peut encore observer des métadonnées (comme l’adresse IP et parfois le nom de
  domaine), et les fichiers téléchargés restent sur le disque.
- Interface claire aux tons lavande, avec barres de navigation et contrôles
  arrondis, et une page de nouvel onglet assortie.
- Page d’accueil WebAD avec recherche via le moteur configuré et raccourcis
  vers les favoris enregistrés.
- Logo WebAD navigateur/globe en dégradé, sans fond, utilisé comme icône de
  l’application et dans l’interface.
- Icônes vectorielles dessinées par WebAD pour la navigation, les pages
  intégrées, les favoris et la fermeture des onglets.
- Barre d’adresse et moteur de recherche configurable : DuckDuckGo, Google ou
  Bing.
- Favoris et historique des téléchargements conservés entre les lancements.
- Historique de navigation local, avec ouverture et suppression de pages.
- Suivi de progression, annulation et ouverture des fichiers téléchargés.
- Thème clair, sombre ou système ; barre de favoris et JavaScript configurables.
- Cookies persistants dans le profil normal, gestion de leur suppression et
  option de blocage des cookies tiers ; localStorage persistant et configurable.
- Inspecteur Chromium intégré (`Ctrl+Maj+I`) et page d’erreur WebAD avec
  l’illustration `https://http.cat/404`.
- Ouverture des fichiers locaux par boîte de dialogue, chemin direct ou URL
  `file:///`.
- Arguments de lancement pris en charge : un fichier HTML ou une URL ouvre
  directement un onglet. Dans les paramètres Windows, WebAD peut être ajouté à
  « Ouvrir avec » pour les fichiers HTML, sans remplacer l’application par défaut.
- Pages intégrées pour les favoris, téléchargements, paramètres et extensions.
- Accès direct aux pages intégrées via `webad:settings`, `webad:downloads`,
  `webad:extensions`, `webad:bookmarks`, `webad:history` et `webad:404`.
- Extensions locales de scripts de contenu : installation depuis un dossier,
  activation, désactivation et suppression ; chaque script est limité aux
  motifs HTTP/HTTPS déclarés dans son manifeste. Les extensions ne sont pas
  exécutées en fenêtre privée.
- Exemple d’extension [`ext/`](./ext/README.md) : Mimo, un petit compagnon
  animé que l’on peut déplacer sur la page.
- Cache HTTP sur disque et profil WebEngine persistant.

## Raccourcis clavier

| Raccourci | Action |
| --- | --- |
| `Ctrl+L` | Sélectionner la barre d’adresse |
| `Ctrl+K` | Rechercher depuis la barre d’adresse |
| `Ctrl+T` | Nouvel onglet |
| `Ctrl+Maj+T` | Rouvrir l’onglet récemment fermé |
| `Ctrl+W` | Fermer l’onglet |
| `Ctrl+Maj+P` | Nouvelle fenêtre privée |
| `Ctrl+Maj+N` | Nouvelle fenêtre privée (alternative) |
| `Ctrl+O` | Ouvrir un fichier local |
| `Ctrl+R` | Actualiser |
| `Ctrl+Maj+R` | Actualiser sans le cache |
| `Échap` | Arrêter le chargement |
| `Alt+Début` | Page d’accueil |
| `Alt+←` / `Alt+→` | Précédent / suivant |
| `Ctrl+Tab` / `Ctrl+Maj+Tab` | Onglet suivant / précédent |
| `Ctrl+1` à `Ctrl+8` | Aller aux onglets 1 à 8 |
| `Ctrl+9` | Aller au dernier onglet |
| `Ctrl+D` | Ajouter aux favoris |
| `Ctrl+F` | Rechercher dans la page |
| `Ctrl+Maj+I` | Ouvrir l’inspecteur |
| `F12` | Afficher/masquer l’inspecteur |
| `Ctrl+Shift+B` | Afficher les favoris |
| `Ctrl+H` | Afficher l’historique |
| `Ctrl+J` | Afficher les téléchargements |
| `Ctrl+,` | Afficher les paramètres |

Les combinaisons peuvent être personnalisées dans `webad:settings`.

## Prérequis

- CMake 3.21 ou plus récent.
- Un compilateur C++17. Sous Windows : Visual Studio Build Tools et MSVC.
- Qt 6.4 ou plus récent, avec **Svg**, **Widgets** et **WebEngineWidgets**.

Sous Windows, installez Qt avec Qt WebEngine et utilisez le dossier correspondant
au compilateur de votre installation.

## Compiler sous Windows

Depuis le dossier du projet dans PowerShell :

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\msvc2022_64"
cmake --build build --config Release
& .\build\Release\WebAD.exe
```

Remplacez le chemin d’exemple par le chemin réel de Qt. Pour distribuer
l’application, utilisez `windeployqt` depuis cette même installation Qt afin
de copier les bibliothèques et ressources nécessaires.

## Extensions

Les extensions sont des scripts de contenu locaux au format documenté dans
[`extensions/README.md`](./extensions/README.md). Installez seulement du code
de confiance : un script activé peut modifier le contenu des pages des sites
que ses motifs autorisent.

## Vérifier cookies et stockage local

Un test d’intégration autonome se trouve dans `tests/storage-smoke`. Il démarre
un serveur HTTP local et vérifie qu’un cookie persistant et une valeur
`localStorage` survivent à la recréation d’un profil, puis que ces données
restent temporaires dans un profil privé. Il vérifie aussi la détection d’une
réponse HTTP 404 et l’injection ciblée d’un script :

```powershell
cmake -S tests/storage-smoke -B build/storage-smoke `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\msvc2022_64"
cmake --build build/storage-smoke --config Release
& .\build\storage-smoke\Release\WebADStorageSmoke.exe
```
