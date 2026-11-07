---
name: kadoka-pr-lifecycle
description: Kadoka Shougi AI の実装を最新リモート基点のPRとして自己レビューし、CIとレビューを確認して安全にマージする。
---

# PR lifecycle

詳細は[PR作業手順](../../doc/development/PR_WORKFLOW.md)を参照する。

1. 作業前とマージ直前に`git fetch --prune origin`。未保存変更を保護し、Issue・公開PR・依存先を確認する。「次」では未完了PRを依存元から一件処理する。
2. 一件ごとの分離されたブランチで実装し、日本語コメントと必要な仕様・確認を加える。
3. 差分全体を自己レビューし、意図しないファイル、改行のみの差分、未解決の指摘を除いてPR本文に証跡を書く。
4. 最終headと現在のbaseに対するPRイベントのLinux・Windows・安全性CIを確認する。失敗、欠落、古い結果はマージ不可。
5. 積み上げPRは親から処理し、親のマージ後にbaseを更新してCIを再確認する。マージ直前にheadとbaseを再取得し、head SHA一致を指定してマージする。

チェックの名前、実行例、例外時の扱いは手順書を参照する。CI成功だけを自己レビュー完了とみなさない。
