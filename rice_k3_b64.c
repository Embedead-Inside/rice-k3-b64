/**
 * @file rice_k3_b64.c
 * @brief 極限最適化版 ゴロム・ライス符号化 (k=3, 64バイト固定) 実装ソースファイル
 * @copyright Copyright (c) 2026 Embedead-Inside
 * @license SPDX-License-Identifier: MIT-0
 *
 * @details
 * 32ビット組み込みマイコンのレジスタ構造とコンパイラのループ展開特性を最大限に活かすために設計された、
 * 超軽量・超高速・ゼロ分岐のデータ圧縮・展開コーデックです。
 */

#include "rice_k3_b64.h"

/**
 * @brief 【一括エンコード】64バイトの入力データをライス符号化(k=3)で圧縮します。
 *
 * @details
 * 内部で「直前値との差分抽出（予測残差）」および「ZigZag符号化（正の整数への写像）」
 * の前処理をシームレスに行い、ライス符号化を適用します。
 * ループや条件分岐(if文)をアセンブラレベルで完全に排除し、すべて64ビットレジスタ上の
 * シフト演算とビットマージのみで実行されるため、最速のクロック数で直線実行されます。
 *
 * @note
 * 出力バッファ(@p out_buf)は、ビット論理和(OR)によるマージを行うため、
 * 呼び出し前に必ず**一括ゼロクリア(0初期化)**されている必要があります。
 *
 * @param[in]  in_buf   圧縮対象である生の8ビットデータ列のポインタ（要素数は必ず RICE_K3_B64_DATA_SIZE）
 * @param[out] out_buf  圧縮されたビットストリームを書き込むバッファのポインタ（最悪値に備え最大320バイト推奨）
 * @return uint32_t     実際に out_buf へ書き込まれた総出力バイト数（端数ビットは1バイトに切り上げ）
 */
uint32_t rice_k3_b64_encode(const uint8_t *in_buf, uint8_t *out_buf) {
    uint8_t *ptr = out_buf;
    uint8_t  bits = 0;
    uint8_t  prev = 0;

    /* コンパイラによる完全なループ展開(アンロール)を期待する固定数ループ */
    for (uint32_t i = 0; i < RICE_K3_B64_DATA_SIZE; i++) {
        /* 差分符号化（予測残差の抽出） */
        int16_t diff = (int16_t)in_buf[i] - (int16_t)prev;
        prev = in_buf[i];

        /* ZigZag符号化（符号付き整数を0付近の正の整数Nへ写像） */
        uint32_t n = (uint32_t)((diff << 1) ^ (diff >> 15)) & 0xFF;

        /* ライス符号化 (k=3 固定) */
        uint32_t q = n >> 3;
        uint32_t r = n & 0x07;

        uint64_t len  = q + 4;
        uint64_t code = ((((uint64_t)1 << q) - 1) << 4) | 0x08 | r;

        uint32_t total_bits = bits + len;

        /* 64ビットレジスタ空間を用いた一括ORマージ（分岐なし） */
        uint64_t window = 0;
        window |= (uint64_t)ptr[0] << 32;
        window |= (uint64_t)ptr[1] << 24;
        window |= (uint64_t)ptr[2] << 16;
        window |= (uint64_t)ptr[3] << 8;
        window |= (uint64_t)ptr[4];
        
        window |= code << (40 - total_bits);

        ptr[0] = (uint8_t)(window >> 32);
        ptr[1] = (uint8_t)(window >> 24);
        ptr[2] = (uint8_t)(window >> 16);
        ptr[3] = (uint8_t)(window >> 8);
        ptr[4] = (uint8_t)(window);

        /* ポインタと充填ビット位置の算術更新（分岐なし） */
        ptr  += total_bits >> 3;
        bits  = total_bits & 7;
    }

    uint32_t out_len = (uint32_t)(ptr - out_buf);
    if (bits > 0) {
        out_len++;
    }
    return out_len;
}

/**
 * @brief 【一括デコード】圧縮されたビットストリームから64バイトの生データを復元します。
 *
 * @details
 * エンコード処理の完全な逆演算を行います。ビットストリームから連続する「1」の個数（商q）
 * を数える際、コンパイラにハードウェアの高速ビット探索命令（CLZ: Count Leading Zeros）
 * を1クロックで生成させるため、GCC/Clangの組み込み関数(__builtin_clz)を利用しています。
 * エンコード同様、関数内部に条件分岐や可変ループは一切含まれません。
 *
 * @param[in]  in_buf   圧縮されたビットストリームの入力ポインタ
 * @param[out] out_buf  展開された生の8ビットデータを書き込むバッファのポインタ（要素数は必ず RICE_K3_B64_DATA_SIZE）
 * @return uint32_t     このブロックの展開にあたって、in_buf から消費（読込）された総入力バイト数
 */
uint32_t rice_k3_b64_decode(const uint8_t *in_buf, uint8_t *out_buf) {
    const uint8_t *ptr = in_buf;
    uint8_t        bits = 0;
    uint8_t        prev = 0;

    /* コンパイラによる完全なループ展開(アンロール)を期待する固定数ループ */
    for (uint32_t i = 0; i < RICE_K3_B64_DATA_SIZE; i++) {
        /* 最大5バイトのデータをレジスタのウインドウに引き出す */
        uint64_t window = 0;
        window |= (uint64_t)ptr[0] << 32;
        window |= (uint64_t)ptr[1] << 24;
        window |= (uint64_t)ptr[2] << 16;
        window |= (uint64_t)ptr[3] << 8;
        window |= (uint64_t)ptr[4];

        /* 既読ビットを破棄し、現在の符号頭を合わせる */
        window <<= bits;
        
        /* 商 q を一発特定（~window によりCLZ高速命令をコンパイラに誘発させる） */
        uint32_t temp = (uint32_t)(~window >> 8);
        uint32_t q = __builtin_clz(temp);
        
        /* 余り r をマスク抽出 */
        uint32_t r_shift = 32 - q - 1 - 3;
        uint32_t r = (temp >> r_shift) & 0x07;

        /* 正の整数 N を復元 */
        uint32_t n = (q << 3) | r;

        /* ZigZag逆変換を行い符号付き差分を復元 */
        int16_t diff = (int16_t)((n >> 1) ^ -(int16_t)(n & 1));

        /* 直前値を加算し、元の生の8ビットデータを復元 */
        out_buf[i] = (uint8_t)((int16_t)prev + diff);
        prev = out_buf[i];

        /* ストリーム読込位置の算術更新（分岐なし） */
        uint32_t total_bits = bits + q + 4;
        ptr  += total_bits >> 3;
        bits  = total_bits & 7;
    }

    uint32_t in_bytes_consumed = (uint32_t)(ptr - in_buf);
    if (bits > 0) {
        in_bytes_consumed++;
    }
    return in_bytes_consumed;
}
