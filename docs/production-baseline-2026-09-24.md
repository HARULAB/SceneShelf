# SceneShelf v0.1.0 本番基点

確認日: 2026-09-24

新機能の実装前に、現在の本番プラグインと Git の対応を確認した記録。

| 対象 | 確認結果 |
| --- | --- |
| GitHub リリース | [`v0.1.0`](https://github.com/HaruLab/SceneShelf/releases/tag/v0.1.0) |
| Git タグのコミット | `e578c921a7f7fcc5dad772c913172166bbcab6cf` |
| リリース ZIP 内の `SceneShelf.aux2` | SHA-256 `240149809FA2AC5A32DFF946DE57B48CA52EC8CE6BF5C377C985954E942E16BD` |
| 手元の `build/Release/SceneShelf.aux2` | 同じ SHA-256 |
| 本番 `C:\ProgramData\aviutl2\Plugin\SceneShelf.aux2` | 同じ SHA-256 |
| 実行中の AviUtl2 が読み込んだモジュール | 上記の本番 `SceneShelf.aux2` |

`v0.1.0` と実装開始前の HEAD の間で、`src/main.cpp`、`CMakeLists.txt`、同梱 SDK ヘッダーには差分がない。したがって、本番で使用中の v0.1.0 のソースはこの Git タグから復元できる。`build/` とテストホストは Git 管理対象外で、リリース ZIP は GitHub Releases に保存されている。
