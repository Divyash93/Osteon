<div align="center">

# Osteon

**Um editor gratuito e de código aberto de animação esquelética 2D para Windows.**

[English](../../README.md) · [हिन्दी](README.hi.md) · [Español](README.es.md) · [简体中文](README.zh-CN.md) · [日本語](README.ja.md) · Português (BR)

</div>

## O que é o Osteon?

O Osteon é um editor de desktop nativo em que você cria uma hierarquia de ossos sobre a sua arte, vincula imagens ou malhas aos ossos e anima personagens 2D com poses-chave ao longo do tempo. É um único executável pequeno (C++20, Direct3D 11, Dear ImGui): sem navegador, sem runtime separado e sem conta.

> **Inspirado no Spine, não uma cópia.** O Osteon é inspirado no [Spine](https://esotericsoftware.com), da Esoteric Software, mas **não pretende replicar o software inteiro**. É um projeto independente, com código e direção próprios. Quando faz sentido, ele lê e grava o formato JSON do Spine, documentado publicamente.

## Download

Baixe o instalador mais recente na **[página de Releases](https://github.com/Divyash93/Osteon/releases/latest)**: `OsteonSetup.exe` (instalador, sem direitos de administrador) ou `Osteon-0.1.0-win64.zip` (portátil). Requisitos: Windows 10/11 (x64) e uma GPU com Direct3D 11. É um software em alfa inicial (versão 0.1.0).

## Recursos

- **Rigging:** ossos, slots, skins, ordem de desenho, ferramentas Pose/Create e gizmos de Rotate/Translate/Scale/Shear.
- **Arte:** regiões, malhas, malhas vinculadas, caixas delimitadoras, recorte, caminhos; importação de **PSD**, de uma imagem ou de várias imagens.
- **Pesos:** Bind, Auto, Smooth, Prune, pincel e pintura direta.
- **Restrições:** IK, transformação, caminho e física.
- **Animação:** dopesheet, editor de gráfico, Auto Key e pré-visualização.
- **Saída:** `.ost`, JSON do Spine, sequências PNG, GIF, vídeo e atlas de texturas.
- Salvamento automático com recuperação de falhas; desfazer e refazer em todas as edições.

## Runtimes (em breve)

Os runtimes oficiais para reproduzir as animações em jogos estão **em breve**: Unity, Godot, web (PixiJS, Phaser, WebGL) e engines C/C++. Ainda não foram escritos.

## Idiomas

Este README está disponível em seis idiomas. A interface do editor está por enquanto em inglês; **mais idiomas de interface chegam em breve.**

## Licença e aviso legal

O Osteon é distribuído sob a **licença MIT** ([LICENSE](../../LICENSE)). O código de terceiros mantém a sua própria licença ([THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md)). O Osteon é um projeto independente, sem afiliação, endosso ou patrocínio da Esoteric Software; "Spine" é marca registrada da Esoteric Software LLC.
