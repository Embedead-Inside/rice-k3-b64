/**
 * @file rice_k3_b64.h
 * @brief 極限最適化版 ゴロム・ライス符号化 (k=3, 64バイト固定) 独自のヘッダファイル
 * @copyright Copyright (c) 2026 Embedead-Inside
 * @license SPDX-License-Identifier: MIT-0
 *
 * @details
 * 32ビット組み込みマイコンのレジスタ構造とコンパイラのループ展開特性を最大限に活かすために設計された、
 * 超軽量・超高速・ゼロ分岐のデータ圧縮・展開コーデックです。
 */

#ifndef RICE_K3_B64_H
#define RICE_K3_B64_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 1ブロックあたりの固定入力データ数（バイト数）
 */
#define RICE_K3_B64_DATA_SIZE 64

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
uint32_t rice_k3_b64_encode(const uint8_t *in_buf, uint8_t *out_buf);

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
uint32_t rice_k3_b64_decode(const uint8_t *in_buf, uint8_t *out_buf);

#ifdef __cplusplus
}
#endif

#endif /* RICE_K3_B64_H */
