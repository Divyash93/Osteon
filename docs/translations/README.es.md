<div align="center">

# Osteon2D

**Un editor gratuito y de código abierto de animación esquelética 2D para Windows.**

[English](../../README.md) · [हिन्दी](README.hi.md) · Español · [简体中文](README.zh-CN.md) · [日本語](README.ja.md) · [Português (BR)](README.pt-BR.md)

</div>

## ¿Qué es Osteon2D?

Osteon2D es un editor de escritorio nativo en el que dibujas una jerarquía de huesos sobre tu arte, les asignas imágenes o mallas y animas personajes 2D con poses clave a lo largo del tiempo. Es un único ejecutable pequeño (C++20, Direct3D 11, Dear ImGui): sin navegador, sin runtime aparte y sin cuenta.

> **Inspirado en Spine, no una copia.** Osteon2D se inspira en [Spine](https://esotericsoftware.com) de Esoteric Software, pero **no pretende replicar todo el software**. Es un proyecto independiente, con su propio código y su propia dirección. Cuando tiene sentido, lee y escribe el formato JSON de Spine, documentado públicamente.

## Descarga

Consigue el instalador más reciente en la **[página de Releases](https://github.com/Divyash93/Osteon2D/releases/latest)**: `OsteonSetup.exe` (instalador, sin derechos de administrador) o `Osteon-0.1.0-win64.zip` (portable). Requisitos: Windows 10/11 (x64) y una GPU Direct3D 11. Es software en fase alfa temprana (versión 0.1.0).

## Características

- **Rigging:** huesos, slots, skins, orden de dibujo, herramientas Pose/Create y gizmos Rotate/Translate/Scale/Shear.
- **Arte:** regiones, mallas, mallas enlazadas, cajas delimitadoras, recorte, rutas; importación de **PSD**, una imagen o varias imágenes.
- **Pesos:** Bind, Auto, Smooth, Prune, pincel y pintura directa.
- **Restricciones:** IK, transformación, rutas y física.
- **Animación:** dopesheet, editor de gráficas, Auto Key y vista previa.
- **Salida:** `.ost`, JSON de Spine, secuencias PNG, GIF, vídeo y atlas de texturas.
- Autoguardado con recuperación ante fallos; deshacer y rehacer en cada cambio.

## Runtimes (próximamente)

Los runtimes oficiales para reproducir las animaciones en juegos están **próximamente**: Unity, Godot, web (PixiJS, Phaser, WebGL) y motores C/C++. Todavía no se han escrito.

## Idiomas

Este README está disponible en seis idiomas. La interfaz del editor está por ahora en inglés; **pronto habrá más idiomas de interfaz.**

## Licencia y aviso legal

Osteon2D se publica bajo la **licencia MIT** ([LICENSE](../../LICENSE)). El código de terceros conserva su propia licencia ([THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md)). Osteon2D es un proyecto independiente, sin afiliación, respaldo ni patrocinio de Esoteric Software; «Spine» es una marca registrada de Esoteric Software LLC.
