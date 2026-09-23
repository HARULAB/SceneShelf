# SceneShelf for AviUtl2

シーンをフォルダで整理するAviUtl2用のウィンドウプラグインです。AviUtl2本体のシーン一覧は変更せず、分類・並び順をプロジェクトごとに保存します。

## Features

- フォルダの作成、名前変更、削除（削除したフォルダ内のシーンは親へ移動）
- シーンのフォルダ分けとドラッグによる並び替え
- シーン名検索、ダブルクリックでシーン切替
- シーンの右クリックから、名前変更・設定・削除をAviUtl2標準メニューへ委譲
- 選択シーンのタイムライン追加、新規シーン作成
- フォルダとシーンをアイコンで区別

分類・並び順の保存にはプラグイン専用のプロジェクト領域を使います。既存のプロジェクトキーを維持しているため、旧版から分類データを引き継げます。

## Build

Requirements: Windows x64, CMake 3.23+, Visual Studio 2022 or newer, AviUtl2 ExEdit2 2.1.10+

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

Output: `build/Release/SceneShelf.aux2`

The required AviUtl2 SDK header is included under `vendor/aviutl2_sdk/`. Its separate MIT notice is preserved in `vendor/aviutl2_sdk/LICENSE.txt`.

## Install

Copy `SceneShelf.aux2` to AviUtl2's application-data `Plugin` directory, allow/trust the module if prompted, and restart AviUtl2. Open **SceneShelf** from the window list.

## Compatibility notes

- Scene rename, settings, and deletion are delegated to the Japanese AviUtl2 main-window menu so the host performs its normal operation and confirmation.
- Project folders are organizational metadata; they do not create native scene hierarchy in AviUtl2.
- The plugin requires AviUtl2 2.1.10 or later.

## License

MIT. See [LICENSE](LICENSE). The AviUtl2 SDK header has its own notice at `vendor/aviutl2_sdk/LICENSE.txt`.
