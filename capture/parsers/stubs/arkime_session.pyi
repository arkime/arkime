"""
Python Arkime Session Module

The Python Arkime Session module has methods for dealing with sessions. The API is very unpythonic and treats the session as a opaque object that needs to be passed around.
"""

from typing import Any

from .types import ArkimeSession, ArkimeParserBuf, ParserCb, ParserBufCb

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

def register_parser_buf(session: ArkimeSession, parserBufCb: ParserBufCb) -> None:
    """
    Register a buffered parser callback for every packet of the session.
    Data is automatically accumulated in a per-direction buffer (up to 8KB per direction)
    and passed as a memoryview of the accumulated data. Use parser_buf_del or parser_buf_skip
    to consume processed bytes from the buffer.

    Args:
        session: The session object from the classifyCb or parserCb.
        parserBufCb: The callback to call for every packet of the session in each direction.
                     Receives (session, pb, buf, which) where buf is accumulated data.
    """
    ...

def parser_buf_del(pb: ArkimeParserBuf, which: int, length: int) -> None:
    """
    Delete bytes from the front of the parser buffer after processing them.
    Call this after successfully parsing a complete message to remove consumed bytes.

    Args:
        pb: The parser buffer handle from the parserBufCb callback.
        which: Direction: 0 = client to server, 1 = server to client.
        length: Number of bytes to delete from the front of the buffer.
    """
    ...

def parser_buf_skip(pb: ArkimeParserBuf, which: int, skip: int) -> None:
    """
    Skip bytes in the parser buffer. If skip is less than or equal to current buffer length,
    acts like parser_buf_del. If skip is greater than current buffer length, also skips
    that many bytes from future incoming data.

    Args:
        pb: The parser buffer handle from the parserBufCb callback.
        which: Direction: 0 = client to server, 1 = server to client.
        skip: Number of bytes to skip.
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