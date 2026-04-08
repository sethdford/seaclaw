#!/usr/bin/env python3
"""
Mine Apple Photos metadata to build a life timeline and recent activity tracker.

Reads ONLY metadata (GPS, dates, scene IDs) from Photos.sqlite.
Never touches actual image files. All output stays local.

Output:
  ~/.human/photos/life_timeline.json   — places lived, travel history, GPS clusters
  ~/.human/photos/recent_activity.json — last 30 days of photo locations/dates

Usage:
  python3 scripts/mine-photos.py
  python3 scripts/mine-photos.py --days 60    # recent activity window
  python3 scripts/mine-photos.py --no-geocode # skip reverse geocoding (use cached)
"""
from __future__ import annotations

import argparse
import json
import math
import os
import sqlite3
import sys
import time
from collections import defaultdict
from datetime import datetime, timedelta
from pathlib import Path

PHOTOS_DB = Path.home() / "Pictures" / "Photos Library.photoslibrary" / "database" / "Photos.sqlite"
OUTPUT_DIR = Path.home() / ".human" / "photos"
GEOCODE_CACHE = OUTPUT_DIR / "geocode_cache.json"

APPLE_EPOCH_OFFSET = 978307200

HOME_LOCATIONS = [
    {"name": "King of Prussia, PA", "lat": 40.09, "lon": -75.41, "radius_km": 15,
     "start": "2023-01", "current": True},
    {"name": "Raleigh, NC", "lat": 35.78, "lon": -78.64, "radius_km": 20,
     "start": "2018-01", "end": "2023-01"},
    {"name": "Salt Lake City, UT", "lat": 40.76, "lon": -111.89, "radius_km": 30,
     "start": "2000-01", "end": "2018-01"},
    {"name": "Miyako, Japan", "lat": 39.64, "lon": 141.96, "radius_km": 20,
     "start": "2010-01", "end": "2013-01"},
]

KNOWN_PLACES = {
    (40.6, -112.0): "Salt Lake City, UT",
    (40.5, -112.0): "South Jordan, UT",
    (40.5, -111.9): "Draper, UT",
    (40.6, -111.9): "Murray / Midvale, UT",
    (40.8, -111.9): "North Salt Lake, UT",
    (40.6, -111.6): "Park City area, UT",
    (40.7, -111.5): "Heber City area, UT",
    (35.7, -78.7): "Raleigh, NC",
    (35.8, -78.6): "Raleigh, NC",
    (40.1, -75.4): "King of Prussia, PA",
    (40.1, -75.5): "Malvern / Vanguard, PA",
    (39.6, 142.0): "Miyako, Iwate, Japan",
    (35.6, 139.8): "Tokyo, Japan",
    (35.7, 139.7): "Tokyo, Japan",
    (42.4, -71.1): "Boston, MA",
    (36.1, -115.2): "Las Vegas, NV",
    (47.7, -122.4): "Issaquah / Seattle, WA",
    (37.4, -122.1): "Palo Alto / Silicon Valley, CA",
    (35.5, -82.6): "Asheville, NC",
    (35.6, -82.2): "Black Mountain, NC",
    (42.9, -111.0): "Afton, WY",
    (37.1, -113.6): "St. George, UT",
    (37.2, -113.6): "St. George, UT",
    (28.5, -81.4): "Orlando, FL",
    (27.8, -82.6): "St. Petersburg, FL",
    (-33.9, 151.2): "Sydney, Australia",
    (-38.1, 145.1): "Phillip Island, Australia",
}


def haversine_km(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    R = 6371.0
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    a = math.sin(dlat / 2) ** 2 + math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) * math.sin(dlon / 2) ** 2
    return R * 2 * math.asin(math.sqrt(a))


def is_home_location(lat: float, lon: float, date_str: str) -> str | None:
    for home in HOME_LOCATIONS:
        dist = haversine_km(lat, lon, home["lat"], home["lon"])
        if dist <= home["radius_km"]:
            if home.get("current"):
                if date_str >= home["start"]:
                    return home["name"]
            elif home.get("start") and home.get("end"):
                if home["start"] <= date_str <= home["end"]:
                    return home["name"]
            elif date_str >= home.get("start", "1900"):
                return home["name"]
    return None


