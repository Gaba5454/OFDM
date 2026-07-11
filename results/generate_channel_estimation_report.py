#!/usr/bin/env python3

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import math
import numpy as np
import matplotlib.pyplot as plt


NFFT = 128
CP_LEN = 20
PILOT_INDEX = np.array([28, 38, 48, 58, 68, 78, 88, 98], dtype=int)
DATA_INDEX = np.array(
    [i for i in range(28, 64) if i not in PILOT_INDEX]
    + [i for i in range(65, 101) if i not in PILOT_INDEX],
    dtype=int,
)
ACTIVE_INDEX = np.sort(np.concatenate([PILOT_INDEX, DATA_INDEX]))
ACTIVE_FREQ = ACTIVE_INDEX - NFFT // 2
PILOT_POS_IN_ACTIVE = np.array([np.where(ACTIVE_INDEX == idx)[0][0] for idx in PILOT_INDEX], dtype=int)
DATA_POS_IN_ACTIVE = np.array([np.where(ACTIVE_INDEX == idx)[0][0] for idx in DATA_INDEX], dtype=int)

KNOWN_PILOT = (1.0 + 1.0j) / math.sqrt(2.0)
CHANNEL_DELAYS = np.array([0, 1, 3, 5, 8, 12], dtype=int)
CHANNEL_POWERS_DB = np.array([0.0, -1.5, -3.0, -5.0, -8.0, -10.0], dtype=float)
CHANNEL_POWERS = 10.0 ** (CHANNEL_POWERS_DB / 10.0)
CHANNEL_POWERS /= CHANNEL_POWERS.sum()

SNR_DB_LIST = np.array([0, 5, 10, 15, 20, 25, 30], dtype=float)
TRIALS = 500
EXAMPLE_SNR_DB = 15.0
REPORT_DIR = Path(__file__).resolve().parent
PLOTS_DIR = REPORT_DIR / "plots"

DISPLAY_NAMES = {
    "perfect": "Идеальный канал",
    "train_ls": "LS по обучающему символу",
    "pilot_linear": "Pilot-LS + линейная интерполяция",
    "pilot_lmmse": "Pilot-LMMSE",
    "train_dft": "DFT-LS",
    "semi_blind": "Decision-Directed",
}


@dataclass
class TrialData:
    h_true: np.ndarray
    h_train_ls: np.ndarray
    h_pilot_linear: np.ndarray
    h_pilot_lmmse: np.ndarray
    h_train_dft: np.ndarray
    h_semi_blind: np.ndarray
    ber: dict[str, float]


def make_training_symbols(count: int) -> np.ndarray:
    symbols = np.zeros(count, dtype=np.complex128)
    state = 0x5A17
    for i in range(count):
        state = (state * 1664525 + 1013904223) & 0xFFFFFFFF
        re = 1.0 if ((state >> 31) & 1) else -1.0
        state = (state * 1664525 + 1013904223) & 0xFFFFFFFF
        im = 1.0 if ((state >> 31) & 1) else -1.0
        symbols[i] = (re + 1j * im) / math.sqrt(2.0)
    return symbols


TRAINING_DATA = make_training_symbols(len(DATA_INDEX))


def qpsk_mod(bits: np.ndarray) -> np.ndarray:
    bit_pairs = bits.reshape(-1, 2)
    symbols = np.empty(bit_pairs.shape[0], dtype=np.complex128)
    for i, (b0, b1) in enumerate(bit_pairs):
        re = -1.0 if b0 == 0 else 1.0
        im = -1.0 if b1 == 0 else 1.0
        symbols[i] = (re + 1j * im) / math.sqrt(2.0)
    return symbols


def qpsk_demod(symbols: np.ndarray) -> np.ndarray:
    bits = np.empty(symbols.size * 2, dtype=np.uint8)
    bits[0::2] = (symbols.real >= 0.0).astype(np.uint8)
    bits[1::2] = (symbols.imag >= 0.0).astype(np.uint8)
    return bits


def build_full_symbol(data_symbols: np.ndarray) -> np.ndarray:
    full = np.zeros(NFFT, dtype=np.complex128)
    full[PILOT_INDEX] = KNOWN_PILOT
    full[DATA_INDEX] = data_symbols
    return full


def generate_channel(rng: np.random.Generator) -> tuple[np.ndarray, np.ndarray]:
    taps = (
        rng.normal(size=CHANNEL_DELAYS.size) + 1j * rng.normal(size=CHANNEL_DELAYS.size)
    ) / math.sqrt(2.0)
    taps *= np.sqrt(CHANNEL_POWERS)

    impulse = np.zeros(NFFT, dtype=np.complex128)
    impulse[CHANNEL_DELAYS] = taps
    h_centered = np.fft.fftshift(np.fft.fft(impulse, NFFT))
    return impulse, h_centered


