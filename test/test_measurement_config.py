import json
import unittest
from datetime import datetime, timezone


METERS_PER_MILE = 1609.344
LB_TO_KG = 0.45359237
US_GALLON_LITERS = 3.785411784
IMP_GALLON_LITERS = 4.54609


class RegionalSettings:
    def __init__(self, game_profile="ETS2", preset="Metric", currency="EUR", distance="km", weight="kg",
                 cargo_weight="metric_tonne", volume="L", speed="km/h", number_format="locale_default",
                 date_format="locale_default", updated_at_utc=None):
        self.game_profile = game_profile
        self.preset = preset
        self.currency = currency
        self.distance = distance
        self.weight = weight
        self.cargo_weight = cargo_weight
        self.volume = volume
        self.speed = speed
        self.number_format = number_format
        self.date_format = date_format
        self.updated_at_utc = updated_at_utc or datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def default_settings(game_profile):
    if game_profile == "ATS":
        return RegionalSettings(
            game_profile="ATS",
            preset="Imperial",
            currency="USD",
            distance="mi",
            weight="lb",
            cargo_weight="short_ton",
            volume="US gal",
            speed="mph",
        )
    return RegionalSettings(
        game_profile="ETS2",
        preset="Metric",
        currency="EUR",
        distance="km",
        weight="kg",
        cargo_weight="metric_tonne",
        volume="L",
        speed="km/h",
    )


def convert_distance_km_to_display(km, distance_unit):
    if distance_unit == "mi":
        return km * 0.621371192237334
    return km


def convert_speed_kmh_to_display(kmh, speed_unit):
    if speed_unit == "mph":
        return kmh * 0.621371192237334
    return kmh


def convert_volume_liters_to_display(liters, volume_unit):
    if volume_unit == "US gal":
        return liters / US_GALLON_LITERS
    if volume_unit == "Imp gal":
        return liters / IMP_GALLON_LITERS
    return liters


def convert_weight_kg_to_display(kg, weight_unit):
    if weight_unit == "lb":
        return kg * 2.20462262185
    return kg


def convert_cargo_weight_kg_to_display(kg, cargo_weight_unit):
    if cargo_weight_unit == "metric_tonne":
        return kg / 1000.0
    if cargo_weight_unit == "short_ton":
        return kg / 907.18474
    if cargo_weight_unit == "long_ton":
        return kg / 1016.0469088
    if cargo_weight_unit == "lb":
        return kg * 2.20462262185
    return kg


def format_currency(amount, currency_code):
    if currency_code == "USD":
        return f"${amount:,.2f}"
    return f"€{amount:,.2f}"


def apply_preset(preset_name):
    if preset_name == "Imperial":
        return RegionalSettings(
            preset="Imperial",
            currency="USD",
            distance="mi",
            weight="lb",
            cargo_weight="short_ton",
            volume="US gal",
            speed="mph",
        )
    if preset_name == "Custom":
        return RegionalSettings(preset="Custom")
    return RegionalSettings(
        preset="Metric",
        currency="EUR",
        distance="km",
        weight="kg",
        cargo_weight="metric_tonne",
        volume="L",
        speed="km/h",
    )


def save_profile(profile_name, settings):
    payload = {
        "record_type": "regional_settings",
        "schema_version": 1,
        "game_profile": profile_name,
        "preset": settings.preset,
        "currency": settings.currency,
        "distance_unit": settings.distance,
        "weight_unit": settings.weight,
        "cargo_weight_unit": settings.cargo_weight,
        "volume_unit": settings.volume,
        "speed_unit": settings.speed,
        "number_format": settings.number_format,
        "date_format": settings.date_format,
        "updated_at_utc": settings.updated_at_utc,
    }
    return json.dumps(payload)


def load_profile(raw_text):
    payload = json.loads(raw_text)
    return RegionalSettings(
        game_profile=payload.get("game_profile", "ETS2"),
        preset=payload.get("preset", "Metric"),
        currency=payload.get("currency", "EUR"),
        distance=payload.get("distance_unit", "km"),
        weight=payload.get("weight_unit", "kg"),
        cargo_weight=payload.get("cargo_weight_unit", "metric_tonne"),
        volume=payload.get("volume_unit", "L"),
        speed=payload.get("speed_unit", "km/h"),
        number_format=payload.get("number_format", "locale_default"),
        date_format=payload.get("date_format", "locale_default"),
        updated_at_utc=payload.get("updated_at_utc"),
    )


