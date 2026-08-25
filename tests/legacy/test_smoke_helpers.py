from __future__ import annotations

import sys
from pathlib import Path
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parent))

from run_smoke import PLAYER_MARKERS, evaluate_player_log, redact_log


class SmokeHelperTests(unittest.TestCase):
    def test_complete_log_has_no_missing_markers(self) -> None:
        missing, fatal = evaluate_player_log("\n".join(PLAYER_MARKERS))
        self.assertEqual([], missing)
        self.assertEqual([], fatal)

    def test_sanitizer_output_is_fatal(self) -> None:
        missing, fatal = evaluate_player_log("AddressSanitizer: heap-use-after-free")
        self.assertTrue(missing)
        self.assertEqual(["AddressSanitizer"], fatal)

    def test_private_cards_and_large_integers_are_redacted(self) -> None:
        raw = (
            "player1 | My Hand:\n"
            "player1 | Ace of Spades:King of Hearts:\n"
            "player1 | ciphertext 123456789012345678901234567890\n"
            "player1 | Player 1 hand value: 27 ||\n"
        )
        redacted = redact_log(raw)
        self.assertNotIn("Ace of Spades", redacted)
        self.assertNotIn("123456789012345678901234567890", redacted)
        self.assertNotIn("hand value: 27", redacted)
        self.assertIn("<redacted legacy private cards>", redacted)


if __name__ == "__main__":
    unittest.main()
