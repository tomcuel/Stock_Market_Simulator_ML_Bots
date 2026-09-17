#!/usr/bin/env python3
"""
Prints "action_id:price" pairs (one per line) for every action currently in the exchange's database, 
using each action's most recent price in the `prices` table as the reference price 

Used by launch_multi_client_test.sh's --use_real_prices mode to feed 
generate_client_orders.py's --action-prices flag with the real tickers/prices Data/enter_in_database.py just loaded, 

Usage:
    python3 query_action_prices.py --db ../../Data/Stock_Market_App.db
"""
from __future__ import annotations

import argparse
import sqlite3
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--db", type=Path, required=True, help="path to the SQLite database")
    args = parser.parse_args()

    conn = sqlite3.connect(args.db)
    try:
        rows = conn.execute(
            """
            SELECT action_id, price FROM (
                SELECT action_id, price, ROW_NUMBER() 
                    OVER (PARTITION BY action_id ORDER BY date_time DESC, daily_time DESC) AS row_num 
                FROM prices
            )
            WHERE row_num = 1 ORDER BY action_id
            """
        ).fetchall()
    finally:
        conn.close()

    for action_id, price in rows:
        print(f"{action_id}:{price}")


if __name__ == "__main__":
    main()
