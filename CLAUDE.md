# CLAUDE.md

開発ルールは [AGENTS.md](AGENTS.md) にまとめてある。作業を始める前に必ず読むこと。

特に次の 3 点は事故につながりやすいので注意する。

- 機種ごとの分岐を書かず、差分は `lib/DeviceProfile` に集約する
- 実機に依存しない純粋ロジックは `#ifdef ARDUINO` の外に置き、native テストを書く
- M5PaperColor はタッチパネルを搭載していないため、物理ボタンだけで操作できる UI にする
