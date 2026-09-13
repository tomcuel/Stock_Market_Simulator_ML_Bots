#!/usr/bin/env python3
"""
Reads Datasets/processed/all_prices.csv (produced by preprocess.py) and computes, per ticker:
    daily_return, log_return, sma_20, sma_50, ema_20, rolling_volatility_20 (annualized), rsi_14, latest_price

Two things downstream consumers care about most:
  - `latest_price` + `rolling_volatility_20` are exactly what you'd hand to Src_Simulation.SymbolConfig (as `initial_price`) and to a bot's random price-offset distribution, 
    so simulated symbols start from and move around realistic real-world values  instead of arbitrary numbers
    --> extensions will surely come later when the Src_Simulation will be closer to a production-ready level
  - the full per-day feature table is what enter_in_database.py loads into the `prices` table of the SQL exchange's database, 
    so the market can be seeded with real historical price history instead of a single starting price

Usage:
    python3 feature_engineering.py
    python3 feature_engineering.py --input Datasets/processed/all_prices.csv
"""
from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import pandas as pd

DATA_DIR = Path(__file__).resolve().parent
PROCESSED_DIR = DATA_DIR / "Datasets" / "processed"

TRADING_DAYS_PER_YEAR = 252


def _rsi(close: pd.Series, window: int = 14) -> pd.Series:
    delta = close.diff()
    gain = delta.clip(lower=0.0)
    loss = -delta.clip(upper=0.0)
    # Wilder's smoothing (equivalent to an EMA with alpha = 1/window), the standard RSI definition
    avg_gain = gain.ewm(alpha=1.0 / window, min_periods=window, adjust=False).mean()
    avg_loss = loss.ewm(alpha=1.0 / window, min_periods=window, adjust=False).mean()
    rs = avg_gain / avg_loss.replace(0.0, np.nan)
    rsi = 100.0 - (100.0 / (1.0 + rs))
    # an avg_loss of exactly 0 (a pure uptrend window) means RSI should read 100, not NaN
    rsi = rsi.where(avg_loss != 0.0, 100.0)
    return rsi


def compute_features(prices: pd.DataFrame) -> pd.DataFrame:
    prices = prices.sort_values(["ticker", "date"]).copy()

    def per_ticker(group: pd.DataFrame) -> pd.DataFrame:
        group = group.copy()
        close = group["close"]

        group["daily_return"] = close.pct_change()
        group["log_return"] = np.log(close / close.shift(1))
        group["sma_20"] = close.rolling(window=20, min_periods=1).mean()
        group["sma_50"] = close.rolling(window=50, min_periods=1).mean()
        group["ema_20"] = close.ewm(span=20, adjust=False, min_periods=1).mean()
        group["rolling_volatility_20"] = group["log_return"].rolling(window=20, min_periods=5).std() * np.sqrt(TRADING_DAYS_PER_YEAR)
        group["rsi_14"] = _rsi(close, window=14)
        return group

    features = prices.groupby("ticker", group_keys=False).apply(per_ticker, include_groups=False)
    features.insert(0, "ticker", prices["ticker"].values)
    return features


def summarize_latest(features: pd.DataFrame) -> pd.DataFrame:
    """
    One row per ticker: the most recent snapshot, useful for seeding Src_Simulation symbols
    """
    latest = features.sort_values(["ticker", "date"]).groupby("ticker").tail(1).reset_index(drop=True)
    return latest[["ticker", "date", "close", "rolling_volatility_20", "rsi_14", "sma_20", "sma_50"]].rename(columns={"close": "latest_price"})


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--input", type=Path, default=PROCESSED_DIR / "all_prices.csv")
    parser.add_argument("--output", type=Path, default=PROCESSED_DIR / "features.csv")
    parser.add_argument("--summary-output", type=Path, default=PROCESSED_DIR / "latest_snapshot.csv")
    args = parser.parse_args()
    if not args.input.exists():
        raise SystemExit(f"{args.input} not found. Run preprocess.py first.")

    prices = pd.read_csv(args.input, parse_dates=["date"])
    features = compute_features(prices)

    PROCESSED_DIR.mkdir(parents=True, exist_ok=True)
    features.to_csv(args.output, index=False)
    print(f"Wrote {len(features)} rows of features to {args.output}")

    summary = summarize_latest(features)
    summary.to_csv(args.summary_output, index=False)
    print(f"Wrote latest per-ticker snapshot ({len(summary)} tickers) to {args.summary_output}")


if __name__ == "__main__":
    main()