def add_noise(signal: np.ndarray, snr_db: float, rng: np.random.Generator) -> tuple[np.ndarray, float]:
    snr_linear = 10.0 ** (snr_db / 10.0)
    noise_var = 1.0 / snr_linear
    noise = (
        rng.normal(size=signal.shape) + 1j * rng.normal(size=signal.shape)
    ) * math.sqrt(noise_var / 2.0)
    return signal + noise, noise_var


def interpolate_complex_linear(
    src_freq: np.ndarray,
    src_values: np.ndarray,
    dst_freq: np.ndarray,
) -> np.ndarray:
    re = np.interp(dst_freq, src_freq, src_values.real)
    im = np.interp(dst_freq, src_freq, src_values.imag)
    return re + 1j * im


def project_to_cp_support(h_active: np.ndarray, projector: np.ndarray) -> np.ndarray:
    taps = projector @ h_active
    return TIME_SUPPORT_MATRIX @ taps


def hard_equalize(y_data: np.ndarray, h_est_data: np.ndarray) -> np.ndarray:
    safe_h = np.where(np.abs(h_est_data) < 1e-9, 1.0 + 0.0j, h_est_data)
    return y_data / safe_h


def nmse(h_est: np.ndarray, h_true: np.ndarray) -> float:
    return float(np.mean(np.abs(h_est - h_true) ** 2) / np.mean(np.abs(h_true) ** 2))


def compute_active_correlation() -> np.ndarray:
    freq_delta = ACTIVE_FREQ[:, None] - ACTIVE_FREQ[None, :]
    kernel = np.zeros_like(freq_delta, dtype=np.complex128)
    for delay, power in zip(CHANNEL_DELAYS, CHANNEL_POWERS):
        kernel += power * np.exp(-1j * 2.0 * np.pi * freq_delta * delay / NFFT)
    return kernel


R_ACTIVE = compute_active_correlation()
TIME_SUPPORT_MATRIX = np.exp(
    -1j
    * 2.0
    * np.pi
    * ACTIVE_FREQ[:, None]
    * np.arange(CP_LEN, dtype=float)[None, :]
    / NFFT
)
TIME_SUPPORT_PROJECTOR = np.linalg.pinv(TIME_SUPPORT_MATRIX)


def make_lmmse_matrix(noise_var: float) -> np.ndarray:
    r_hp = R_ACTIVE[:, PILOT_POS_IN_ACTIVE]
    r_pp = R_ACTIVE[np.ix_(PILOT_POS_IN_ACTIVE, PILOT_POS_IN_ACTIVE)]
    return r_hp @ np.linalg.inv(r_pp + noise_var * np.eye(len(PILOT_INDEX)))


def run_single_trial(snr_db: float, rng: np.random.Generator, lmmse_w: np.ndarray) -> TrialData:
    _, h_full = generate_channel(rng)
    h_true = h_full[ACTIVE_INDEX]

    payload_bits = rng.integers(0, 2, size=len(DATA_INDEX) * 2, dtype=np.uint8)
    payload_data = qpsk_mod(payload_bits)
    payload_full = build_full_symbol(payload_data)
    training_full = build_full_symbol(TRAINING_DATA)

    y_train, noise_var = add_noise(h_full * training_full, snr_db, rng)
    y_payload, _ = add_noise(h_full * payload_full, snr_db, rng)

    h_train_ls = y_train[ACTIVE_INDEX] / training_full[ACTIVE_INDEX]

    h_pilot_ls = y_payload[PILOT_INDEX] / KNOWN_PILOT
    h_pilot_linear = interpolate_complex_linear(
        ACTIVE_FREQ[PILOT_POS_IN_ACTIVE],
        h_pilot_ls,
        ACTIVE_FREQ,
    )
    h_pilot_lmmse = lmmse_w @ h_pilot_ls

    h_train_dft = project_to_cp_support(h_train_ls, TIME_SUPPORT_PROJECTOR)

    y_payload_active = y_payload[ACTIVE_INDEX]
    y_payload_data = y_payload[DATA_INDEX]
    init_eq = hard_equalize(y_payload_data, h_pilot_lmmse[DATA_POS_IN_ACTIVE])
    init_bits = qpsk_demod(init_eq)
    init_decisions = qpsk_mod(init_bits)

    h_dd_active = np.empty_like(h_true)
    h_dd_active[PILOT_POS_IN_ACTIVE] = h_pilot_ls
    h_dd_active[DATA_POS_IN_ACTIVE] = y_payload_data / init_decisions
    h_semi_blind = project_to_cp_support(h_dd_active, TIME_SUPPORT_PROJECTOR)

    estimators = {
        "perfect": h_true,
        "train_ls": h_train_ls,
        "pilot_linear": h_pilot_linear,
        "pilot_lmmse": h_pilot_lmmse,
        "train_dft": h_train_dft,
        "semi_blind": h_semi_blind,
    }

    ber = {}
    for name, h_est in estimators.items():
        eq = hard_equalize(y_payload_data, h_est[DATA_POS_IN_ACTIVE])
        bits_hat = qpsk_demod(eq)
        ber[name] = float(np.mean(bits_hat != payload_bits))

    return TrialData(
        h_true=h_true,
        h_train_ls=h_train_ls,
        h_pilot_linear=h_pilot_linear,
        h_pilot_lmmse=h_pilot_lmmse,
        h_train_dft=h_train_dft,
        h_semi_blind=h_semi_blind,
        ber=ber,
    )


