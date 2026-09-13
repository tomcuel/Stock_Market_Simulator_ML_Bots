#!/usr/bin/env python3
"""
Loads Datasets/processed/all_prices.csv (produced by preprocess.py) into the same SQLite database Src_SQL's C++ server reads/writes (Stock_Market_App.db by default): 
one row per ticker into the `actions` table, and its full daily close-price history into the `prices` table

Does NOT touch the `clients` table -- seeding demo clients is `server.x init`'s job (see Src_SQL/server.cpp and Src_SQL/scripts/launch_multi_client_test.sh)
Only ever adds actions/prices; it never deletes existing rows, so it's safe to re-run to add new tickers or extend an existing ticker's price history

Schema assumptions (see Src_SQL/database_management.cpp:create_tables), kept in sync by hand since this script has no C++ dependency:
    actions(action_id INTEGER PRIMARY KEY, name TEXT NOT NULL, quantity INTEGER NOT NULL)
    prices(price_id INTEGER PRIMARY KEY, action_id INTEGER NOT NULL, price REAL NOT NULL, date_time INTEGER NOT NULL, daily_time INTEGER NOT NULL)

`date_time`/`daily_time` are NOT Unix timestamps: they are the custom integer encoding produced by Src_SQL/utility.cpp's get_date_time()/get_daily_time(), 
built from a `struct tm` the same way the live C++ server encodes its own timestamps (using tm_year, i.e. year-1900, tm_mon 0-indexed, and D_IN_M=31 / M_IN_Y=12 as fixed-radix multipliers 
We replicate that exact formula in `encode_date_time`/`encode_daily_time` below so that historical rows inserted by this script sort correctly against rows the live server produces going forward

Usage:
    python3 enter_in_database.py
    python3 enter_in_database.py --db Stock_Market_App.db --input Datasets/processed/all_prices.csv
    python3 enter_in_database.py --tickers AAPL MSFT   # only load a subset of tickers
"""
from __future__ import annotations

import argparse
import sqlite3
from pathlib import Path

import pandas as pd

DATA_DIR = Path(__file__).resolve().parent
DEFAULT_DB_PATH = DATA_DIR / "Stock_Market_App.db"
DEFAULT_INPUT_PATH = DATA_DIR / "Datasets" / "processed" / "all_prices.csv"

# must stay identical to the #define constants in Src_SQL/utility.hpp
D_IN_M = 31
M_IN_Y = 12
MS_IN_S = 1000
S_IN_M = 60
M_IN_H = 60


def encode_date_time(timestamp: pd.Timestamp) -> int:
    """
    Mirrors Src_SQL/utility.cpp::get_date_time(): mday + D_IN_M * (mon0 + M_IN_Y * (year - 1900))
    """
    tm_year = timestamp.year - 1900
    tm_mon = timestamp.month - 1  # C's tm_mon is 0-indexed
    return timestamp.day + D_IN_M * (tm_mon + M_IN_Y * tm_year)


def encode_daily_time(hour: int = 0, minute: int = 0, second: int = 0, millisecond: int = 0) -> int:
    """
    Mirrors Src_SQL/utility.cpp::get_daily_time(). Daily bars carry no intraday timestamp, so every historical row is encoded as midnight by default
    """
    return millisecond + MS_IN_S * (second + S_IN_M * (minute + M_IN_H * hour))


def create_schema_if_missing(conn: sqlite3.Connection) -> None:
    """
    Creates the `actions` and `prices` tables if the database doesn't exist yet (e.g. this script is run before the C++ server has ever been launched)
    Kept byte-for-byte compatible with Src_SQL/database_management.cpp so the C++ server can open a database this script created
    """
    conn.executescript(
        """
        CREATE TABLE IF NOT EXISTS actions (
            action_id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            quantity INTEGER NOT NULL
        );
        CREATE TABLE IF NOT EXISTS prices (
            price_id INTEGER PRIMARY KEY,
            action_id INTEGER NOT NULL,
            price REAL NOT NULL,
            date_time INTEGER NOT NULL,
            daily_time INTEGER NOT NULL,
            FOREIGN KEY (action_id) REFERENCES actions(action_id)
        );
        """
    )
    conn.commit()


