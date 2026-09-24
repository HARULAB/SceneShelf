# SceneShelf

AviUtl2 のシーンを、SceneShelf ウィンドウ内の仮想フォルダで整理するプラグインです。

## できること

- フォルダの作成、名前変更、削除（削除したフォルダ内のシーンは親フォルダへ移動）
- シーンのフォルダ分け、ドラッグ＆ドロップによる並び替え
- シーン名の検索、ダブルクリックによるシーン切り替え
- シーンの右クリックメニューから、名前変更・設定・削除
- 選択したシーンのタイムライン追加、タイムラインへのドラッグ配置
- 「新規シーン」で選択中のオブジェクトだけをコピーしたシーンを作成（未選択なら空のシーン）
- タイムラインのオブジェクト右クリックから「SceneShelf → 選択オブジェクトからシーン作成」
- フォルダとシーンをアイコンで区別

## 大切な仕様

フォルダは SceneShelf ウィンドウ内だけに存在する仮想フォルダです。AviUtl2 本体のシーン一覧にフォルダを作ったり、シーンそのものを本体の階層間で移動したりはしません。フォルダ分けと並び順はプロジェクトごとの SceneShelf 用データとして保存されます。

シーンの名前変更・設定・削除は、対象シーンを選択したうえで AviUtl2 標準のシーン操作メニューに処理を渡します。SceneShelf 独自の分類情報と、AviUtl2 本体のシーン操作は別々に管理されます。

シーンをタイムラインへドラッグした場合は、ドロップ位置のレイヤー・フレームに 150 フレームのシーン参照を作ります。自分自身のシーンやタイムライン外へのドロップは追加しません。

選択オブジェクトからのシーン作成は、元のオブジェクトを残したままコピーします。最も早い開始位置と最上位レイヤーを新シーンの原点にし、相対位置を維持します。シーン作成とオブジェクト追加は SDK 上で一つの Undo 操作にまとめられず、対応しないエイリアスがあると一部のコピーに失敗する場合があります。

## 必要環境

- Windows x64
- AviUtl2 ExEdit2 2.1.10 以降

## インストール

1. リリースページから `SceneShelf-v0.1.0.zip` をダウンロードして展開します。
2. 展開した `SceneShelf.aux2` を AviUtl2 のデータフォルダ内にある `Plugin` フォルダへコピーします。
3. モジュールの信頼確認が表示された場合は許可し、AviUtl2 を再起動します。
4. ウィンドウ一覧から **SceneShelf** を開きます。

## ビルド

Windows x64、CMake 3.23 以降、Visual Studio 2022 以降が必要です。

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

生成物は `build/Release/SceneShelf.aux2` です。必要な AviUtl2 SDK ヘッダーを `vendor/aviutl2_sdk/` に含み、SDK のライセンス表記は `vendor/aviutl2_sdk/LICENSE.txt` に保持しています。

## ライセンス

本プラグインは MIT License です。詳細は [LICENSE](LICENSE) を確認してください。AviUtl2 SDK ヘッダーには別個のライセンス表記が適用されます。

---

# SceneShelf (English)

SceneShelf organizes AviUtl2 scenes into virtual folders inside its own window.

## Features

- Create, rename, and delete folders. Scenes in a deleted folder are moved to its parent.
- Assign scenes to folders and reorder them with drag and drop.
- Search scenes by name and switch scenes by double-clicking.
- Open AviUtl2's standard rename, settings, and delete commands from a scene's context menu.
- Add a scene at the timeline cursor or drag it from SceneShelf to a timeline position.
- Create a scene containing copies of selected objects via the New Scene button or an object context-menu command. With no selection, the button creates an empty scene.
- Distinguish folders and scenes with icons.

## Important behavior

Folders are virtual and exist only in the SceneShelf window. SceneShelf does not create folders in AviUtl2's native scene list or move scenes between native scene hierarchies. Folder assignments and ordering are saved as SceneShelf project data, separately from AviUtl2's native scene operations.

Rename, settings, and delete actions are handed to AviUtl2's standard scene-operation menu with the target scene selected.

Dragging a scene to the timeline creates a 150-frame scene reference at the drop layer and frame. Creating a scene from selected objects preserves their relative timing and layers and leaves the originals untouched. Scene creation and object insertion cannot be combined into one Undo operation by the SDK; some aliases may fail to recreate.

## Requirements

- Windows x64
- AviUtl2 ExEdit2 2.1.10 or later

## Installation

1. Download and extract `SceneShelf-v0.1.0.zip` from the Releases page.
2. Copy `SceneShelf.aux2` into the `Plugin` folder in AviUtl2's application-data directory.
3. Trust the module if prompted, then restart AviUtl2.
4. Open **SceneShelf** from the window list.

## Build

Requirements: CMake 3.23+ and Visual Studio 2022 or newer on Windows x64.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

The output is `build/Release/SceneShelf.aux2`. The required AviUtl2 SDK header is included in `vendor/aviutl2_sdk/`; its separate license notice is preserved in `vendor/aviutl2_sdk/LICENSE.txt`.

## License

This plugin is licensed under the MIT License; see [LICENSE](LICENSE). The AviUtl2 SDK header has a separate license notice.
