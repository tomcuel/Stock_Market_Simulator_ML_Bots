#!/usr/bin/env python3
"""
Normalizes every raw price CSV under Datasets/
- Legacy wide-format Stock_Market_Initial_Data.csv
- Long-format Dataset_By_Year(Smaller)/*.csv files
- Anything freshly written by fetch_data.py into Datasets/raw/) into a single canonical long-format table:
    ticker, date, open, high, low, close, adj_close, volume

and writes it to Datasets/processed/all_prices.csv, sorted by (ticker, date) with duplicates removed and missing values handled. 
This is the single file every downstream step (feature engineering, database loading) should read from

Usage:
    python3 preprocess.py
    python3 preprocess.py --keep_only_fetched --input test --min-rows 10 --max-rows 100   # keep only fetched data and drop tickers with too little history to be useful
"""
from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import pandas as pd

DATA_DIR = Path(__file__).resolve().parent
DATASETS_DIR = DATA_DIR / "Datasets"
PROCESSED_DIR = DATASETS_DIR / "processed"

CANONICAL_COLUMNS = ["ticker", "date", "open", "high", "low", "close", "adj_close", "volume"]


def _load_wide_format(path: Path) -> pd.DataFrame:
    """
    Parses Stock_Market_Initial_Data.csv: a wide CSV where each ticker occupies its own block of 6 columns (High, Low, Open, Close, Volume, Adj Close), 
    with the ticker name written once in the very first header row above that block
    """
    raw = pd.read_csv(path, header=None, low_memory=False)
    ticker_row = raw.iloc[0]
    field_row = raw.iloc[1]
    body = raw.iloc[2:].reset_index(drop=True)
    body.columns = range(body.shape[1])

    # forward-fill the ticker header across its 6-column block (only the first column of each block
    # actually names the ticker in this file's layout)
    tickers = ticker_row.ffill()

    frames = []
    date_col = body[0]
    for col_index in range(1, body.shape[1]):
        field_name = str(field_row[col_index]).strip()
        if field_name not in {"Open", "High", "Low", "Close", "Adj Close", "Volume"}:
            continue
        ticker = str(tickers[col_index]).strip()
        if not ticker or ticker.lower() == "nan":
            continue
        frames.append(pd.DataFrame({"ticker": ticker, "date": date_col, "field": field_name, "value": pd.to_numeric(body[col_index], errors="coerce")}))

    if not frames:
        return pd.DataFrame(columns=CANONICAL_COLUMNS)

    long_df = pd.concat(frames, ignore_index=True)
    pivoted = long_df.pivot_table(index=["ticker", "date"], columns="field", values="value", aggfunc="first")
    pivoted = pivoted.reset_index().rename(columns={"Open": "open", "High": "high", "Low": "low", "Close": "close", "Adj Close": "adj_close", "Volume": "volume"})
    for col in CANONICAL_COLUMNS:
        if col not in pivoted.columns:
            pivoted[col] = np.nan
    return pivoted[CANONICAL_COLUMNS]


def _load_long_format(path: Path) -> pd.DataFrame:
    """
    Parses the Dataset_By_Year(Smaller)/*.csv files and anything fetch_data.py writes: 
    both already use the columns Ticker,Date,Open,High,Low,Close,Adj Close,Volume
    """
    df = pd.read_csv(path, low_memory=False)
    df = df.rename(columns={"Ticker": "ticker", "Date": "date", "Open": "open", "High": "high", "Low": "low", "Close": "close", "Adj Close": "adj_close", "Volume": "volume",})
    missing = [col for col in CANONICAL_COLUMNS if col not in df.columns]
    if missing:
        raise ValueError(f"{path} is missing expected columns: {missing}")
    return df[CANONICAL_COLUMNS]


