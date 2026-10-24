# Training Checkpoint v1

## 目的

Training Checkpointは、学習処理を再開するための状態を保存し、読み込み時に学習設定とdatasetの互換性を確認します。特定のtensor libraryやtrainerには依存しません。trainerは重み、optimizer、scheduler、乱数生成器、dataset cursorの各状態をbyte列へencodeして渡します。

この層が保存する状態は、epoch、optimizer step、学習設定hash、dataset version、任意の数値metricsも含みます。optimizerを使わない場合など、不要なbyte列は空にできます。重み、config hash、dataset versionは必須です。

## API

- `save_checkpoint(path, state)` はtargetが存在しない場合だけ保存します。
- 保存先の親directoryは事前に存在する必要があります。
- checkpointはversion付きbinary形式で書き出し、checksumで偶発的な破損を検出します。
- 一時directoryはtargetと同じ親directoryに作り、全量を書いた後にhard linkで公開します。読み手には未完成ファイルを見せません。既存targetは置換しません。保存先のfilesystemがhard linkをサポートしない場合は保存に失敗します。
- `load_checkpoint(path, expected)` は必須field、format version、checksum、metricsを検証して状態を返します。
- expectedにconfig hashまたはdataset versionを指定した場合、不一致は読み込み失敗になります。空文字列の条件は照合しません。
- 1 checkpointの最大サイズは1 GiB、metrics数は最大100,000です。

Checksumは破損検出用であり、改ざん防止署名ではありません。公開の原子性は通常実行中の読み手に対する保証です。ファイル内容のflushは行いますが、電源断時の媒体永続性までは保証しません。保存・読込時は最大1 GiBのcheckpointに加えて復元用のbyte列をメモリに保持します。

## 保存形式

形式はlittle-endianの固定versionです。magic `KDKCKPT1`、u32 format version、u64 epoch、u64 step、u32長付きconfig hash、u32長付きdataset version、u64長付きbyte列5個（weights、optimizer、scheduler、RNG、dataset cursor）、u32 metrics個数、metrics map、u64 FNV-1a checksumの順です。各metricはu32長付き名前とIEEE 754 binary64値の組で、名前順に保存します。名前は非空、値は有限数のみです。checksumは末尾8 byteを除く全byteを対象にします。

## 境界

- モデルtensorの解釈、optimizer更新、schedulerの再構築はtrainer側の責務です。
- `latest` pointer、periodic save、best checkpoint、retentionはこのcoreに含めません。
- format versionを変更する場合は、新しいversionを読み取る移行方針と互換性テストを追加します。

## 確認

`checkpoint_store_test` は全stateのround-trip、既存targetを壊さないこと、config/dataset不一致、checksum破損、不正な必須fieldの拒否を確認します。
