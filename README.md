XTF-8 Codec
===========

The **XTF-8** codec encodes a hybrid string that may consists of UTF-8
sequences and binary bytes to a valid UTF-8 string, which can then be
used in places that require UTF-8, e.g., JSON.

The encoded UTF-8 string can be decoded to the original data, provided
that there was no collision (although very unlikely) in the encoding
process.


Algorithm
---------
The **XTF-8** codec uses the Unicode range U+EF80..U+EFFF to encode
any invalid UTF-8 sequence (assume to be binary bytes) in the input
hybrid string.  The chosen encoding area is located in the Unicode
Private User Area (PUA), and has been registered with
[ConScript Unicode Registry](https://www.evertype.com/standards/csur/conscript-table.html)
by MirBSD for their [OPTU encoding](http://www.mirbsd.org/htman/i386/man3/optu8to16.htm).

The encoding procedure is as follows:

1. Decode the input string according to UTF-8.
2. For every valid UTF-8 sequence:
   * If the Unicode code point is *inside* the XTF8 encoding area:
     - If error handler is "replace" (the default), the conflicting
       code point is replaced with U+FFFD (Replacement Character),
       and output the UTF-8 encoded sequence of U+FFFD;
     - Otherwise, raise an error and abort the process.
   * Otherwise, copy the sequence to the output;
3. When come across an invalid UTF-8 sequence:
   1. Assume the bytes following the previous valid UTF-8 sequence
      to the current byte are binary data;
   2. Assert each byte is non-ASCII (i.e., 0x80..0xFF);
   3. Encode each byte to a Unicode code point within the XTF8
      encoding area, i.e., `codepoint = 0xEF80 | (byte & 0x7F)`;
   4. Output the UTF-8 encoded sequence of the encoded code point.
4. Assert the encoded output is a valid UTF-8 string.

The decoding procedure is as follows:

1. Decode the input string according to UTF-8.
2. For every valid UTF-8 sequence:
   * If the Unicode code point is *inside* the XTF8 encoding area,
     decode the code point to a non-ASCII byte, i.e.,
     `byte = (codepoint & 0x7F) | 0x80`;
   * Otherwise, copy the sequence to the output;
3. When come across an invalid UTF-8 sequence:
   * If error handler is "replace" (the default), the invalid sequence
     since the previous valid UTF-8 sequence is replaced with U+FFFD
     (Replacement Character), and output the UTF-8 encoded sequence
     of U+FFFD;
   * Otherwise, raise an error and abort the process.

**Security**

- MUST NOT *ignore* the invalid bytes in decoding the input; abortion
  or replacement should be used instead.
- MUST NOT decode XTF8-encoded code points to *ASCII characters*
  (i.e., 0x00..0x7F), avoiding character smuggling.


Rationale
---------
[Python PEP 383](https://peps.python.org/pep-0383/) presents another
method to losslessly encode strings of mixed text and binary bytes.
Every non-decodable byte (0x80..0xFF) will be encoded as lone surrogate
code of Unicode range U+DC80..U+DCFF.  However, this encoding area
is *disallowed* in UTF-8 and thus the encoded results would still be
*invalid* UTF-8 strings.

The MirBSD Project proposed the
[OPTU encoding](http://www.mirbsd.org/htman/i386/man3/optu8to16.htm)
to tackle the similar issue.  It uses a block of PUA to encode the
non-decodable bytes and thus makes the encoded results valid UTF-8
strings.  This would probably cause the fewest security and
interoperability problems.  There is, however, some possibility of
collision with other uses of the same PUA characters.

In reality, the **XTF-8** codec implements the OPTU encoding.


License
-------
[The MIT License](https://opensource.org/licenses/MIT)

This software makes use of the fast
[DFA-based UTF-8 decoder](http://bjoern.hoehrmann.de/utf-8/decoder/dfa/),
developed by Björn Höhrmann and released under the MIT License.


References
----------
* [Unicode Technical Report #36 (Unicode Security Considerations): 3.7. Enabling Lossless Conversion](https://www.unicode.org/reports/tr36/#EnablingLosslessConversion)
* [Python: PEP 383: Non-decodable Bytes in System Character Interfaces](https://peps.python.org/pep-0383/)
* [MirBSD: OPTU encoding](http://www.mirbsd.org/htman/i386/man3/optu8to16.htm)