def run_monte_carlo() -> tuple[dict[str, np.ndarray], dict[str, np.ndarray], TrialData]:
    rng = np.random.default_rng(20260608)
    methods = ["train_ls", "pilot_linear", "pilot_lmmse", "train_dft", "semi_blind"]
    nmse_acc = {name: np.zeros_like(SNR_DB_LIST, dtype=float) for name in methods}
    ber_acc = {name: np.zeros_like(SNR_DB_LIST, dtype=float) for name in ["perfect"] + methods}
    example_trial = None

    for snr_idx, snr_db in enumerate(SNR_DB_LIST):
        lmmse_w = make_lmmse_matrix(1.0 / (10.0 ** (snr_db / 10.0)))
        for _ in range(TRIALS):
            trial = run_single_trial(snr_db, rng, lmmse_w)
            if example_trial is None and abs(snr_db - EXAMPLE_SNR_DB) < 1e-9:
                example_trial = trial

            nmse_acc["train_ls"][snr_idx] += nmse(trial.h_train_ls, trial.h_true)
            nmse_acc["pilot_linear"][snr_idx] += nmse(trial.h_pilot_linear, trial.h_true)
            nmse_acc["pilot_lmmse"][snr_idx] += nmse(trial.h_pilot_lmmse, trial.h_true)
            nmse_acc["train_dft"][snr_idx] += nmse(trial.h_train_dft, trial.h_true)
            nmse_acc["semi_blind"][snr_idx] += nmse(trial.h_semi_blind, trial.h_true)

            for name, value in trial.ber.items():
                ber_acc[name][snr_idx] += value

        for name in nmse_acc:
            nmse_acc[name][snr_idx] /= TRIALS
        for name in ber_acc:
            ber_acc[name][snr_idx] /= TRIALS

    if example_trial is None:
        example_trial = run_single_trial(EXAMPLE_SNR_DB, rng, make_lmmse_matrix(1.0 / (10.0 ** (EXAMPLE_SNR_DB / 10.0))))

    return nmse_acc, ber_acc, example_trial


def plot_nmse(nmse_curve: dict[str, np.ndarray]) -> Path:
    output = PLOTS_DIR / "channel_estimation_nmse.png"
    plt.figure(figsize=(9, 5))
    for name, values in nmse_curve.items():
        plt.semilogy(SNR_DB_LIST, values, marker="o", linewidth=2, label=DISPLAY_NAMES[name])
    plt.grid(True, which="both", linestyle="--", alpha=0.4)
    plt.xlabel("SNR, dB")
    plt.ylabel("NMSE")
    plt.title("NMSE оценки канала в зависимости от SNR")
    plt.legend()
    plt.tight_layout()
    plt.savefig(output, dpi=180)
    plt.close()
    return output


def plot_ber(ber_curve: dict[str, np.ndarray]) -> Path:
    output = PLOTS_DIR / "channel_estimation_ber.png"
    plt.figure(figsize=(9, 5))
    for name, values in ber_curve.items():
        plt.semilogy(SNR_DB_LIST, values, marker="o", linewidth=2, label=DISPLAY_NAMES[name])
    plt.grid(True, which="both", linestyle="--", alpha=0.4)
    plt.xlabel("SNR, dB")
    plt.ylabel("BER")
    plt.title("BER полезных данных после эквализации")
    plt.legend()
    plt.tight_layout()
    plt.savefig(output, dpi=180)
    plt.close()
    return output