def load_geocode_cache() -> dict:
    if GEOCODE_CACHE.exists():
        with open(GEOCODE_CACHE) as f:
            return json.load(f)
    return {}


def save_geocode_cache(cache: dict):
    with open(GEOCODE_CACHE, "w") as f:
        json.dump(cache, f, indent=2)


def reverse_geocode_cluster(lat: float, lon: float, cache: dict, do_geocode: bool) -> str:
    key = f"{round(lat, 1)},{round(lon, 1)}"

    if key in cache:
        return cache[key]

    rounded = (round(lat, 1), round(lon, 1))
    if rounded in KNOWN_PLACES:
        name = KNOWN_PLACES[rounded]
        cache[key] = name
        return name

    if not do_geocode:
        return f"({lat:.1f}, {lon:.1f})"

    try:
        from geopy.geocoders import Nominatim
        geolocator = Nominatim(user_agent="human-photo-miner/1.0", timeout=10)
        location = geolocator.reverse(f"{lat}, {lon}", language="en", exactly_one=True)
        if location:
            addr = location.raw.get("address", {})
            city = addr.get("city") or addr.get("town") or addr.get("village") or addr.get("county", "")
            state = addr.get("state", "")
            country = addr.get("country", "")

            if country in ("United States", "United States of America"):
                state_abbr = addr.get("ISO3166-2-lvl4", "").replace("US-", "")
                name = f"{city}, {state_abbr}" if state_abbr else f"{city}, {state}"
            elif country:
                name = f"{city}, {country}" if city else country
            else:
                name = f"({lat:.1f}, {lon:.1f})"

            cache[key] = name
            time.sleep(1.1)
            return name
    except Exception as e:
        print(f"    Geocode failed for ({lat:.1f}, {lon:.1f}): {e}", file=sys.stderr)

    name = f"({lat:.1f}, {lon:.1f})"
    cache[key] = name
    return name


def load_photos(db_path: Path) -> list[dict]:
    conn = sqlite3.connect(str(db_path))
    conn.row_factory = sqlite3.Row
    rows = conn.execute("""
        SELECT
            ZDATECREATED as date_raw,
            ZLATITUDE as lat,
            ZLONGITUDE as lon,
            ZKIND as kind
        FROM ZASSET
        WHERE ZTRASHEDSTATE = 0
          AND ZLATITUDE != 0 AND ZLONGITUDE != 0
          AND ZLATITUDE > -90 AND ZLATITUDE < 90
          AND ZLONGITUDE > -180 AND ZLONGITUDE < 180
          AND ZLATITUDE != -180 AND ZLONGITUDE != -180
        ORDER BY ZDATECREATED ASC
    """).fetchall()
    conn.close()

    photos = []
    for r in rows:
        ts = r["date_raw"] + APPLE_EPOCH_OFFSET
        dt = datetime.utcfromtimestamp(ts)
        if dt.year < 2000:
            continue
        photos.append({
            "date": dt.strftime("%Y-%m-%d"),
            "month": dt.strftime("%Y-%m"),
            "year": dt.year,
            "lat": round(r["lat"], 4),
            "lon": round(r["lon"], 4),
            "lat_r": round(r["lat"], 1),
            "lon_r": round(r["lon"], 1),
            "kind": r["kind"],
        })
    return photos


def build_gps_clusters(photos: list[dict]) -> list[dict]:
    clusters: dict[tuple, dict] = {}
    for p in photos:
        key = (p["lat_r"], p["lon_r"])
        if key not in clusters:
            clusters[key] = {
                "lat": p["lat_r"], "lon": p["lon_r"],
                "count": 0, "dates": [], "months": set(), "years": set(),
            }
        c = clusters[key]
        c["count"] += 1
        c["dates"].append(p["date"])
        c["months"].add(p["month"])
        c["years"].add(p["year"])

    result = []
    for key, c in clusters.items():
        c["first_date"] = min(c["dates"])
        c["last_date"] = max(c["dates"])
        c["first_month"] = min(c["months"])
        c["last_month"] = max(c["months"])
        c["year_span"] = sorted(c["years"])
        c["months"] = sorted(c["months"])
        del c["dates"]
        del c["years"]
        result.append(c)

    result.sort(key=lambda x: x["count"], reverse=True)
    return result


