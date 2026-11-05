<div align="center">

# Osteon2D

**Windows 向けの無料・オープンソースの 2D スケルタルアニメーションエディター。**

[English](../../README.md) · [हिन्दी](README.hi.md) · [Español](README.es.md) · [简体中文](README.zh-CN.md) · 日本語 · [Português (BR)](README.pt-BR.md)

</div>

## Osteon2D とは？

Osteon2D は、アートワークの上にボーン階層を作り、画像やメッシュをボーンに割り当て、キーフレームで 2D キャラクターを動かすネイティブのデスクトップエディターです。小さな単一の実行ファイル（C++20、Direct3D 11、Dear ImGui）で、ブラウザーも追加のランタイムもアカウントも不要です。

> **Spine に着想を得ていますが、複製ではありません。** Osteon2D は Esoteric Software の [Spine](https://esotericsoftware.com) に着想を得ていますが、ソフトウェア全体を再現するつもりは**ありません**。独自のコードと方向性を持つ独立したプロジェクトです。適切な範囲で、公開されている Spine JSON 形式を読み書きします。

## ダウンロード

最新のインストーラーは **[Releases ページ](https://github.com/Divyash93/Osteon2D/releases/latest)** から入手できます。`OsteonSetup.exe`（管理者権限不要）または `Osteon-0.1.0-win64.zip`（ポータブル版）。動作環境：Windows 10/11（x64）と Direct3D 11 対応 GPU。現在は初期アルファ版（0.1.0）です。

## 主な機能

- **リギング：** ボーン、スロット、スキン、描画順、Pose/Create ツール、回転/移動/拡大縮小/シアーのギズモ。
- **アート：** リージョン、メッシュ、リンクメッシュ、バウンディングボックス、クリッピング、パス。**PSD**・単一画像・複数画像のインポート。
- **ウェイト：** Bind、Auto、Smooth、Prune、ブラシと直接ペイント。
- **コンストレイント：** IK、トランスフォーム、パス、物理。
- **アニメーション：** ドープシート、グラフエディター、オートキー、プレビュー。
- **出力：** `.ost`、Spine JSON、PNG 連番、GIF、動画、テクスチャアトラス。
- 自動保存とクラッシュ復旧、すべての編集で元に戻す/やり直し。

## ランタイム（近日公開）

ゲーム内でアニメーションを再生する公式ランタイムは**近日公開予定**です：Unity、Godot、Web（PixiJS、Phaser、WebGL）、C/C++ エンジン。まだ開発には着手していません。

## 言語

この README は 6 言語で提供しています。エディターの UI は現在英語のみで、**対応する UI 言語は今後追加予定です。**

## ライセンスと法的事項

Osteon2D は **MIT ライセンス**（[LICENSE](../../LICENSE)）で公開されています。サードパーティのコードはそれぞれのライセンスに従います（[THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md)）。Osteon2D は独立したプロジェクトであり、Esoteric Software とは提携・承認・後援の関係にありません。「Spine」は Esoteric Software LLC の商標です。
