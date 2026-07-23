import pytest

from app.services import npc


def test_get_personality_returns_text():
    text = npc.get_personality("merchant")
    assert "상인" in text


def test_get_personality_unknown_id_raises():
    with pytest.raises(npc.UnknownNpc):
        npc.get_personality("no-such-npc")