def detect_trips(photos: list[dict], geocode_cache: dict, do_geocode: bool) -> list[dict]:
    """Detect trips: clusters of photos in non-home locations within short timeframes."""
    trips = []
    current_trip = None

    for p in photos:
        home = is_home_location(p["lat"], p["lon"], p["date"])
        if home:
            if current_trip and current_trip["photo_count"] >= 3:
                trips.append(current_trip)
            current_trip = None
            continue

        place = reverse_geocode_cluster(p["lat_r"], p["lon_r"], geocode_cache, do_geocode)

        if current_trip is None:
            current_trip = {
                "place": place,
                "start_date": p["date"],
                "end_date": p["date"],
                "photo_count": 1,
                "locations": {place},
            }
        else:
            days_since = (datetime.strptime(p["date"], "%Y-%m-%d") -
                          datetime.strptime(current_trip["end_date"], "%Y-%m-%d")).days

            if days_since <= 3:
                current_trip["end_date"] = p["date"]
                current_trip["photo_count"] += 1
                current_trip["locations"].add(place)
            else:
                if current_trip["photo_count"] >= 3:
                    trips.append(current_trip)
                current_trip = {
                    "place": place,
                    "start_date": p["date"],
                    "end_date": p["date"],
                    "photo_count": 1,
                    "locations": {place},
                }

    if current_trip and current_trip["photo_count"] >= 3:
        trips.append(current_trip)

    for trip in trips:
        start = datetime.strptime(trip["start_date"], "%Y-%m-%d")
        end = datetime.strptime(trip["end_date"], "%Y-%m-%d")
        trip["duration_days"] = (end - start).days + 1
        locs = sorted(trip["locations"])
        trip["place"] = locs[0] if len(locs) == 1 else f"{locs[0]} + {len(locs) - 1} more"
        trip["all_locations"] = locs
        del trip["locations"]

    trips.sort(key=lambda x: x["start_date"], reverse=True)
    return trips


def build_recent_activity(photos: list[dict], days: int, geocode_cache: dict, do_geocode: bool) -> dict:
    cutoff = (datetime.now() - timedelta(days=days)).strftime("%Y-%m-%d")
    recent = [p for p in photos if p["date"] >= cutoff]

    if not recent:
        return {"window_days": days, "photo_count": 0, "locations": [], "days_active": []}

    location_counts: dict[str, int] = defaultdict(int)
    days_active = set()
    for p in recent:
        place = reverse_geocode_cluster(p["lat_r"], p["lon_r"], geocode_cache, do_geocode)
        location_counts[place] += 1
        days_active.add(p["date"])

    locations = [
        {"place": place, "photo_count": cnt}
        for place, cnt in sorted(location_counts.items(), key=lambda x: -x[1])
    ]

    return {
        "window_days": days,
        "cutoff_date": cutoff,
        "photo_count": len(recent),
        "days_active": sorted(days_active),
        "locations": locations,
    }


def build_monthly_summaries(photos: list[dict], geocode_cache: dict, do_geocode: bool) -> list[dict]:
    by_month: dict[str, list] = defaultdict(list)
    for p in photos:
        by_month[p["month"]].append(p)

    summaries = []
    for month in sorted(by_month.keys(), reverse=True)[:24]:
        month_photos = by_month[month]
        home_count = 0
        travel_count = 0
        locations: dict[str, int] = defaultdict(int)

        for p in month_photos:
            home = is_home_location(p["lat"], p["lon"], p["date"])
            if home:
                home_count += 1
                locations[home] += 1
            else:
                travel_count += 1
                place = reverse_geocode_cluster(p["lat_r"], p["lon_r"], geocode_cache, do_geocode)
                locations[place] += 1

        top_locations = sorted(locations.items(), key=lambda x: -x[1])[:5]
        summaries.append({
            "month": month,
            "total_photos": len(month_photos),
            "home_photos": home_count,
            "travel_photos": travel_count,
            "travel_ratio": round(travel_count / len(month_photos), 2) if month_photos else 0,
            "top_locations": [{"place": p, "count": c} for p, c in top_locations],
        })

    return summaries


