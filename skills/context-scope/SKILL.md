---
name: kadoka-context-scope
description: Kadoka Shougi AI のIssue調査や実装時、必要なコード・仕様・テストだけを選び、複数エージェントの担当範囲を決める。
---

# Context scope

- 最初にIssueの目的、必要な変更、完了条件を各1行で定める。`AI_CONTEXT.md`と`doc/architecture/CONTEXT_ROUTING.md`から対象の経路を選ぶ。
- `rg --files`で候補を探し、`rg -n`で関係箇所を絞る。対象の実装、仕様、テストを読み、判断に足りないときだけ隣接範囲へ広げる。
- 目的、変更場所、確認方法がそろったら探索を止める。検索結果の抜粋だけで意味を決めず、必要な原典を読む。
- 複数担当では一つの編集対象に一人の所有者を決め、IssueまたはPRに担当・開始SHA・編集範囲・依存先・確認方法を残す。共通ファイルは統合担当が順番に変更する。

出典: [ai-context-reducerの方法一覧](https://github.com/tomiya7688/ai-context-reducer/tree/main/docs/jp/手法)。このリポジトリの作業手順は`AGENTS.md`と`doc/development/PR_WORKFLOW.md`を正本とする。
