# Kadoka Shougi AI: agent entrypoint

作業の正本は依頼・Issue、仕様、実装とテストです。このファイルは入口だけを示します。

1. `git status --short` で未保存変更を確認し、`git fetch --prune origin` で共有先の最新情報を取得する。取得できなければ鮮度を未確認と報告し、古い基点でPRやマージを進めない。
2. Issueと公開PRを確認する。「次」では未完了PRを依存元から一件処理し、新規実装を積み増さない。各担当は基点SHA、担当範囲、編集可能なパス、確認方法を記録し、別の作業ツリーを使う。共有された未保存変更を上書きしない。
3. `AI_CONTEXT.md` → `doc/architecture/CONTEXT_ROUTING.md` → 対象のコード・仕様・テストの順で絞って読む。先に`rg --files`と`rg -n`で候補を探し、目的・対象・完了条件が分かったら探索を止める。
4. C++の新規・変更した宣言と処理には、[コーディング規約](doc/development/coding-guidelines.md)に従い、[Json-like Comment Outs](https://github.com/tomiya7688/Json-like-comment-outs)形式の説明を付ける。
5. 実装後は自己レビュー、対象確認、PR、最終headのCI、未解決レビュー確認を経て、問題がない場合にマージする。判定と積み上げPRの順序は[PR作業手順](doc/development/PR_WORKFLOW.md)を使う。

用途別の短い案内: [作業範囲の絞り方](skills/context-scope/SKILL.md) / [PR完了手順](skills/pr-lifecycle/SKILL.md)。
