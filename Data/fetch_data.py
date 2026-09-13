#!/usr/bin/env python3
"""
Downloads historical OHLCV data from Yahoo Finance for a list of tickers and writes it out in the same long format already used by Datasets/Dataset_By_Year(Smaller)/*.csv:
    Ticker,Date,Open,High,Low,Close,Adj Close,Volume
so that preprocess.py can treat freshly-fetched data and the pre-existing historical CSVs uniformly, without caring which one produced a given row

Usage:
    python3 fetch_data.py --full-tickers --start 2015-01-01 --end 2024-12-31 --output test
    python3 fetch_data.py --tickers AAPL MSFT GOOG --start 2015-01-01 --end 2024-12-31
    python3 fetch_data.py --tickers-file tickers.txt --period 5y --interval 1d
    python3 fetch_data.py --index-tickers   # reuse the ^GSPC / ^IXIC / ... index tickers already present in Stock_Market_Initial_Data.csv

Output:
    Datasets/raw/<run_timestamp>_fetched_data.csv
"""
from __future__ import annotations

import argparse
import datetime as dt
import sys
from pathlib import Path

import pandas as pd

# tickers already referenced in Datasets/Stock_Market_Initial_Data.csv -- convenient default set to
# refresh/extend that dataset with the same instruments
DEFAULT_INDEX_TICKERS = [
    # US
    "^GSPC",           # S&P 500
    "^IXIC",           # NASDAQ Composite
    "^DJI",            # Dow Jones Industrial Average
    "^NYA",            # NYSE Composite
    "^XAX",            # NYSE AMEX Composite
    "^RUT",            # Russell 2000
    "^VIX",            # CBOE Volatility Index

    # Europe
    "^FTSE",           # FTSE 100
    "^GDAXI",          # DAX
    "^FCHI",           # CAC 40
    "^STOXX50E",       # Euro Stoxx 50
    "^N100",           # Euronext 100
    "^SSMI",           # Swiss Market Index
    "^IBEX",           # IBEX 35
    "^AEX",            # AEX Netherlands

    # Asia
    "^N225",            # Nikkei
    "^HSI",             # Hang Seng
    "^KS11",            # KOSPI South Korea
    "^TWII",            # Taiwan
    "^STI",             # Singapore
    "^JKSE",            # Indonesia
    "^KLSE",            # Malaysia
    "^NSEI",            # Nifty 50 India
]
SMALL_DEFAULT_TICKERS = ["AAPL", "MSFT", "GOOG", "AMZN", "TSLA", "META", "NVDA", "BRK-B", "JPM", "JNJ"]  # top 10 US stocks by market cap as of 2024-06-01

DATA_DIR = Path(__file__).resolve().parent
RAW_DIR = DATA_DIR / "Datasets" / "raw"


def _import_yfinance():
    try:
        import yfinance as yf  # noqa: local import kept lazy so --help works without the dependency
    except ModuleNotFoundError as exc:
        raise SystemExit("The 'yfinance' package is required to fetch data but is not installed.\n Install it with: pip install yfinance") from exc
    return yf


def fetch_tickers(tickers: list[str], start: str | None, end: str | None, period: str | None, interval: str) -> pd.DataFrame:
    """
    Downloads OHLCV history for every ticker and returns one long-format DataFrame
    """
    yf = _import_yfinance()

    frames = []
    for ticker in tickers:
        print(f"Fetching {ticker} ...", file=sys.stderr)
        try:
            history = yf.Ticker(ticker).history(start=start, end=end, period=period, interval=interval, auto_adjust=False)
        except Exception as exc:  # yfinance raises a variety of exception types per failure mode
            print(f"  -> failed to fetch {ticker}: {exc}", file=sys.stderr)
            continue

        if history.empty:
            print(f"  -> no data returned for {ticker}, skipping", file=sys.stderr)
            continue

        history = history.reset_index()
        history["Ticker"] = ticker
        # yfinance's `Adj Close` column is sometimes just `Close` depending on auto_adjust/version, normalize defensively so downstream code can always rely on both columns existing
        if "Adj Close" not in history.columns:
            history["Adj Close"] = history["Close"]
        frames.append(history[["Ticker", "Date", "Open", "High", "Low", "Close", "Adj Close", "Volume"]])

    if not frames:
        raise SystemExit("No data was fetched for any ticker; aborting.")

    combined = pd.concat(frames, ignore_index=True)
    combined["Date"] = pd.to_datetime(combined["Date"], utc=True).dt.tz_localize(None)
    return combined


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ticker_group = parser.add_mutually_exclusive_group(required=True)
    ticker_group.add_argument("--tickers", nargs="+", help="explicit list of ticker symbols to fetch")
    ticker_group.add_argument("--tickers-file", type=Path, help="path to a text file, one ticker per line")
    ticker_group.add_argument("--small-tickers", action="store_true", help="fetch a small set of top 10 US stocks by market cap")
    ticker_group.add_argument("--full-tickers", action="store_true", help="fetch the full set of main world index tickers")

    date_group = parser.add_mutually_exclusive_group()
    date_group.add_argument("--start", help="start date, YYYY-MM-DD (mutually exclusive with --period)")
    parser.add_argument("--end", help="end date, YYYY-MM-DD (only used with --start)")
    date_group.add_argument("--period", choices=["1d", "5d", "1mo", "3mo", "6mo", "1y", "2y", "5y", "10y", "ytd", "max"], help="yfinance relative period, e.g. '5y', '1mo', 'max' (mutually exclusive with --start/--end)")
    parser.add_argument("--interval", default="1d", choices=["1d", "1h", "1wk", "1mo"], help="bar interval: 1d, 1h, 1wk, ... (default: 1d)")
    parser.add_argument("--output", type=str, default=None, help="output CSV path (default: Datasets/raw/<timestamp>_fetched_data.csv)")
    args = parser.parse_args()

    if args.tickers:
        tickers = args.tickers
    elif args.tickers_file:
        tickers = [line.strip() for line in args.tickers_file.read_text().splitlines() if line.strip()]
    elif args.small_tickers:
        tickers = SMALL_DEFAULT_TICKERS
    elif args.full_tickers:
        tickers = DEFAULT_INDEX_TICKERS

    combined = fetch_tickers(tickers, args.start, args.end, args.period, args.interval)

    RAW_DIR.mkdir(parents=True, exist_ok=True)
    output_path = (RAW_DIR / f"{args.output}_fetched_data.csv") if args.output else (RAW_DIR / f"{dt.datetime.now():%Y%m%dT%H%M%S}_fetched_data.csv")
    combined.to_csv(output_path, index=False)
    print(f"Wrote {len(combined)} rows for {combined['Ticker'].nunique()} tickers to {output_path}")


if __name__ == "__main__":
    main()