def load_all_raw_sources(keep_only_fetched: bool, input: str | None) -> pd.DataFrame:
    """
    Loads every raw CSV under Datasets/ into a single long-format DataFrame, ready for cleaning and normalization
    If keep_only_fetched is True, only the CSVs freshly written by fetch_data.py are loaded, ignoring legacy historical CSVs
    If input is provided, only that single CSV is loaded from the fetched only `raw` directory, ignoring all other sources
    """
    frames = []

    if not keep_only_fetched:
        wide_path = DATASETS_DIR / "Stock_Market_Initial_Data.csv"
        if wide_path.exists():
            print(f"Loading (wide format) {wide_path}")
            frames.append(_load_wide_format(wide_path))

        year_dir = DATASETS_DIR / "Dataset_By_Year(Smaller)"
        for path in sorted(year_dir.glob("*.csv")) if year_dir.exists() else []:
            print(f"Loading (long format) {path}")
            frames.append(_load_long_format(path))

    if input:
        raw_dir = DATASETS_DIR / "raw"
        path = raw_dir / f"{input}_fetched_data.csv"
        if path.exists():
            print(f"Loading (long format, fetched) {path}")
            frames.append(_load_long_format(path))
    else:
        raw_dir = DATASETS_DIR / "raw"
        for path in sorted(raw_dir.glob("*.csv")) if raw_dir.exists() else []:
            print(f"Loading (long format, fetched) {path}")
            frames.append(_load_long_format(path))

    if not frames:
        raise SystemExit(f"No source CSVs found under {DATASETS_DIR}. Run fetch_data.py first.")

    return pd.concat(frames, ignore_index=True)



def clean(df: pd.DataFrame, min_rows: int, max_rows: int) -> pd.DataFrame:
    df = df.copy()
    df["ticker"] = df["ticker"].astype(str).str.strip()
    df["date"] = pd.to_datetime(df["date"], errors="coerce")
    df = df.dropna(subset=["ticker", "date"])

    for col in ["open", "high", "low", "close", "adj_close", "volume"]:
        df[col] = pd.to_numeric(df[col], errors="coerce")

    # a row with no close price at all is useless: drop it outright
    df = df.dropna(subset=["close"])

    # de-duplicate: keep the last row seen for a given (ticker, date), so a re-fetch overwrites stale figures for the same day rather than creating a duplicate
    df = df.sort_values(["ticker", "date"]).drop_duplicates(subset=["ticker", "date"], keep="last")

    # within each ticker's own timeline, forward-fill small gaps (e.g. a single missing Adj Close) but never fill across ticker boundaries
    df[["open", "high", "low", "adj_close", "volume"]] = df.groupby("ticker")[["open", "high", "low", "adj_close", "volume"]].ffill()
    
    # anything still missing after a per-ticker forward-fill (e.g. the very first row of a ticker's history) falls back to the close price for OHLC columns, and 0 for volume
    for col in ["open", "high", "low", "adj_close"]:
        df[col] = df[col].fillna(df["close"])
    df["volume"] = df["volume"].fillna(0)

    # drop tickers with too little history to be useful downstream (feature engineering, seeding a believable reference price / volatility for a simulated market)
    counts = df.groupby("ticker")["date"].transform("count")
    df = df[(counts >= min_rows)]

    df = df.sort_values(["ticker", "date"]).reset_index(drop=True)
    # keep only the max_rows most recent rows for each ticker, to avoid overwhelming the database with decades of history for a few tickers that have been around forever
    df = df.groupby("ticker").tail(max_rows).reset_index(drop=True)
    return df[CANONICAL_COLUMNS]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--keep_only_fetched", action="store_true", help="only process the CSVs freshly fetched by fetch_data.py, ignore legacy historical CSVs")
    parser.add_argument("--min-rows", type=int, default=30, help="drop tickers with fewer than this many rows (default: 30)")
    parser.add_argument("--max-rows", type=int, default=100, help="drop tickers with more than this many rows (default: 100)")
    parser.add_argument("--input", type=str, default=None, help="only process this single CSV from the fetched-only `raw` directory, ignore all other sources")
    parser.add_argument("--output", type=str, default=None, help="Name of the output CSV file (default: `all_prices`, save to Datasets/processed/all_prices.csv)")
    args = parser.parse_args()

    raw = load_all_raw_sources(args.keep_only_fetched, args.input)
    cleaned = clean(raw, args.min_rows, args.max_rows)

    PROCESSED_DIR.mkdir(parents=True, exist_ok=True)
    output_path = PROCESSED_DIR / f"{args.output}.csv" if args.output else PROCESSED_DIR / "all_prices.csv"
    cleaned.to_csv(output_path, index=False)

    print(f"Wrote {len(cleaned)} rows across {cleaned['ticker'].nunique()} tickers to {output_path}")


if __name__ == "__main__":
    main()