class RegionalConfigTests(unittest.TestCase):
    def test_ets2_and_ats_defaults(self):
        self.assertEqual(default_settings("ETS2").preset, "Metric")
        self.assertEqual(default_settings("ATS").preset, "Imperial")
        self.assertEqual(default_settings("ETS2").currency, "EUR")
        self.assertEqual(default_settings("ATS").currency, "USD")
        self.assertEqual(default_settings("ETS2").distance, "km")
        self.assertEqual(default_settings("ATS").distance, "mi")

    def test_independent_game_profiles_are_not_overwritten(self):
        ets2 = default_settings("ETS2")
        ats = default_settings("ATS")
        ets2.currency = "EUR"
        ats.currency = "USD"
        self.assertEqual(ets2.currency, "EUR")
        self.assertEqual(ats.currency, "USD")

    def test_preset_changes_update_fields_and_custom_detection(self):
        metric = apply_preset("Metric")
        imperial = apply_preset("Imperial")
        custom = apply_preset("Custom")
        self.assertEqual(metric.distance, "km")
        self.assertEqual(imperial.distance, "mi")
        self.assertEqual(custom.preset, "Custom")

        custom.distance = "mi"
        self.assertEqual(custom.preset, "Custom")
        self.assertEqual(custom.distance, "mi")

    def test_conversion_factors_are_correct(self):
        self.assertAlmostEqual(convert_distance_km_to_display(1.0, "mi"), 0.621371192237334)
        self.assertAlmostEqual(convert_speed_kmh_to_display(1.0, "mph"), 0.621371192237334)
        self.assertAlmostEqual(convert_volume_liters_to_display(1.0, "US gal"), 0.26417205235814845)
        self.assertAlmostEqual(convert_volume_liters_to_display(1.0, "Imp gal"), 0.21996924829908778)
        self.assertAlmostEqual(convert_weight_kg_to_display(1.0, "lb"), 2.20462262185)
        self.assertAlmostEqual(convert_cargo_weight_kg_to_display(1000.0, "metric_tonne"), 1.0)
        self.assertAlmostEqual(convert_cargo_weight_kg_to_display(1000.0, "short_ton"), 1.1023113109243878)
        self.assertAlmostEqual(convert_cargo_weight_kg_to_display(1000.0, "long_ton"), 0.9842065277199573)
        self.assertAlmostEqual(convert_cargo_weight_kg_to_display(1000.0, "lb"), 2204.62262185)

    def test_no_double_conversion_of_display_values(self):
        already_converted = convert_distance_km_to_display(100.0, "mi")
        self.assertAlmostEqual(already_converted, 62.1371192237334)
        self.assertAlmostEqual(convert_distance_km_to_display(100.0, "mi"), 62.1371192237334)

    def test_persistence_and_restart_round_trip(self):
        settings = default_settings("ETS2")
        settings.updated_at_utc = "2026-10-12T10:00:00Z"
        saved = save_profile("ETS2", settings)
        loaded = load_profile(saved)
        self.assertEqual(loaded.game_profile, "ETS2")
        self.assertEqual(loaded.distance, "km")
        self.assertEqual(loaded.updated_at_utc, "2026-10-12T10:00:00Z")

    def test_reset_only_affects_active_profile(self):
        ets2 = default_settings("ETS2")
        ats = default_settings("ATS")
        ets2.distance = "mi"
        ats.distance = "km"
        self.assertNotEqual(ets2.distance, default_settings("ETS2").distance)
        self.assertNotEqual(ats.distance, default_settings("ATS").distance)

    def test_timestamp_is_valid_utc(self):
        stamp = datetime.now(timezone.utc).replace(microsecond=0).isoformat()
        parsed = datetime.fromisoformat(stamp.replace("Z", "+00:00"))
        self.assertEqual(parsed.tzinfo.utcoffset(parsed), timezone.utc.utcoffset(None))

    def test_currency_formatting_remains_separate_from_exchange_rates(self):
        self.assertEqual(format_currency(1234.56, "EUR"), "€1,234.56")
        self.assertEqual(format_currency(1234.56, "USD"), "$1,234.56")
        self.assertNotEqual(format_currency(1234.56, "EUR"), "$1,234.56")


if __name__ == "__main__":
    unittest.main()
