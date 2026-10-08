# C++安全性CI

## 背景と目的

C++では、範囲外アクセス、解放済み領域の利用、未定義動作などが、実行時の不具合や将来の修正による回帰につながります。通常のテストだけでは検出できない問題を早い段階で見つけるため、このワークフローは sanitizer 検査と CodeQL 解析をプルリクエストごとに実行します。

## CIで実行する検査

[ワークフロー定義](../../.github/workflows/cpp-safety.yml)は、プルリクエスト、`main`へのpush、手動実行で動きます。ワークフローは次の2種類の検査を独立して実行します。

1. **GCCとClangによるsanitizer検査**  
   Linux上でプロジェクトをDebug構成でビルドし、CTest全件を実行します。AddressSanitizerはメモリ誤用を、UndefinedBehaviorSanitizerは未定義動作を検出します。libstdc++のassertionとchecked iteratorも有効にします。
2. **CodeQLによる静的解析**  
   CMakeのRelease構成をビルドし、C/C++コードを解析してGitHub code scanningへ結果を送信します。

いずれかの構成、ビルド、テスト、解析が失敗した場合、該当するCIチェックは失敗します。

## ローカルでの実行方法

Linux上にGCCまたはClang、CMake、Python 3.11を用意し、リポジトリのルートで次のコマンドを実行します。

```bash
cmake -S . -B build-safety \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=g++ \
  -DKADOKA_ENABLE_SANITIZERS=ON
cmake --build build-safety --parallel 2
ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1 \
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  ctest --test-dir build-safety --output-on-failure
```

Clangで実行する場合は、構成コマンドの `-DCMAKE_CXX_COMPILER=g++` を `-DCMAKE_CXX_COMPILER=clang++` に置き換えます。ビルド成果物は `build-safety/` に作成されます。

CodeQL解析はGitHub Actions上で実行します。ローカルで同じ解析を実行する手順は、この変更の対象外です。

## CMakeオプション

`KADOKA_ENABLE_SANITIZERS` は既定で無効です。有効にすると、CMakeはLinux上のGCCまたはClangに対してASan、UBSan、libstdc++ assertion、checked iteratorを設定します。対応しないOSまたはコンパイラで有効にした場合、構成時にエラーになります。

## 検査の範囲

これらの検査は、現在のビルド対象とテストが通るコード経路を調べます。すべての不具合が存在しないことを証明するものではありません。