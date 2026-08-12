"""Validate entity-control requests received from a Seeed HA device."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Collection


MAX_COMMAND_ENTITIES = 20

_ALLOWED_DOMAINS = {
    "toggle": frozenset({"cover", "light", "media_player", "switch"}),
    "turn_off": frozenset({"light", "media_player", "switch"}),
}


@dataclass(frozen=True)
class EntityCommand:
    """A validated action targeting subscribed Home Assistant entities."""

    request_id: int
    action: str
    entity_ids: tuple[str, ...]


class EntityCommandError(ValueError):
    """Describe a rejected entity-control request with a stable error code."""

    def __init__(self, code: str) -> None:
        super().__init__(code)
        self.code = code


def parse_entity_command(
    payload: dict[str, Any], subscribed_entities: Collection[str]
) -> EntityCommand:
    """Validate one device request against its configured subscription list."""

    request_id = payload.get("request_id")
    if type(request_id) is not int or request_id <= 0:
        raise EntityCommandError("invalid_request_id")

    action = payload.get("action")
    if not isinstance(action, str) or action not in _ALLOWED_DOMAINS:
        raise EntityCommandError("invalid_action")

    raw_entity_ids = payload.get("entity_ids")
    if not isinstance(raw_entity_ids, list) or not raw_entity_ids:
        raise EntityCommandError("invalid_entity_ids")
    if len(raw_entity_ids) > MAX_COMMAND_ENTITIES:
        raise EntityCommandError("too_many_entities")

    selected = set(subscribed_entities)
    allowed_domains = _ALLOWED_DOMAINS[action]
    entity_ids: list[str] = []
    seen: set[str] = set()

    for entity_id in raw_entity_ids:
        if not isinstance(entity_id, str) or entity_id.count(".") != 1:
            raise EntityCommandError("invalid_entity_id")
        domain, object_id = entity_id.split(".", 1)
        if not domain or not object_id:
            raise EntityCommandError("invalid_entity_id")
        if entity_id not in selected:
            raise EntityCommandError("entity_not_subscribed")
        if domain not in allowed_domains:
            raise EntityCommandError("unsupported_entity_domain")
        if entity_id not in seen:
            seen.add(entity_id)
            entity_ids.append(entity_id)

    if action == "toggle" and len(entity_ids) != 1:
        raise EntityCommandError("toggle_requires_one_entity")

    return EntityCommand(request_id, action, tuple(entity_ids))
