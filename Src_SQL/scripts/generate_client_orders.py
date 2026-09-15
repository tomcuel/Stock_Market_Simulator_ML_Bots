#!/usr/bin/env python3
"""
Generates a sequence of commands for one simulated client, in the exact format that
client_account.cpp reads line by line from stdin. Piping the output of this script into
client_account.x lets you drive a non-interactive "bot" client without touching a keyboard,
which is what launch_multi_client_test.sh uses to spin up many clients at once.

Order line format (see Src_SQL/server.cpp for the authoritative parser):
    [BUY/SELL] [quantity] [action_id] [MARKET/LIMIT/STOP/LIMIT_STOP] [price] [trigger_price_lower] [trigger_price_upper] [validity_date] [validity_time]

    - MARKET:     no price / trigger_price_lower / trigger_price_upper
    - LIMIT:      price + trigger_price_lower
    - STOP:       price + trigger_price_upper
    - LIMIT_STOP: price + trigger_price_lower + trigger_price_upper

Usage:
    python3 generate_client_orders.py --num-actions 5 --num-orders 20 --seed 1 > client1_commands.txt
    ./client_account.x Client1 123 < client1_commands.txt
"""
import argparse
import random
import sys


def build_order(rng: random.Random, num_actions: int, reference_prices: list[float]) -> str:
    action_id = rng.randint(1, num_actions)
    side = rng.choice(["BUY", "SELL"])
    quantity = rng.randint(1, 10)
    ref_price = reference_prices[action_id - 1]

    trigger = rng.choices(
        ["MARKET", "LIMIT", "STOP", "LIMIT_STOP"],
        weights=[0.5, 0.3, 0.1, 0.1],
        # weights=[1.0, 0.0, 0.0, 0.0],
    )[0]

    if trigger == "MARKET":
        return f"{side} {quantity} {action_id} MARKET"

    # random price offset around the reference price, kept positive
    price = max(0.5, round(ref_price * rng.uniform(0.85, 1.15), 2))

    if trigger == "LIMIT":
        trigger_price_lower = max(0.0, round(price * rng.uniform(0.9, 1.0), 2))
        return f"{side} {quantity} {action_id} LIMIT {price} {trigger_price_lower}"

    if trigger == "STOP":
        trigger_price_upper = round(price * rng.uniform(1.0, 1.1), 2)
        return f"{side} {quantity} {action_id} STOP {price} {trigger_price_upper}"

    # LIMIT_STOP
    trigger_price_lower = max(0.0, round(price * rng.uniform(0.9, 1.0), 2))
    trigger_price_upper = round(price * rng.uniform(1.0, 1.1), 2)
    return f"{side} {quantity} {action_id} LIMIT_STOP {price} {trigger_price_lower} {trigger_price_upper}"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--num-actions", type=int, default=2, help="number of actions registered on the server")
    parser.add_argument("--num-orders", type=int, default=10, help="number of orders this client will submit")
    parser.add_argument("--seed", type=int, default=None, help="RNG seed, for reproducible test runs")
    parser.add_argument("--reference-price", type=float, default=50.0, help="rough reference price used to generate plausible LIMIT/STOP prices (per action, +/-15%%)")
    parser.add_argument("--display-every", type=int, default=5, help="insert a `display market` command every N orders so the bot's log shows the book evolving")
    parser.add_argument("--exit-at-end", action="store_true",help="append an `exit` command at the end")
    args = parser.parse_args()

    rng = random.Random(args.seed)
    reference_prices = [round(args.reference_price * rng.uniform(0.5, 1.5), 2) for _ in range(args.num_actions)]

    lines = []
    for i in range(1, args.num_orders + 1):
        lines.append(build_order(rng, args.num_actions, reference_prices))
        if args.display_every > 0 and i % args.display_every == 0:
            lines.append("display market")

    if args.exit_at_end:
        lines.append("exit")

    sys.stdout.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
