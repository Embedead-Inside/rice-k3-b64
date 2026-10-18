# rice-k3-b64

[![License: MIT-0](https://img.shields.io/badge/License-MIT--0-blue.svg)](https://spdx.org/licenses/MIT-0.html)
[![Language: C99](https://img.shields.io/badge/Language-C99-green.svg)](#)

ベアメタル32ビット組み込み環境向けに極限まで最適化された、超軽量・超高速・ゼロ条件分岐の **ゴロム・ライス（Golomb-Rice）符号化コーデック** ライブラリです。
本ライブラリは、リソース制限が非常に厳しいマイコンシステムにおいて、ハードウェアの理論最速値でデータ圧縮・展開を行うことを目的に設計されています。

## 特徴

- **完全条件分岐レス（Branchless）**: `while` ループや `if` 文によるストリーム境界判定をアセンブラレベルで完全に排除。CPUのパイプラインを1クロックもストールさせません。
- **完全ループ展開（Loop Unrolled）**: ブロックサイズを `64バイト` に完全固定。コンパイラ最適化（`-O3`）によってループが直線命令へと完全に展開されます。
- **RAMアクセスゼロ（レジスタ完結）**: 演算中の全ての変数がCPUの汎用レジスタ内で完結するように配慮されており、実行中のスタック（RAM）への退避・読み出しオーバーヘッドがありません。
- **前処理の完全一体化**: 関数内部で自動的に「差分抽出（予測残差）」および「ZigZag符号化」を行います。これにより、ゆるやかに変化するセンサーデータ等に対して強制的に「0付近への超集中」を作り出し、ライス符号の持つ本来の最大圧縮率を引き出します。
- **高速CLZ命令の活用**: デコード時の一進符号処理には、GCC/Clangの組み込み関数 `__builtin_clz` を採用。アーキテクチャの持つ1クロックビット検索命令（CLZ）へ直結します。
- **MIT-0 ライセンス**: 著作権表示すら不要な無条件ライセンス。どのような商用/組み込み製品にも完全にノーストレスで統合可能です。

## 推奨するアーキテクチャ

汎用レジスタ（32bit）を豊富に持ち、CLZ（ビット探索）命令、またはそれに準ずる高速命令を備えたすべての32ビット組み込みプロセッサ。

## 仕様

- **パラメータ k**: `3` 固定（割り数 M = 8）
- **1ブロックサイズ**: `64 バイト` 固定
- **入力データ型**: `uint8_t` (0〜255)
- **最悪時出力サイズ**: 1データあたり最大35ビットの可変長となるため、64バイトの生データブロックに対して、圧縮バッファは念のため**最大320バイト**（5倍）確保することを推奨します（データが0付近に集中していれば、劇的に小さくなります）。

## 使い方

### 1. 1ブロック（64バイト）のみを圧縮・展開する場合

```c
#include "rice_k3_b64.h"
#include <string.h>

uint8_t raw_data[RICE_K3_B64_DATA_SIZE];      // 64バイトの生データ
uint8_t comp_buf[320];                        // 圧縮データ格納先
uint8_t rest_data[RICE_K3_B64_DATA_SIZE];     // 復元データ格納先

void example_single_block(void) {
    // 【重要】エンコーダはORマージを行うため、出力先を必ずゼロクリアします
    memset(comp_buf, 0, sizeof(comp_buf));

    // 圧縮を実行
    uint32_t compressed_bytes = rice_k3_b64_encode(raw_data, comp_buf);
    // 戻り値：実際に圧縮されて消費されたバイト数

    // 展開を実行
    uint32_t consumed_bytes = rice_k3_b64_decode(comp_buf, rest_data);
    // 戻り値：展開にあたって読み進めた圧縮バッファのバイト数
}
```

### 2. 256バイトなどの大きなデータを処理する場合（64バイト単位でループ）

大きなバッファを処理する場合は、呼び出し側で64バイトずつポインタを進めながら複数回呼び出します。

```c
#include "rice_k3_b64.h"
#include <string.h>

#define TOTAL_BYTES 256
#define NUM_BLOCKS  (TOTAL_BYTES / RICE_K3_B64_DATA_SIZE) // 4回

uint8_t src_buffer[TOTAL_BYTES];
uint8_t dst_buffer[TOTAL_BYTES * 2]; // 余裕を持たせたサイズ

void compress_large_packet(void) {
    memset(dst_buffer, 0, sizeof(dst_buffer));

    const uint8_t *in_ptr = src_buffer;
    uint8_t *out_ptr = dst_buffer;
    uint32_t total_out_bytes = 0;

    for (int i = 0; i < NUM_BLOCKS; i++) {
        uint32_t comp_len = rice_k3_b64_encode(in_ptr, out_ptr);
        
        in_ptr  += RICE_K3_B64_DATA_SIZE; // 入力は常に64バイト進む
        out_ptr += comp_len;              // 出力は実際の圧縮サイズ分だけ進む
        
        total_out_bytes += comp_len;
    }
    // 最終的に total_out_bytes 分のサイズを送信・保存します。
}
```

## 推奨コンパイルフラグ (GCC)

本ライブラリの分岐レス・固定ループのポテンシャルを100%解放し、アセンブラ命令を極限まで直線化させるために、以下の最適化フラグを付与してビルドしてください。

```bash
-O3 -funroll-loops
```

## ライセンス

このプロジェクトはMIT-0ライセンスのもとで公開されています。詳細は[LICENSE](LICENSE)ファイルをご覧ください。
- 商用利用・個人利用を問わず、完全自由に使用できます。
- 著作権表示やライセンス文言の保持・記載義務すらありません。
- ソースコードの改変、流用、再配布、自社製品への組み込み等、制限なくご活用いただけます。