def plot_example_channel(trial: TrialData) -> Path:
    output = PLOTS_DIR / "channel_estimation_example.png"
    x_axis = ACTIVE_FREQ
    plt.figure(figsize=(10, 7))

    plt.subplot(2, 1, 1)
    plt.plot(x_axis, np.abs(trial.h_true), linewidth=2, label=DISPLAY_NAMES["perfect"])
    plt.plot(x_axis, np.abs(trial.h_train_ls), "--", label=DISPLAY_NAMES["train_ls"])
    plt.plot(x_axis, np.abs(trial.h_pilot_linear), "--", label=DISPLAY_NAMES["pilot_linear"])
    plt.plot(x_axis, np.abs(trial.h_pilot_lmmse), "--", label=DISPLAY_NAMES["pilot_lmmse"])
    plt.plot(x_axis, np.abs(trial.h_train_dft), "--", label=DISPLAY_NAMES["train_dft"])
    plt.plot(x_axis, np.abs(trial.h_semi_blind), "--", label=DISPLAY_NAMES["semi_blind"])
    plt.grid(True, linestyle="--", alpha=0.4)
    plt.ylabel("|H[k]|")
    plt.title(f"Пример оценки канала при SNR = {EXAMPLE_SNR_DB:.0f} dB")
    plt.legend(ncol=3, fontsize=9)

    plt.subplot(2, 1, 2)
    plt.plot(x_axis, np.unwrap(np.angle(trial.h_true)), linewidth=2, label=DISPLAY_NAMES["perfect"])
    plt.plot(x_axis, np.unwrap(np.angle(trial.h_train_ls)), "--", label=DISPLAY_NAMES["train_ls"])
    plt.plot(x_axis, np.unwrap(np.angle(trial.h_pilot_linear)), "--", label=DISPLAY_NAMES["pilot_linear"])
    plt.plot(x_axis, np.unwrap(np.angle(trial.h_pilot_lmmse)), "--", label=DISPLAY_NAMES["pilot_lmmse"])
    plt.plot(x_axis, np.unwrap(np.angle(trial.h_train_dft)), "--", label=DISPLAY_NAMES["train_dft"])
    plt.plot(x_axis, np.unwrap(np.angle(trial.h_semi_blind)), "--", label=DISPLAY_NAMES["semi_blind"])
    plt.grid(True, linestyle="--", alpha=0.4)
    plt.xlabel("Центрированный индекс поднесущей")
    plt.ylabel("unwrap(angle(H[k]))")
    plt.tight_layout()
    plt.savefig(output, dpi=180)
    plt.close()
    return output


def make_summary_table(curves: dict[str, np.ndarray], snr_points: list[int]) -> str:
    header = "| Метод | " + " | ".join(f"{snr} dB" for snr in snr_points) + " |\n"
    header += "|---|" + "|".join("---:" for _ in snr_points) + "|\n"
    lines = [header]

    for name, values in curves.items():
        cols = []
        for snr in snr_points:
            idx = int(np.where(SNR_DB_LIST == snr)[0][0])
            cols.append(f"{values[idx]:.4e}")
        lines.append("| " + DISPLAY_NAMES[name] + " | " + " | ".join(cols) + " |\n")

    return "".join(lines)


