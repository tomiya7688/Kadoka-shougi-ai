# 実装からPRマージまで

この手順は人とAIの共通作業です。PRを作っただけ、または過去のCIが成功しただけでは完了にしません。GitHub Issueと公開PRを課題一覧とし、「次」の依頼では未完了PRを依存元から一件処理します。着手できるPRがない場合にだけ、依存が満たされた新しいIssueを選びます。

## 1. 最新の共有状態から始める

実際のGitルートで次を実行します。

```sh
git status --short
git remote -v
git fetch --prune origin
git rev-parse origin/main
git diff --name-status HEAD...origin/main
```

取得に失敗したら原因を記録し、古い`origin/main`を最新と呼びません。GitHub APIなど別の経路で現在のSHAを確認できても、ローカル基点との差分を確認してから実装します。未保存変更がある作業ツリーを更新・清掃せず、担当別の作業ツリーを作ります。

対象Issue、公開PR、依存関係を確認します。積み上げPRの子に先行して新しい実装を重ねません。各担当はIssueかPRに担当者、開始SHA、base、編集可能なパス、完了条件、確認方法を残します。共通のCMake・README・目次・CI設定は統合担当が順に編集します。

## 2. 必要な範囲を選んで実装する

`AI_CONTEXT.md`と`doc/architecture/CONTEXT_ROUTING.md`から経路を選びます。まず`rg --files`と`rg -n`で候補を探し、対象の実装・仕様・テストを読みます。目的、対象、完了条件が分かったら検索を止め、実装中に新しい疑問が出た箇所だけ調べます。

一つのPRは単独で確認できる範囲にします。公開契約を変更した場合は対応する回帰確認と日本語仕様を追加します。C++の新規・変更した宣言と処理には日本語Json-like Comment Outsを適用します。

## 3. 自己レビューしてPRを作る

対象の検査後、`git diff --check`、変更ファイル一覧、`git diff`の全文を確認します。無関係なファイル、改行だけの差分、生成物、秘密情報、未完成のコメントを除きます。差し戻し・未解決レビューがあるPRでは、指摘の場所と修正結果を確認します。

PR本文には対象Issue、baseと開始SHA、依存PR、担当範囲、変更した責務、ローカル確認、未確認範囲、自己レビュー結果を書きます。CI欄は最終headの実行後に埋めます。PRのbaseは実際の依存先を指定し、`main`向けに見せるためだけのbase変更をしません。

## 4. マージ判定

担当者はマージ直前に`git fetch --prune origin`し、GitHub上のPR・base・headを再取得します。次のすべてを満たした場合だけマージします。

1. PRはopenでdraftではなく、mergeable。依存PRは先にマージ済みで、現在のbase SHAとPRのbase SHAが一致する。
2. PR本文に自己レビューの結果があり、変更ファイルの全差分を確認済み。差し戻し要求と未解決レビューがない。
3. **最終head**の`pull_request`イベントで、Linux `C++ CI`と`Windows build artifact`が成功。`PR metadata`と安全性ワークフローが対象なら、それぞれのmetadata検査、GCC・Clang sanitizer、CodeQLも成功。ジョブの欠落、進行中、失敗、取消し、別headまたは古いbaseでの成功は不可。
4. 仕様・テスト・配布物への影響を確認し、報告された未確認範囲がマージ条件と矛盾しない。

積み上げPRは親から順に処理します。親をマージした後、子のbaseを現在の`main`または次の未マージ親へ変更し、必要ならheadを更新してCIを再実行します。古いbaseでの成功を流用しません。

マージ直前にもPR head SHAとbase SHAを再照合します。GitHub CLIを使う場合は、検証済みheadを指定します。

```sh
gh pr merge <番号> --merge --match-head-commit <検証済みhead SHA>
```

積み上げPRの親はmerge commitで履歴の祖先関係を残します。別のAPIでマージする場合もexpected head SHAを指定します。baseが途中で進んだ場合は判定をやり直します。マージ後は`main`の新SHAと`push`イベントのCIを確認し、子PRのbaseと状態を更新します。失敗したチェックを無効化してマージしません。

## 5. 現在の制約

CI設定だけではGitHub上でマージを禁止できません。required checksとレビュー条件をmainのbranch protectionまたはrulesetへ設定する必要があります。設定権限がない場合は、上の手順をマージ担当が必ず実行し、保護が未設定であることを報告します。

この手順の情報量を抑えるため、作業メモには目的・参照先・完了条件とSHAだけを残し、長いログや仕様本文は元のIssue、PR、ファイルへリンクします。考え方は[ai-context-reducer](https://github.com/tomiya7688/ai-context-reducer)の「変更に必要なファイルから読む」「共有先の差分を先に確認する」「調査の停止条件を決める」に基づきます。