def get_or_create_action_id(conn: sqlite3.Connection, ticker: str, outstanding_quantity: int) -> int:
    cur = conn.execute("SELECT action_id FROM actions WHERE name = ?", (ticker,))
    row = cur.fetchone()
    if row is not None:
        return row[0]

    cur = conn.execute("SELECT COALESCE(MAX(action_id), 0) + 1 FROM actions")
    next_id = cur.fetchone()[0]
    conn.execute(
        "INSERT INTO actions (action_id, name, quantity) VALUES (?, ?, ?)",
        (next_id, ticker, outstanding_quantity),
    )
    return next_id


def get_next_price_id(conn: sqlite3.Connection) -> int:
    cur = conn.execute("SELECT COALESCE(MAX(price_id), 0) FROM prices")
    return cur.fetchone()[0] + 1


def load_ticker_history(conn: sqlite3.Connection, ticker: str, history: pd.DataFrame, outstanding_quantity: int) -> int:
    action_id = get_or_create_action_id(conn, ticker, outstanding_quantity)

    # avoid inserting duplicate history if this ticker was already loaded before: only insert rows for date_time values not already present for this action
    existing_dates = {row[0] for row in conn.execute("SELECT date_time FROM prices WHERE action_id = ?", (action_id,))}

    next_price_id = get_next_price_id(conn)
    rows_to_insert = []
    for _, row in history.sort_values("date").iterrows():
        date_time = encode_date_time(row["date"])
        if date_time in existing_dates:
            continue
        daily_time = encode_daily_time()
        price = float(row["adj_close"] if pd.notna(row["adj_close"]) else row["close"])
        rows_to_insert.append((next_price_id, action_id, price, date_time, daily_time))
        next_price_id += 1
        existing_dates.add(date_time)

    if rows_to_insert:
        conn.executemany(
            "INSERT INTO prices (price_id, action_id, price, date_time, daily_time) VALUES (?, ?, ?, ?, ?)",
            rows_to_insert,
        )

    return len(rows_to_insert)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--db", type=Path, default=DEFAULT_DB_PATH, help="path to the SQLite database (default: Data/Stock_Market_App.db)")
    parser.add_argument("--input", type=Path, default=DEFAULT_INPUT_PATH, help="processed prices CSV (default: Datasets/processed/all_prices.csv)")
    parser.add_argument("--tickers", nargs="+", default=None, help="only load these tickers (default: all tickers in the input file)")
    parser.add_argument("--outstanding-quantity", type=int, default=100_000, help="shares outstanding recorded for each newly created action (default: 100,000)")
    args = parser.parse_args()
    if not args.input.exists():
        raise SystemExit(f"{args.input} not found. Run preprocess.py first.")

    prices = pd.read_csv(args.input, parse_dates=["date"])
    if args.tickers:
        prices = prices[prices["ticker"].isin(args.tickers)]
        if prices.empty:
            raise SystemExit(f"None of the requested tickers {args.tickers} were found in {args.input}")

    conn = sqlite3.connect(args.db)
    try:
        create_schema_if_missing(conn)
        total_inserted = 0
        for ticker, history in prices.groupby("ticker"):
            inserted = load_ticker_history(conn, ticker, history, args.outstanding_quantity)
            print(f"{ticker}: inserted {inserted} new price rows ({len(history)} rows in source)")
            total_inserted += inserted
        conn.commit()
        print(f"\nDone. Inserted {total_inserted} new price rows across {prices['ticker'].nunique()} tickers into {args.db}")
    finally:
        conn.close()


if __name__ == "__main__":
    main()