def write_report(
    nmse_curve: dict[str, np.ndarray],
    ber_curve: dict[str, np.ndarray],
    nmse_plot: Path,
    ber_plot: Path,
    example_plot: Path,
) -> Path:
    output = REPORT_DIR / "channel_estimation_report.md"
    nmse_table = make_summary_table(nmse_curve, [10, 20, 30])
    ber_table = make_summary_table(ber_curve, [10, 20, 30])

    report = f"""# Отчёт по оценке канала в OFDM

## Цель

В этом отчёте сравниваются практические методы оценки канала для OFDM-кадра, который уже используется в проекте:

- размер БПФ: `128`
- длина циклического префикса: `20`
- пилотные поднесущие: `{PILOT_INDEX.tolist()}`
- число информационных поднесущих: `64`
- число активных поднесущих: `72`
- модуляция полезных данных: нормированная `QPSK`
- модель канала: рэлеевский многолучевой канал с задержками `{CHANNEL_DELAYS.tolist()}` и нормированными мощностями `{np.round(CHANNEL_POWERS, 4).tolist()}`
- число испытаний Монте-Карло на каждое значение `SNR`: `{TRIALS}`

Используется модель приёма

`Y[k] = H[k] * X[k] + W[k]`

где качество оценки измеряется по двум метрикам:

- `NMSE = E[|H_hat - H|^2] / E[|H|^2]`
- `BER` полезных данных после одноканальной эквализации.

## Реализованные методы оценки

### 1. LS по обучающему символу

Классическая оценка метода наименьших квадратов по полностью известному обучающему OFDM-символу:

`H_hat[k] = Y_train[k] / X_train[k]`

Это естественная базовая оценка для текущего приёмника, потому что в кадре уже есть отдельный обучающий символ перед полезными данными.

### 2. Pilot-LS + линейная интерполяция

Сначала выполняется LS-оценка только в пилотных позициях:

`H_ls[p] = Y_p[p] / X_p[p]`

затем полученная оценка линейно интерполируется по частоте на все активные поднесущие.

### 3. Pilot-LMMSE

Пилотная линейная оценка минимальной среднеквадратической ошибки:

`H_hat = R_hp (R_pp + sigma^2 I)^(-1) H_ls,p`

Здесь используются корреляционные матрицы канала, построенные по той же модели многолучевого канала, что и в эксперименте.

### 4. DFT-LS

Это стандартный вариант временной оценки канала, известный как `DFT-LS`:

1. сначала вычисляется LS-оценка на активных поднесущих,
2. затем оценка проектируется на импульсную характеристику канала, длина которой не превышает `CP`,
3. после этого короткая импульсная характеристика снова переводится в частотную область.

Такой подход подавляет шум вне допустимой области задержек и соответствует временной области представления из теории.

### 5. Decision-Directed

Полуслепая `decision-directed` оценка:

1. начальная оценка строится методом `Pilot-LMMSE`,
2. полезные данные эквализуются,
3. по ним принимаются жёсткие решения,
4. затем по восстановленным символам строится уточнённая оценка канала,
5. итоговая оценка сглаживается тем же методом `DFT-LS`.

Это не полностью слепая оценка, а именно полуслепой подход с опорой на пилоты и принятые решения.

## NMSE оценки канала

![NMSE](plots/{nmse_plot.name})

{nmse_table}

## BER после эквализации

![BER](plots/{ber_plot.name})

{ber_table}

## Пример одной реализации канала

Ниже показан пример истинной и оценённых характеристик канала для одной случайной реализации при `SNR = 15 dB`.

![Пример оценки канала](plots/{example_plot.name})

## Основные выводы

1. `Pilot-LS + линейная интерполяция` показывает худший результат в частотно-селективном канале, потому что восемь пилотов должны восстановить всю активную полосу, а шум LS-оценки напрямую переносится в интерполяцию.
2. `Pilot-LMMSE` заметно лучше обычного пилотного LS-метода, особенно при низком и среднем `SNR`, так как использует априорную корреляцию канала.
3. `LS по обучающему символу` уже даёт хороший базовый результат, потому что оценка строится сразу по всем активным поднесущим.
4. `DFT-LS` оказался лучшим практическим методом в этом эксперименте: ограничение длины импульсной характеристики эффективно подавляет шум вне области допустимых задержек.
5. `Decision-Directed` улучшает оценку по сравнению с чисто пилотными методами при среднем и высоком `SNR`, но на низком `SNR` может деградировать из-за ошибочных решений по полезным данным.

## Что имеет смысл перенести в основной приёмник

1. Сохранить текущий обучающий OFDM-символ, потому что он уже даёт сильную опорную оценку канала.
2. В качестве первого практического улучшения заменить текущую частотную LS-оценку на `DFT-LS`.
3. Оставить пилоты в полезной части кадра для последующей компенсации остаточной фазовой ошибки и `decision-directed` уточнения.
4. На следующем этапе сравнить те же методы уже в более нестационарном канале, где характеристики меняются быстрее во времени.

## Воспроизводимость

Этот отчёт и все графики автоматически генерируются скриптом:

`results/generate_channel_estimation_report.py`
"""

    output.write_text(report, encoding="utf-8")
    return output


def main() -> None:
    PLOTS_DIR.mkdir(parents=True, exist_ok=True)
    nmse_curve, ber_curve, example_trial = run_monte_carlo()
    nmse_plot = plot_nmse(nmse_curve)
    ber_plot = plot_ber(ber_curve)
    example_plot = plot_example_channel(example_trial)
    report_path = write_report(nmse_curve, ber_curve, nmse_plot, ber_plot, example_plot)
    print(f"Report written to: {report_path}")


if __name__ == "__main__":
    main()