def main():
    parser = argparse.ArgumentParser(description="Mine Apple Photos metadata for persona enrichment")
    parser.add_argument("--days", type=int, default=30, help="Recent activity window in days (default: 30)")
    parser.add_argument("--no-geocode", action="store_true", help="Skip live reverse geocoding, use cache/known places only")
    parser.add_argument("--db", default=str(PHOTOS_DB), help="Path to Photos.sqlite")
    args = parser.parse_args()

    db_path = Path(args.db)
    if not db_path.exists():
        print(f"ERROR: Photos database not found at {db_path}", file=sys.stderr)
        sys.exit(1)

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    print(f"\n{'='*60}")
    print(f"  Apple Photos Mining")
    print(f"{'='*60}")
    print(f"  Database:  {db_path}")
    print(f"  Output:    {OUTPUT_DIR}")
    print(f"  Geocode:   {'cached/known only' if args.no_geocode else 'live + cached'}")
    print(f"  Recent:    last {args.days} days")
    print(f"{'='*60}\n")

    do_geocode = not args.no_geocode
    geocode_cache = load_geocode_cache()

    print("  Loading photos...", end=" ", flush=True)
    photos = load_photos(db_path)
    print(f"{len(photos)} photos with valid GPS")

    print("  Building GPS clusters...", end=" ", flush=True)
    clusters = build_gps_clusters(photos)
    print(f"{len(clusters)} location clusters")

    print("  Reverse geocoding clusters...")
    for c in clusters:
        if c["count"] >= 5:
            name = reverse_geocode_cluster(c["lat"], c["lon"], geocode_cache, do_geocode)
            c["place"] = name
        else:
            c["place"] = f"({c['lat']:.1f}, {c['lon']:.1f})"
    save_geocode_cache(geocode_cache)
    print(f"    Geocoded {sum(1 for c in clusters if not c['place'].startswith('('))} clusters")

    print("  Detecting trips...", end=" ", flush=True)
    trips = detect_trips(photos, geocode_cache, do_geocode)
    save_geocode_cache(geocode_cache)
    print(f"{len(trips)} trips detected")

    print("  Building recent activity...", end=" ", flush=True)
    recent = build_recent_activity(photos, args.days, geocode_cache, do_geocode)
    save_geocode_cache(geocode_cache)
    print(f"{recent['photo_count']} photos in last {args.days} days")

    print("  Building monthly summaries...", end=" ", flush=True)
    monthly = build_monthly_summaries(photos, geocode_cache, do_geocode)
    save_geocode_cache(geocode_cache)
    print(f"{len(monthly)} months")

    significant_clusters = [c for c in clusters if c["count"] >= 10]
    timeline = {
        "generated": datetime.now().isoformat(),
        "total_photos": len(photos),
        "date_range": {
            "first": photos[0]["date"] if photos else None,
            "last": photos[-1]["date"] if photos else None,
        },
        "home_locations": HOME_LOCATIONS,
        "location_clusters": significant_clusters,
        "trips": trips[:100],
        "monthly_summaries": monthly,
    }

    timeline_path = OUTPUT_DIR / "life_timeline.json"
    with open(timeline_path, "w") as f:
        json.dump(timeline, f, indent=2, default=str)
    print(f"\n  Written: {timeline_path}")

    recent_path = OUTPUT_DIR / "recent_activity.json"
    with open(recent_path, "w") as f:
        json.dump(recent, f, indent=2, default=str)
    print(f"  Written: {recent_path}")

    print(f"\n{'='*60}")
    print(f"  Summary")
    print(f"{'='*60}")
    print(f"  Photos:     {len(photos)}")
    print(f"  Clusters:   {len(significant_clusters)} significant (10+ photos)")
    print(f"  Trips:      {len(trips)}")
    print(f"  Recent:     {recent['photo_count']} photos, {len(recent.get('days_active', []))} active days")

    print(f"\n  Top 15 locations:")
    for c in significant_clusters[:15]:
        years = f"{c['first_date'][:4]}-{c['last_date'][:4]}"
        print(f"    {c['place']:35s} {c['count']:5d} photos  ({years})")

    print(f"\n  Recent trips:")
    for t in trips[:10]:
        dur = f"{t['duration_days']}d" if t["duration_days"] > 1 else "day trip"
        print(f"    {t['start_date']}  {t['place']:35s} {t['photo_count']:3d} photos  ({dur})")

    print(f"{'='*60}\n")


if __name__ == "__main__":
    main()
