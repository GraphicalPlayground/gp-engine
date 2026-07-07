![Graphical Playground - Engine](https://github.com/GraphicalPlayground/.github/blob/main/assets/banners/gp-engine.svg)

[![Explorar la Plataforma](https://github.com/GraphicalPlayground/.github/blob/main/assets/cta/cta-explore-platform.svg)](https://graphical-playground.com)
[![Leer el Manual](https://github.com/GraphicalPlayground/.github/blob/main/assets/cta/cta-read-handbook.svg)](https://handbook.graphical-playground.com)
[![Documentación](https://github.com/GraphicalPlayground/.github/blob/main/assets/cta/cta-documentation.svg)](https://docs.graphical-playground.com)
[![Conocer las Licencias](https://github.com/GraphicalPlayground/.github/blob/main/assets/cta/cta-learn-licensing.svg)](https://graphical-playground.com/licensing)

# [@GraphicalPlayground](https://github.com/GraphicalPlayground)/gp-engine
<!-- gp:protected:start -->

🌎 Leer en: [English](../../README.md) | [Español](../es/README.md) | [Français](../fr/README.md) | [简体中文](../zh-cn/README.md)

**Tabla de contenido**  
[Descripción](#descripción)  
┕ [Primeros Pasos](#primeros-pasos)  
┕ [Prerrequisitos](#prerrequisitos)  
┕ [Compilar el Motor](#compilar-el-motor)  
┕ [Galería](#galería)  
[Arquitectura](#arquitectura)  
┕ [Sistemas Principales](#sistemas-principales)  
┕ [Abstracciones Progresivas](#abstracciones-progresivas)  
┕ [Pipelines de Renderizado (OpenGL / Vulkan / DirectX / ...)](#pipelines-de-renderizado-opengl--vulkan--directx--)  
[Uso y Experimentación](#uso-y-experimentación)  
┕ [Proyectos de Ejemplo](#proyectos-de-ejemplo)  
┕ [Programación de Shaders](#programación-de-shaders)  
[Documentación](#documentación)  
┕ [Referencia de API](#referencia-de-api)  
┕ [Rutas de Aprendizaje](#rutas-de-aprendizaje)  
[Contribuir](#contribuir)  
┕ [Código de Conducta](#código-de-conducta)  
┕ [Seguridad](#seguridad)  
┕ [Licencia](#licencia)  
┕ [Donaciones](#donaciones)  
[Contacto](#contacto)  

## Descripción

**gp-engine** es un motor de gráficos en C++ específicamente diseñado para el aprendizaje, la experimentación y la enseñanza de arquitecturas de renderizado modernas.

Construido alrededor de una **Pedagogía Deconstructivista**, el motor tiende un puente entre las herramientas creativas de alto nivel y la programación de hardware de bajo nivel. En lugar de ocultar la complejidad detrás de un sistema de caja negra, `gp-engine` proporciona abstracciones progresivas sobre las APIs de renderizado modernas (OpenGL y Vulkan). Permite a los usuarios desmontar, estudiar, modificar y reimplementar limpiamente los conceptos gráficos fundamentales, desde algoritmos de renderizado básicos hasta arquitecturas avanzadas de GPU, sin necesidad de dominar millones de líneas de código heredado.

### Primeros Pasos

Bienvenido al ecosistema Graphical Playground. Para comenzar a experimentar con `gp-engine`, necesitarás clonar el repositorio y configurar el entorno de compilación. El motor está construido con una cadena de herramientas moderna C++23 y se basa en una arquitectura modular de grado de producción.

Primero, clona el repositorio junto con sus submódulos:

```bash
git clone --recursive https://github.com/GraphicalPlayground/gp-engine.git
cd gp-engine
```

### Prerrequisitos

Dado que `gp-engine` aprovecha las APIs gráficas modernas y las últimas características de C++, asegúrate de que tu entorno de desarrollo cumpla con los siguientes requisitos:

- **Compilador**: Un compilador compatible con C++23 (GCC 13+, Clang 16+, o MSVC 19.38+)
- **Sistema de Compilación**: CMake (Versión 3.20 o superior)
- **Controladores Gráficos**: Controladores GPU actualizados con soporte para:
  - Vulkan `tba`
  - OpenGL `tba`
  - DirectX `tba` (solo Windows)
  - Metal `tba` (solo macOS)

### Compilar el Motor

El motor utiliza CMake Presets para simplificar la configuración y compilación en diferentes plataformas y entornos. Recomendamos realizar una compilación fuera del árbol de fuentes, que los presets manejan automáticamente para mantener limpio el directorio de tu proyecto.

**Prerrequisitos importantes para Linux**: Nuestros presets de Linux requieren específicamente el compilador Clang (`clang` y `clang++`) así como `ccache` (caché del compilador) instalados en tu sistema antes de compilar.

**1. Listar los presets disponibles:**

Primero, comprueba los presets de configuración disponibles para tu plataforma específica (Linux, Windows o macOS):

```bash
cmake --list-presets
```

_Ejemplo de salida:_

```text
Available configure presets:

  "linux-release"
  "linux-debug"
  "linux-profile"
  "linux-development"
```

> (Nota: Verás prefijos `windows-...` o `macos-...` según tu sistema operativo).

Para información detallada sobre lo que implica cada configuración, consulta la
[documentación sobre Tipos de Compilación](https://docs.graphical-playground.com/docs/gp-engine/Programming%20With%20C++/GP%20Build%20Tool/Build%20Type).

**2. Configurar el proyecto:**

Selecciona el preset adecuado para tu entorno y genera los archivos de compilación. Por ejemplo, para configurar una compilación de desarrollo en Linux:

```bash
cmake --preset linux-development
```

> Nota: Puedes añadir flags como `-DGP_USE_VULKAN=ON` o `-DGP_USE_OPENGL=ON` a tu comando de preset según el backend de renderizado que desees usar. Lee la [documentación](#documentación) para más detalles sobre la configuración de backends de renderizado.

**3. Compilar el proyecto:**

Compila el motor usando el preset de compilación correspondiente. También puedes añadir el flag `-j` para utilizar múltiples núcleos de CPU y acelerar el proceso:

```bash
cmake --build --preset linux-development -j$(nproc)
```

Una vez que la compilación se complete con éxito, los binarios compilados y los proyectos de ejemplo estarán ubicados en el directorio `binaries/bin/`. Puedes ejecutar uno de los ejecutables de ejemplo básicos para verificar que el pipeline de renderizado y el contexto de ventana se hayan inicializado correctamente.

### Galería

_en progreso..._

## Arquitectura

_en progreso..._

### Sistemas Principales

_en progreso..._

### Abstracciones Progresivas

_en progreso..._

### Pipelines de Renderizado (OpenGL / Vulkan / DirectX / ...)

_en progreso..._

## Uso y Experimentación

_en progreso..._

### Proyectos de Ejemplo

_en progreso..._

### Programación de Shaders

_en progreso..._

## Documentación

La documentación completa de `gp-engine` está alojada en nuestro portal de documentación principal. Ya sea que estés dibujando tu primer triángulo o escribiendo un renderizado Vulkan personalizado, nuestras guías están diseñadas para apoyar tu experimentación.

- [**Portal de Documentación Principal**](https://docs.graphical-playground.com)
- [**Introducción al Motor**](https://docs.graphical-playground.com/docs/engine/intro)

### Referencia de API

Nuestra detallada documentación de la API de C++ describe las clases principales, los pipelines de renderizado y las bibliotecas matemáticas que componen `gp-engine`. Si estás extendiendo el motor o modificando sus componentes principales, revisa nuestras guías de C++ y las normas de formato para garantizar que tu código se alinee con la arquitectura del proyecto.

- [**Referencia de la API de C++**](https://docs.graphical-playground.com/docs/engine/cpp-api-reference)
- [**Guía de Programación en C++**](https://docs.graphical-playground.com/docs/engine/programming-with-cpp)
- [**Estándar de Código C++**](https://docs.graphical-playground.com/docs/engine/programming-with-cpp/coding-standard)

### Rutas de Aprendizaje

Graphical Playground está construido alrededor de una Pedagogía Deconstructivista: queremos que desmontes el motor y aprendas cómo funciona. Para orientar tus estudios, hemos elaborado rutas de aprendizaje estructuradas que te llevan desde los conceptos fundamentales de la programación gráfica hasta la arquitectura avanzada de motores.

- [**Explorar las Rutas de Aprendizaje**](https://graphical-playground.com/discover)

<!-- gp:protected:end -->
## Contribuir

¡Damos la bienvenida a las contribuciones de todos! Ya sea que estés corrigiendo un error, implementando nuevas funcionalidades o mejorando nuestra documentación, tu ayuda es apreciada. Consulta nuestra guía completa [CONTRIBUTING.md](./CONTRIBUTING.md) para información detallada sobre nuestros estándares y el proceso de revisión de pull requests.

### Código de Conducta

Para garantizar un entorno acogedor, colaborativo e inclusivo para todos los que aprenden y construyen dentro del ecosistema Graphical Playground, se espera que todos los colaboradores y participantes cumplan con nuestro [Código de Conducta](./CODE_OF_CONDUCT.md). Por favor, revísalo antes de participar en discusiones comunitarias o enviar código.

### Seguridad

Si descubres una vulnerabilidad de seguridad en `gp-engine`, por favor no la reportes abriendo un issue público. En su lugar, consulta nuestra [Política de Seguridad](./SECURITY.md) para obtener instrucciones sobre cómo divulgar la vulnerabilidad de forma segura a los mantenedores.

### Licencia

`gp-engine` es software de código abierto. Consulta el archivo [LICENSE.md](../../LICENSE.md) en el directorio raíz para conocer los términos completos sobre modificación, distribución y uso en tus propios proyectos.

### Donaciones

Si encuentras `gp-engine` útil para tu aprendizaje, investigación académica o desarrollo de videojuegos, considera apoyar el proyecto. ¡Mantener un motor de gráficos moderno en C++ requiere tiempo y recursos significativos!

Puedes patrocinar el proyecto Graphical Playground a través de los siguientes enlaces:

- [**Buy Me A Coffee**](https://www.buymeacoffee.com/GraphicalPlayground)
- [**GitHub Sponsors**](https://github.com/sponsors/GraphicalPlayground)
- [**Donación Directa**](https://graphical-playground.com/donate)

Puedes ver la lista completa de patrocinadores y colaboradores en nuestra
[Página de Patrocinadores](https://graphical-playground.com/sponsors) o en [DONORS.md](../../DONORS.md).
Tu apoyo nos ayuda a continuar desarrollando recursos educativos de alta calidad y mantener el motor para la próxima generación de ingenieros gráficos.

## Contacto

Si tienes preguntas, sugerencias o quieres compartir tus proyectos construidos con `gp-engine`, ¡nos encantaría saber de ti! Puedes contactarnos a través de los siguientes canales:

- **GitHub Discussions**: [Únete a la Conversación](https://github.com/orgs/GraphicalPlayground/discussions)
- **Email**:
  - [support@graphical-playground.com](mailto:support@graphical-playground.com)
  - [security@graphical-playground.com](mailto:security@graphical-playground.com)
  - [contact@graphical-playground.com](mailto:contact@graphical-playground.com)
- **Redes Sociales**:
  - [LinkedIn](https://www.linkedin.com/company/graphical-playground)
  - [Discord](https://discord.graphical-playground.com)

---
© 2026 Graphical Playground. Construido para la próxima generación de ingenieros gráficos.

![Graphical Playground](https://github.com/GraphicalPlayground/.github/blob/main/assets/misc/gplayd-footer.svg)
