![Graphical Playground - Engine](https://github.com/GraphicalPlayground/.github/blob/main/assets/banners/gp-engine.svg)

[![Explorer la Plateforme](https://github.com/GraphicalPlayground/.github/blob/main/assets/cta/cta-explore-platform.svg)](https://graphical-playground.com)
[![Lire le Manuel](https://github.com/GraphicalPlayground/.github/blob/main/assets/cta/cta-read-handbook.svg)](https://handbook.graphical-playground.com)
[![Documentation](https://github.com/GraphicalPlayground/.github/blob/main/assets/cta/cta-documentation.svg)](https://docs.graphical-playground.com)
[![En savoir plus sur les Licences](https://github.com/GraphicalPlayground/.github/blob/main/assets/cta/cta-learn-licensing.svg)](https://graphical-playground.com/licensing)

# [@GraphicalPlayground](https://github.com/GraphicalPlayground)/gp-engine
<!-- gp:protected:start -->

🌎 Lire en : [English](../../README.md) | [Español](../es/README.md) | [Français](../fr/README.md) | [简体中文](../zh-cn/README.md)

**Table des matières**  
[Aperçu](#aperçu)  
┕ [Premiers Pas](#premiers-pas)  
┕ [Prérequis](#prérequis)  
┕ [Compilation du Moteur](#compilation-du-moteur)  
┕ [Galerie](#galerie)  
[Architecture](#architecture)  
┕ [Systèmes Principaux](#systèmes-principaux)  
┕ [Abstractions Progressives](#abstractions-progressives)  
┕ [Pipelines de Rendu (OpenGL / Vulkan / DirectX / ...)](#pipelines-de-rendu-opengl--vulkan--directx--)  
[Utilisation et Expérimentation](#utilisation-et-expérimentation)  
┕ [Projets Exemples](#projets-exemples)  
┕ [Programmation de Shaders](#programmation-de-shaders)  
[Documentation](#documentation)  
┕ [Référence API](#référence-api)  
┕ [Parcours d'Apprentissage](#parcours-dapprentissage)  
[Contribuer](#contribuer)  
┕ [Code de Conduite](#code-de-conduite)  
┕ [Sécurité](#sécurité)  
┕ [Licence](#licence)  
┕ [Dons](#dons)  
[Contact](#contact)  

## Aperçu

**gp-engine** est un moteur graphique C++ spécifiquement conçu pour l'apprentissage, l'expérimentation et l'enseignement des architectures de rendu modernes.

Construit autour d'une **Pédagogie Déconstructiviste**, le moteur comble le fossé entre les outils créatifs de haut niveau et la programmation matérielle de bas niveau. Plutôt que de masquer la complexité derrière un système boîte noire, `gp-engine` fournit des abstractions progressives sur les APIs de rendu modernes (OpenGL et Vulkan). Il permet aux utilisateurs de déconstruire, étudier, modifier et réimplémenter proprement les concepts graphiques fondamentaux, des algorithmes de rendu de base aux architectures GPU avancées, sans avoir à maîtriser des millions de lignes de code hérité.

### Premiers Pas

Bienvenue dans l'écosystème Graphical Playground. Pour commencer à expérimenter avec `gp-engine`, vous devrez cloner le dépôt et configurer l'environnement de compilation. Le moteur est construit avec une chaîne d'outils C++23 moderne et repose sur une architecture modulaire de qualité production.

Commencez par cloner le dépôt ainsi que ses sous-modules :

```bash
git clone --recursive https://github.com/GraphicalPlayground/gp-engine.git
cd gp-engine
```

### Prérequis

Étant donné que `gp-engine` tire parti des APIs graphiques modernes et des dernières fonctionnalités de C++, assurez-vous que votre environnement de développement satisfait les exigences suivantes :

- **Compilateur** : Un compilateur compatible C++23 (GCC 13+, Clang 16+, ou MSVC 19.38+)
- **Système de Build** : CMake (version 3.20 ou supérieure)
- **Pilotes Graphiques** : Des pilotes GPU à jour avec prise en charge de :
  - Vulkan `tba`
  - OpenGL `tba`
  - DirectX `tba` (Windows uniquement)
  - Metal `tba` (macOS uniquement)

### Compilation du Moteur

Le moteur utilise les CMake Presets pour simplifier la configuration et la compilation sur différentes plateformes et environnements. Nous recommandons d'effectuer une compilation hors du répertoire source, ce que les presets gèrent automatiquement afin de garder le répertoire de votre projet propre.

**Prérequis importants pour Linux** : Nos presets Linux nécessitent spécifiquement le compilateur Clang (`clang` et `clang++`) ainsi que `ccache` (cache de compilateur) installés sur votre système avant la compilation.

**1. Lister les presets disponibles :**

Vérifiez d'abord les presets de configuration disponibles pour votre plateforme spécifique (Linux, Windows ou macOS) :

```bash
cmake --list-presets
```

_Exemple de sortie :_

```text
Available configure presets:

  "linux-release"
  "linux-debug"
  "linux-profile"
  "linux-development"
```

> (Remarque : Vous verrez des préfixes `windows-...` ou `macos-...` selon votre système d'exploitation).

Pour des informations détaillées sur ce qu'implique chaque configuration, veuillez consulter la
[documentation sur les Types de Build](https://docs.graphical-playground.com/docs/gp-engine/Programming%20With%20C++/GP%20Build%20Tool/Build%20Type).

**2. Configurer le projet :**

Sélectionnez le preset approprié à votre environnement et générez les fichiers de build. Par exemple, pour configurer un build de développement sur Linux :

```bash
cmake --preset linux-development
```

> Remarque : Vous pouvez toujours ajouter des flags comme `-DGP_USE_VULKAN=ON` ou `-DGP_USE_OPENGL=ON` à votre commande de preset selon le backend de rendu que vous souhaitez cibler. Consultez la [documentation](#documentation) pour plus de détails sur la configuration des backends de rendu.

**3. Compiler le projet :**

Compilez le moteur en utilisant le preset de build correspondant. Vous pouvez également ajouter le flag `-j` pour utiliser plusieurs cœurs CPU et accélérer le processus :

```bash
cmake --build --preset linux-development -j$(nproc)
```

Une fois la compilation réussie, les binaires compilés et les projets d'expérimentation exemples seront situés dans le répertoire `binaries/bin/`. Vous pouvez exécuter l'un des exécutables exemples de base pour vérifier que le pipeline de rendu et le contexte de fenêtrage se sont initialisés correctement.

### Galerie

_en cours..._

## Architecture

_en cours..._

### Systèmes Principaux

_en cours..._

### Abstractions Progressives

_en cours..._

### Pipelines de Rendu (OpenGL / Vulkan / DirectX / ...)

_en cours..._

## Utilisation et Expérimentation

_en cours..._

### Projets Exemples

_en cours..._

### Programmation de Shaders

_en cours..._

## Documentation

La documentation complète de `gp-engine` est hébergée sur notre portail de documentation principal. Que vous dessiniez votre premier triangle ou que vous écriviez un pass de rendu Vulkan personnalisé, nos guides sont conçus pour soutenir votre expérimentation.

- [**Portail de Documentation Principal**](https://docs.graphical-playground.com)
- [**Introduction au Moteur**](https://docs.graphical-playground.com/docs/engine/intro)

### Référence API

Notre documentation détaillée de l'API C++ décrit les classes principales, les pipelines de rendu et les bibliothèques mathématiques qui composent `gp-engine`. Si vous étendez le moteur ou modifiez ses composants principaux, veuillez consulter nos guides C++ et nos règles de formatage pour vous assurer que votre code s'aligne avec l'architecture du projet.

- [**Référence de l'API C++**](https://docs.graphical-playground.com/docs/engine/cpp-api-reference)
- [**Guide de Programmation C++**](https://docs.graphical-playground.com/docs/engine/programming-with-cpp)
- [**Standard de Code C++**](https://docs.graphical-playground.com/docs/engine/programming-with-cpp/coding-standard)

### Parcours d'Apprentissage

Graphical Playground est construit autour d'une Pédagogie Déconstructiviste : nous voulons que vous démontiez le moteur et compreniez comment il fonctionne. Pour guider vos études, nous avons élaboré des parcours d'apprentissage structurés qui vous emmènent des concepts fondamentaux de la programmation graphique jusqu'aux architectures de moteurs avancées.

- [**Explorer les Parcours d'Apprentissage**](https://graphical-playground.com/discover)

<!-- gp:protected:end -->
## Contribuer

Nous accueillons les contributions de tous ! Que vous corrigiez un bug, implémentiez de nouvelles fonctionnalités ou amélioriez notre documentation, votre aide est appréciée. Consultez notre guide complet [CONTRIBUTING.md](./CONTRIBUTING.md) pour des informations détaillées sur nos standards et le processus de revue des pull requests.

### Code de Conduite

Pour garantir un environnement accueillant, collaboratif et inclusif pour tous ceux qui apprennent et construisent au sein de l'écosystème Graphical Playground, tous les contributeurs et participants sont tenus de respecter notre [Code de Conduite](./CODE_OF_CONDUCT.md). Veuillez le consulter avant de participer à des discussions communautaires ou de soumettre du code.

### Sécurité

Si vous découvrez une vulnérabilité de sécurité dans `gp-engine`, veuillez ne pas la signaler en ouvrant un issue public. Référez-vous plutôt à notre [Politique de Sécurité](./SECURITY.md) pour obtenir des instructions sur la manière de divulguer la vulnérabilité de manière sécurisée aux mainteneurs.

### Licence

`gp-engine` est un logiciel open-source. Veuillez consulter le fichier [LICENSE.md](../../LICENSE.md) dans le répertoire racine pour les termes complets concernant la modification, la distribution et l'utilisation dans vos propres projets.

### Dons

Si vous trouvez `gp-engine` utile pour votre apprentissage, votre recherche académique ou votre parcours dans le développement de jeux, pensez à soutenir le projet. Maintenir un moteur graphique C++ moderne demande un temps et des ressources considérables !

Vous pouvez parrainer le projet Graphical Playground via les liens suivants :

- [**Buy Me A Coffee**](https://www.buymeacoffee.com/GraphicalPlayground)
- [**GitHub Sponsors**](https://github.com/sponsors/GraphicalPlayground)
- [**Don Direct**](https://graphical-playground.com/donate)

Vous pouvez consulter la liste complète des sponsors et supporters sur notre
[Page des Sponsors](https://graphical-playground.com/sponsors) ou dans [DONORS.md](../../DONORS.md).
Votre soutien nous aide à continuer à développer des ressources éducatives de haute qualité et à maintenir le moteur pour la prochaine génération d'ingénieurs graphiques.

## Contact

Si vous avez des questions, des suggestions ou souhaitez partager vos projets réalisés avec `gp-engine`, nous serions ravis de vous entendre ! Vous pouvez nous contacter via les canaux suivants :

- **GitHub Discussions** : [Rejoindre la Conversation](https://github.com/orgs/GraphicalPlayground/discussions)
- **E-mail** :
  - [support@graphical-playground.com](mailto:support@graphical-playground.com)
  - [security@graphical-playground.com](mailto:security@graphical-playground.com)
  - [contact@graphical-playground.com](mailto:contact@graphical-playground.com)
- **Réseaux Sociaux** :
  - [LinkedIn](https://www.linkedin.com/company/graphical-playground)
  - [Discord](https://discord.graphical-playground.com)

---
© 2026 Graphical Playground. Conçu pour la prochaine génération d'ingénieurs graphiques.

![Graphical Playground](https://github.com/GraphicalPlayground/.github/blob/main/assets/misc/gplayd-footer.svg)
