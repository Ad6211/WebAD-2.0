# Extensions WebAD

WebAD prend en charge des extensions locales simples sous forme de scripts de
contenu. Dans la page `webad:extensions`, choisissez **Installer depuis un
dossier**. Le dossier source doit contenir un manifeste et un seul script JavaScript
local :

```json
{
  "name": "Exemple",
  "version": "1.0",
  "script": "content.js",
  "matches": ["https://*.example.com/*"]
}
```

`matches` accepte jusqu’à 50 motifs avec `*` et `?`, et doit viser des URL
`http://`, `https://` ou `*://`. Les fichiers référencés sont copiés lors de
l’installation ; les scripts distants et les chemins de script imbriqués ne
sont pas acceptés. La taille maximale d’un script est de 1 Mio.

Les scripts actifs sont injectés au chargement du document dans un monde
JavaScript isolé, uniquement lorsque l’URL correspond à un motif. Ils peuvent
toutefois modifier le DOM et agir sur la page : n’installez que du code de
confiance. WebAD ne fournit pas l’API complète des extensions Chrome/Firefox.
Les scripts tiers sont désactivés dans les fenêtres privées. Après une
activation ou une désactivation, rechargez la page pour appliquer le changement.

Un exemple prêt à installer, **Mimo, le petit compagnon**, se trouve dans
[`ext/`](../ext/README.md). Il ajoute un personnage original et déplaçable à
la page.
