"""
Python Arkime Session Module

The Python Arkime Session module has methods for dealing with sessions. The API is very unpythonic and treats the session as a opaque object that needs to be passed around.
"""

from typing import Any

from .types import ArkimeSession, ParserCb

# === Methods ===
def add_int(session: ArkimeSession, fieldPosOrExp: int | str, value: int) -> bool:
    """
    Add an integer value to a session field.

    Args:
        session: The session object from the classifyCb or parserCb.
        fieldPosOrExp: The field position returned by field_define/field_get or the field expression.
        value: The integer value to add to the session field.

    Returns:
        bool: True if the value was added, False if it was a duplicate or field doesn't exist.
    """
    ...
def add_protocol(session: ArkimeSession, protocol: str) -> None:
    """
    Optimized version of add_string(session, 'protocol', protocol).

    Args:
        session: The session object from the classifyCb or parserCb.
        protocol: The protocol string to add to the session.
    """
    ...
def add_string(session: ArkimeSession, fieldPosOrExp: int | str, value: str) -> bool:
    """
    Add a string value to a session field.

    Args:
        session: The session object from the classifyCb or parserCb.
        fieldPosOrExp: The field position returned by field_define/field_get or the field expression.
        value: The string value to add to the session field.

    Returns:
        bool: True if the value was added, False if it was a duplicate or field doesn't exist.
    """
    ...
def add_tag(session: ArkimeSession, tag: str) -> None:
    """
    Optimized version of add_string(session, 'tags', tag).

    Args:
        session: The session object from the classifyCb or parserCb.
        tag: The tag string to add to the session.
    """
    ...

def decref(session: ArkimeSession) -> None:
    """
    Decrement the reference count of a session. Call after incref when done with async operations.
    The session may be freed when the reference count reaches zero.

    Args:
        session: The session object previously passed to incref.
    """
    ...
def incref(session: ArkimeSession) -> None:
    """
    Increment the reference count of a session. Use when storing a session handle for later use
    outside of the callback (e.g., async operations). Must call decref when done.

    Args:
        session: The session object from the classifyCb or parserCb.
    """
    ...

def get(session: ArkimeSession, fieldPosOrExp: int | str) -> Any | list[Any] | None:
    """
    Retrieve the value of a session field.

    Args:
        session: The session object from the classifyCb or parserCb.
        fieldPosOrExp: The field position returned by field_define/field_get or the field expression.

    Returns:
        The field value. Returns a list for multi-value fields, a single value for single-value
        fields, or None if the field is not set.
    """
    ...

def get_attr(session: ArkimeSession, key: str) -> Any | None:
    """
    Retrieve a Python object previously associated with the session via set_attr.

    Args:
        session: The session object from the classifyCb or parserCb.
        key: The attribute key used in set_attr.

    Returns:
        The stored Python object, or None if the key does not exist.
    """
    ...

def has_protocol(session: ArkimeSession, protocol: str) -> bool:
    """
    Check if a protocol has been added to the session.

    Args:
        session: The session object from the classifyCb or parserCb.
        protocol: The protocol string to check for.

    Returns:
        bool: True if the protocol is present, False otherwise.
    """
    ...

def register_parser(session: ArkimeSession, parserCb: ParserCb) -> None:
    """
    Register a parser callback for every packet of the session.

    Args:
        session: The session object from the classifyCb or parserCb.
        parserCb: The callback to call for every packet of the session in each direction.
    """
    ...

def set_attr(session: ArkimeSession, key: str, value: Any) -> None:
    """
    Associate a Python object with the session.
    This is useful for storing state between calls to the parserCb.
    The key is global across all Python modules, so use a unique key to avoid collisions.

    Args:
        session: The session object from the classifyCb or parserCb.
        key: The attribute key.
        value: The Python object to store.
    """
    ...