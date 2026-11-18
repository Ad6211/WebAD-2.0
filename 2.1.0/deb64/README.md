# WebAD 2.1 — Linux 64 bits (Debian/Ubuntu)

Le paquet Debian `webad_2.1.0_amd64.deb` installe WebAD et `WebADUpdater`
dans `/usr/bin`, ainsi qu'une entrée WebAD dans le menu des applications et
son icône. APT installe les dépendances Qt 6 WebEngine, les outils d'indexation
du menu, Git et `pkexec` déclarés dans le paquet. Il cible Debian 13 amd64.

Les exécutables indépendants sont disponibles dans `build/`. L’updater a besoin
de Git et de `pkexec` pour installer une mise à jour confirmée.

Pour compiler depuis les sources, consultez `../source-code/README.md`.
