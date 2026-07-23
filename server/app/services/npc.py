from pathlib import Path

import yaml

_NPCS_PATH = Path(__file__).resolve().parents[1] / "npcs.yaml"


class UnknownNpc(Exception):
    """존재하지 않는 npc_id."""


# 서버 시작 시 한 번 읽는다. 수정하려면 서버를 재시작한다.
_npcs: dict[str, dict[str, str]] = (
    yaml.safe_load(_NPCS_PATH.read_text(encoding="utf-8")) or {}
)


def get_personality(npc_id: str) -> str:
    """NPC의 system 프롬프트를 반환. 없는 id면 UnknownNpc."""
    try:
        return _npcs[npc_id]["personality"]
    except KeyError:
        raise UnknownNpc(npc_id)
